# Archforce（华锐）语义映射：ATP TradeAPI V4.7.3.1 / QuantAPI V2.6.3.5

研究输入（只读头文件，未链接 SDK；路径为本机绝对路径）：

- 交易 TradeAPI（老形态，Windows）：`/Users/wuxw/Workspace/archforce_tradeapi_demo.zip` 内 `archforce_tradeapi_demo/.../AtpTradeAPI_STD_win_vs2017_x64_TradeApi_V4.7.3.1_20260410`（zip 包，未解压）
- 交易 QuantAPI（Linux/gcc11）：`/Users/wuxw/Workspace/archforce_quantapi_demo/sdk/ATPQuantAPI_STD_rhel_gcc-11.2.1_x64_QuantApi_V2.6.3.5_20260608`（`sdk/current_atp` 软链接当前指向该版本；另有 `sdk/ATPQuantAPI_STD_rhel_gcc-4.8.5_x64_Institution_QuantApi_V2.7.1.2_20250619`；demo 统一副本在 `/Users/wuxw/Workspace/archforce_quantapi_demo/demo/`：`trade_app`/`uni_trade_app`/`simple_demo`/`all_message_demo`）
- 行情 AMA（C++）：`/Users/wuxw/Workspace/archforce_ama_demo/c++/`（AMA V4.5.8，`revision` 为 `V4.5.8.250331-rc1.4`；头文件 `include/ama.h`，demo 入口 `demo_src/ama_demo_tcp.cpp` / `ama_demo_fpga-tcp.cpp`，指南见 `doc/` 下三份 PDF；语义映射见 `archforce-ama.md`）

交易两包（TradeAPI/QuantAPI）都不含行情（全文无 Quote/MarketData/Snapshot），只管交易执行与账务查询。行情由 AMA 包覆盖（见上一行）。

## 1. 已验证成立的设计

| STS 假设 | ATP/QuantAPI 现实 | 结论 |
| --- | --- | --- |
| 价格数量定点整数 | TradeAPI 全 int64（价 N13(4)，量 N15(2)，金额 N18(4)） | 成立；QuantAPI 同语义但用 `double` 承载，适配器侧换算 |
| 无改单（ADR-020） | 两包均无 Replace/Modify，改单=撤单+重下 | 成立，无需返工 |
| 回报幂等键 | `exec_id` 单交易日唯一；订单维度 `cl_ord_no(+orig_cl_ord_no)` 聚合 | 成立，见第 3 节两处补丁 |
| 超时=未知（ADR-004） | `Req*` 同步返回仅=受理，业务结果全走异步回调 | 成立；适配器不得把发送成功当委托成功 |
| 重连+补单+完成界定 | 自动重连（`OnRecovering`）+ `retransmit_mode` + 补单结束事件 | 与 `recovery_complete_handler` 同构，可直接映射 |
| 市价靠方式表达 | 无单一市价值，靠 `b~g`（本方最优/对手方/五档/FOK/IOC） | 与 `MarketOrderStyle` 同构 |
| 科创市价须保护价 | 上海股票/存托凭证/基金市价必须在 `price` 填保护价 | 与 `protection_price` 必填同构，但有例外（见 3.1） |
| 查资金/持仓/未终结/成交 | `ReqFund/Share/Order/TradeOrderQuery` 全套异步查询 | 与 `TradeApi` 控制面一一对应 |

## 2. 订单/状态/账户映射

- 方向：现货 `1`买/`2`卖；两融 `B`融资买入/`S`融券卖出等。映射 `(Side, TradingBusiness) -> ATPSide`，期权 `position_effect`（O/C）以后再说。
- 市场：`101`上/`102`深/`104`北（`103`港）。无板块字段，板靠 `market_id + security_id` 隐含，适配器侧派生 `AshareBoard`。
- 状态对照：`kNew/kSended/kUnSend -> PendingNew`；`kPartiallyFilled -> PartiallyFilled`；`kFilled -> Filled`；`kWaitCancelled/kPartiallyFilledWaitCancelled -> PendingCancel`；`kCancelled/kPartiallyCancelled/kPartiallyFilledPartiallyCancelled -> Cancelled`；`kReject/kInternalReject -> Rejected`。找不到原单的撤单回报固定 `Reject(8)`，走对账告警路径（已与 `reconcile` 语义一致）。
- 账户三元组：`cust_id`（客户号）+ `fund_account_id`（资金）+ `account_id`（股东账户），每单必填。`cust_id/branch_id/opu_id` 放适配器自有配置，不进核心领域模型。

> 落地状态（2026-10-01，M7-2c 代码交付 + Linux 环境 vendor 构建验证）：以上映射实现于
> `adapters/quantapi/quantapi_trade_adapter.cpp`（状态表/方向/市场/账户三元组/查询与快照换算）与
> `include/sts/broker/quantapi_fixtures.hpp`（值 fixture），锁表单测
> `adapters/tests/quantapi_adapter_test.cpp`（Linux 环境 `ctest 22/22` 全绿）。状态对照补全
> `kInternalCanceledOrder/kCanceledSuccess(91/92) -> Cancelled`，表外状态值 fail-closed 丢弃并计数
> （`status_unknown_drops`）。**暂定（联调核对后锁定）**：市价单 `order_type` 按 `MarketOrderStyle`
> 映射 `b/c/f/g`（本方最优/对手方最优转限价/五档全额成交剩余撤销/五档即时成交剩余转限价，
> `none -> 0` 本地拒单）；持仓 `today_quantity` 恒 0（ADR-032 今昨拆分未报告，
> `GetStockBuy/GetStockSale` 语义待联调确认）。

## 3. 必须改的四件事（M3-2 前置）

> 落地状态（2026-09-24，M3-2 完成，四项全闭环）：
>
> 1. 保护价按板区分 → `src/market/ashare.cpp`：仅 STAR/创业板市价强制保护价，主板允许 0。
> 2. `client_seq` 映射持久化 → `AlternatePaperBroker` 自维护单调 seq 并落 sidecar 文件，重启不重用（`ALT-<seq>`）；OMS 侧 `client_order_id` 不变。
> 3. `seen` 集随结转清空 → `OrderManager::settle_day()` 清空全单去重集，与组合结转同节拍调用。
> 4. 回调串行化 → `AlternatePaperBroker` 永不内联回调，一律 `drain()` FIFO 交付，submit 内重入不可能。

1. **主板市价保护价语义冲突（真 bug）**：`AshareOrderValidator` 现对所有市价单要求 `protection_price > 0`，但主板市价单按规范填 0，只有沪科创等要求保护价。校验器必须按交易所/板块区分。
2. **`cl_ord_no` 是券商生成的**：客户端幂等键只能是 `client_seq_id`（TradeAPI，要求全局唯一、回报原样带回）/ `request_id`（QuantAPI，API 层参数）/`batch_cl_ord_no`。适配器必须自维护 `client_order_id -> client_seq_id` 映射并持久化（重连重放用），OMS 侧 `client_order_id` 不变。
3. **`exec_id` 只在单交易日内唯一**：OMS 的 `seen_execution_ids` 永不清零，多日运行既膨胀又有理论碰撞。随 `settle_day` 清空（与组合结转同节拍）。
4. **回调线程串行化**：两包回报都在专用接收线程触发，`Req*` 并发安全性未承诺。适配器必须经队列/strand 串行喂给 OMS，不得在回调内阻塞或重入；`GetClientFeatureCode` 的 `thread_local` 缓冲用完即拷。

## 4. 暂不动

- QuantAPI 的 OO 消息类（Set/Get 虚函数 + `NewMessageFast` 零拷贝）是封装形态差异，语义同源，适配时再包一层。
- `order_way` 委托方式、透传 `user_info/extra_data`：适配器填常量，不管。
- 数量 2 位小数：股票按股为整数，基金/债券小数以后再说（整数假设写死股票场景）。
- `transact_time` 毫秒且分源语义（ATP 时间 vs 交易所时间），只做展示/对账；排序仍靠序号（QuantAPI 查询分页 `index`，主推靠 `exec_id` 去重 + 状态机幂等）。
- 成交经查询补全（QuantAPI 无独立成交推送）：恢复流程 = 未终结查询 + 成交查询 + 快照，三源对齐，端口已够（`request_open_orders` + snapshot，成交查询在对账驱动侧按需加）。
