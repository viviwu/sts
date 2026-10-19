# KDVIP（金证极速柜台）语义映射：maCliApi（研究段，未接线）

状态（2026-10-01）：零 infra 研究段（维护者选定时序，见 `dev_scheme.md` §5/§7）。只读头文件与 CSP 参考实现，未链接 SDK、无适配器代码；真实接线待 M7-2 收口 + SDK/账号/环境就绪后按 ADR-033 立项 `adapters/kdvip/`。整体移植 CSP/channel 的评估结论见文末（抄逻辑，不抄架构）。

研究输入（只读；路径为本机绝对路径）：

- SDK：`/Users/wuxw/Workspace/csp/code/common/3rd/kdvip_cli_sdk/` —— 活跃版本 `maCliApi/`（`Jamfile.v2:6` `LIB_ROOT`；全仓无 `Patch*` 引用）。三个 Patch（3.7-R1/3.9_upd001/4.1_upd002）头文件与基线抽查**逐行一致**，差异仅在 `lib/Lnx64/*.so`（未做二进制比对）；修订表止于 `2022-11-10 3.9`。⚠️ 全部 SDK 头为 **GBK 编码**，语义由 ASCII 标识符 + CSP 侧 UTF-8 实现反推，均可复核。
- 参考实现：`/Users/wuxw/Workspace/csp/code/csp/channel/src/KDVIP/`（转换全在头文件 `operator` 里，无 `.cpp`）+ `unittest/test_kdvip_trade.cpp`。
- 先例：`archforce.md`（QuantAPI/TradeAPI）、`archforce-ama.md`（AMA 行情）。

## 1. 已验证成立的设计

| STS 假设 | KDVIP 现实 | 结论 |
| --- | --- | --- |
| 价格数量定点整数 | 价格是**十进制字符串**（`szOrderPrice[22]`，无 ×10000；×10000 只在行情侧 `CastPrice`），量 `LONGLONG` 原样，金额全字符串 | 成立；换算点=字符串↔int64（与 QuantAPI 的 double 换算不同，另写） |
| 无改单（ADR-020） | 无 Replace/Modify API，撤单=独立 `ReqCancelOrder` | 成立 |
| 回报幂等键 | 成交编号 `szMatchedSn`；订单维=券商合同号 `szOrderId[10+1]`。CSP 合成 `execution_id = szOrderId + ":" + szMatchedSn`（`kdvip_trade_struct.h:405`） | 成立；`execution_id` 合成规则可直接同构 |
| 超时=未知（ADR-004） | `Req*` 异步，非 0=发送失败（`GetLastErrorText()`），业务结果全走 `OnRsp*`/`OnRtn*` | 成立 |
| 重连+补单+完成界定 | **传输层**自动重连（ini `reconn=true`，1 次/100ms）但**业务重登是应用责任**；无 `OnRecovering` 等价物、无恢复完成事件 | 差异：recovery_complete 由"查询全部 `is_last`"合成（与 QuantAPI `request_open_orders` 同构）；重连策略不照搬 CSP（见 §5 反例） |
| 市价靠方式表达 | `iStkBizAction`：限价 `100`、五档转限价 `120`、五档即撤 `121`、全额撤销 `122`、本方最优 `123`、对手方 `124`、即时成交剩余撤销 `125`（`kdvip_util.h` `CastStkBizAuction`） | 与 `MarketOrderStyle` 同构，映射见 §3 |
| 查资金/持仓/未终结/成交 | `ReqQryExpendableFund(10303001)` / `ExpendableShares(10303002)` / `CurrDayOrder(10303003)` / `CurrDayFill(10303004)` / `StkholderAcct(10303005)` | 与 `TradeApi` 控制面一一对应 |
| 每请求恰一次回调 | `CFirstSetField.iMsgCode` + `szMsgText`，`p_pRspField` **可能为 NULL**（`if(iMsgCode \|\| !p_pRspField)` 即失败） | 成立；错误首行即失败判定 |

## 2. 会话与线程模型

- **生命周期**：`Init()`/`Exit()`（`maCliTradeApi.h:437/:440`）——**无独立 logout**，关闭=`Exit()`；CSP 注释"Exit 调用后 Init 会失败"（`kdvip_channel_manager.cpp:1666/1674`）。注册顺序：`RegisterServer(ip,port,timeout[,pktVer,ssl])` → `RegisterSpi` → `Init` → `RegisterAcct(channel_flag)` → `ReqUserLogin(10301105)`；**返回值非 0=失败**。
- **登录**：`szAcctType="Z"`、`szAcctId`=账号、`szAuthData`=密码、`szEncryptKey/chUseScope/chAuthType` 来自配置；10s 超时（`kCHKdvipTimeout`）。登录成功后 **CSP 先 `ReqQryStkholderAcct` 确定各市场主股东号再宣告登录完成**（`kdvip_channel_manager.cpp:418-421`）——STS 适配器须复刻这一步。
- **委托不带密码**（口令在登录会话凭证），`chSecurityLevel='1'` + `szSecurityInfo` 校验会话；终端留痕 `szClientInfo`（CSP=user_finger_print）。
- **心跳**：`ReqHeartBeat(00103003)` 每 `heart_beat_invl`（默认 1s），断阈 `session_broken_threshold=3×`；两者任一为 0 即不发。超时 → `OnTradeDisconnected`。另：`OnRtnSystemInfo.chTrdStatus` 可**停止下单**（柜台级 kill switch，与 §4 故障语义同向），`OnRtnTradeRisk` 为风控提示回报。
- **回调线程**：SDK 头无 @thread 注释；ini `SyncMode=0`（默认单线程回调）。回调入参**仅回调期间有效** —— CSP 全部 `new ...(*p_pRspField)` 深拷贝进 `shared_ptr` 再 post；STS 值 fixture 拷贝与该纪律同构。
- **对象模型**：`#pragma pack(1)` POD，调用方分配并持有，`Req*` 只取指针，**无 NewMessage/DeleteMessage**（与 QuantAPI 不同）；请求结构生命周期须覆盖到发送完成（CSP 存进 `kdvip_req_map_`）。
- **关联**：`p_llRequestId` 调用方单调自增（CSP `++requset_id_seed_`），回包原样带回 → 与 QuantAPI `request_id` 归因同构；心跳请求号不入表。

## 3. 订单/状态/账户映射

- **方向**：`iStkBiz` `100` 买 / `101` 卖（`CastStkBiz`）。CSP 非法值直接下发 `-1` —— STS 反例：形状校验 fail-closed 拒单。
- **市场**：⚠️ **无 QuantAPI 型 `101/102/104` 市场码**（那是 QuantAPI 的；KDVIP 里 100/101 是方向码）。用 `chStkex '0'`深/`'1'`沪/`'3'`北 + 板块字典 `szStkbd`：`"00"`深A/`"01"`深B/`"10"`沪A/`"11"`沪B（`CastSTKBD`）。STS `"600000.SSE"` 解析后映射 `szStkbd`。**字典文件（STKBD/STK_BIZ/STK_BIZ_ACTION）未随 SDK**，值域以 CSP 实测映射为准，联调核对。
- **状态对照**（`chOrderStatus` char，SDK 无常量头，值域出自 `kdvip_util.h` `CastOrderStatus`，联调核对）：

  | KDVIP | 语义 | STS |
  | --- | --- | --- |
  | `'0'`/`'1'`/`'A'`/`'B'` | 未报/待报 | `pending_new` |
  | `'2'` | 已报 | `pending_new`（对齐 QuantAPI `kSended→pending_new` 决策；联调核对是否该进 `live`） |
  | `'3'`/`'4'` | 待撤/部分待撤 | `pending_cancel` |
  | `'5'`/`'6'` | 部分撤/已撤 | `cancelled` |
  | `'7'` | 部分成交 | `partially_filled` |
  | `'8'` | 全部成交 | `filled` |
  | `'9'` | 废单 | `rejected` |
  | 其他 | — | fail-closed 丢弃 + 计数（同 `status_unknown_drops`） |

  终态判定（CSP `IsFinalOrderStat`）：部撤/已撤/全成/废单 —— 与 STS 三终态一致。成交类型 `CastExecutionType(chIsWithdraw, chMatchedType)`：`chMatchedType '0'/'1'`→拒单语义、`'2'`+撤标志 `T/F`→撤成/部成。
- **撤单**：只能带 `szOrderId=原合同号`（`trade_struct.h:136` 注释原话："金证极速交易系统不支持用 client_order_id 撤单"），`iOrderBsn=0`；回包 `szOrderId`原单 + `szWtOrderId`撤单合同号 + `szCancelList`（`原委托号#新委托号#内部撤标志#消息`，**委托号=-1 表示失败**）。→ STS 撤单链路与 QuantAPI 同构（归因后撤、失败→`pending_cancel`+reason）。
- **client_order_id 不透传**：`iCuacctSn` 是 **int**（CSP 用它当 client_order_id，溢出抛 `kCHOverFlow`），STS `ClientOrderId` 是字符串 —— 归因必须完全靠 `request_id` 回带 + `szOrderId` sidecar（与 QuantAPI 的 `request_id`+`cl_ord_no` 模式**同构，零架构新意**）。`szOrderText[1025]` 扩展键值 `K=V` 是潜在字符串透传点（联调探索，暂不定）。
- **账户**：`llCuacctCode` 资产账户 + `szTrdacct` 股东代码 + `szStkpbu` 席位 + `llCustCode` 客户代码，登录回包携带（`SetLoginRep`），非每单填三元组（QuantAPI 形态）；市场股东号由登录后 `ReqQryStkholderAcct` 补。
- **拒单码**：**无独立数值拒单码** —— 错误=`CFirstSetField.iMsgCode`+`szMsgText`（Windows 版 GB2312 需转码），行内拒因 `szOfferRetMsg[65]`。→ STS `reject_code` 可用 `iMsgCode`（非 0 时），文本进 `reason`，否则 0+文本（ADR-032 兼容）。
- **功能号**：下单 `10302001`、撤单 `10302004`、登录 `10301105`、资金/股份/当日委托/当日成交/股东 `10303001-5`、成交/确认推送 `00102023/00102024`（对账与抓包用）。

## 4. 查询与恢复（分页与快照）

- **分页**：请求 `chQueryFlag='0'`（0 当前取数/1 续取）+ `iQueryNum=500` + `szQueryPos[33]` 游标；回包 `szQryPos` 下一页游标。行回调 `(CFirstSetField*, Rsp*, llRequestId, p_iFieldNum, p_iFieldIndex)`：**`fieldNum == fieldIndex` 即本页最后一行**；整查询结束 = `is_last && field_index != 500`（页满用 `szQryPos` 翻页）。CSP `HandleKdvipQueryRep` 的四分支驱动（单结果/错误行回放/页满翻页/结果集结束）~50 行，**可逐函数抄**（改 STS 类型）。
- **空结果**：CSP 有 bug —— `empty_rep` 构造了却回调了旧 `rep`（`kdvip_channel_manager.h:335-340`）→ 不抄；STS 按"`is_last` 必达 + `fund_valid_` 才交付"实现。
- **控制面映射**：
  - `request_open_orders` → `ReqQryCurrDayOrder`（开放态上报、终态跳过 —— 与 QuantAPI 查询行规则一致）；成交侧 `ReqQryCurrDayFill` 对账驱动按需。
  - `request_account_snapshot` → `ReqQryExpendableFund` + `ReqQryExpendableShares`（+ `ReqQryStkholderAcct` 补市场股东号）。
  - **资金字段**（全 `char[21+1]` 十进制串）：`szMarketValue` 资产市值、`szFundBln` 资金余额、**`szFundAvl` 可用**、**`szFundFrz` 冻结** → `total_asset/available/withdrawable/frozen`（`szFundUfz` 解冻、`szHFundAvl` 深口径 —— 逐字段语义联调核对；CSP `QueryFundRep.fund_withdraw` 恒 0 是其 bug，勿抄）。
  - **持仓字段**：`llStkPrebln` 昨日 / `llStkBln` 余额 / **`llStkAvl` 可用** / **`llStkFrz` 冻结** / `szCostPrice` 成本价 / `szMktVal` 市值 / **`llStkIntradayQty` 今可流通** → KDVIP **报告今昨拆分**（比 QuantAPI 全 —— `today_quantity` 可填，语义联调确认后补 ADR-032 注记）。
  - `instrument_id` 由 `szStkbd`+`szStkCode` 反向拼回 `"600000.SSE"` 形。
- **恢复完成**：无 vendor 事件 → 由"全部查询 `is_last` 且无错误行"合成（同 QuantAPI 规则）；`is_last`/`err` 判定失败走 `recovery(success=false)`。

## 5. 接入形态与反例清单（结论）

**零架构新取舍**：`adapters/kdvip/` + `sts/broker/kdvip_fixtures.hpp` + pimpl + 值 fixture + `inject_*` seam + 单泵 `drain()` + sidecar —— ADR-027/033 形态原样复用。适配器级差异（不动端口）：① 价格/金额字符串换算（另写，不抄 QuantAPI double 表）② char 状态表（另一张锁表）③ POD 由调用方持有、回调期外失效（值拷贝纪律相同）④ 登录后补 `ReqQryStkholderAcct` ⑤ 无恢复事件靠查询合成 ⑥ GB2312 文本转码（`kdvip_version==kWinVer` 时每字段 `to_utf(...,"gb2312")`；Linux 版待核）。

**CSP 反例不抄**（详见 `docs/csp-reference.md` §3）：布尔生命周期竞态；心跳断线 `clear()` 归因表全丢且重连不重登、任一步失败 `LOG_FATAL` 放弃；固定重连无退避；未知推送静默丢弃（无日志）；终态只在 fill 回调清理（confirm-only 终态残留）；`CastStkBizAuction` 未知→`100`、`CastStkSuspended` 默认→停牌、`CastOrderPrice` 默认 `%.2f` 等**缺省吞值**；空结果回调 bug；`CancelOrderRep.msg` 非 win 分支错用 `szStkName`；`#define private public` 白盒测试 + 活服务器直连。STS 对应：显式 `start/stop` 门控、sidecar 跨重启归因、查询补单、metrics 计数、fail-closed 映射、seam 注入单测。

**可抄的纯逻辑**：`CastOrderPrice` 字符串格式化（改 STS 类型）、`HandleKdvipQueryRep` 分页驱动、状态/方向/业务动作映射表、登录后补股东号流程、测试数量撮合约定（见下）。

**联调备忘**（CSP `test_kdvip_trade.cpp:71-79` 实测其测试环境撮合规则，STS 同环境可复用）：委托量 `2000`=不成交可撤 / `3000`=部分成交 / `4000`=废单 / `2500`=分笔成交 / 其他=全部成交；其测试参数：`heart_beat_invl=0`、`ssl_cfg=-1`、价格=涨停×0.95、可撤单=涨停价+2000 股。

**前置**：`maCliApi.so` 随包（`lib/Lnx64`，未链接验证 —— dlopen 实测留到立项段）；SDK 许可、账号、环境与 M7-2 账号前置同类；字典值域、今昨拆分、资金字段细义留联调核对。

## 6. 暂不动

- 期权（`maCliOptTradeApi`）/期货（`maCliFutureTradeApi`）/基金申赎/组合单（`CombOrder`）/闪电版（`*Flash` 回调）/ETF 申赎成交（`OnRtnOrderFill(CRtnStkPefEtfFillField*)` 同名重载 —— 适配器只覆盖股票现货重载，**防遮蔽需注意**）—— 头文件有接口但 STS 范围不含。
- 银证转账/改密（CSP 侧 `kCHUnsupportedReq`）、历史委托/流水（CSP `kNotImplemented`）—— STS 端口不含。
- `SubTopic` 行情推送（KDVIP 也有行情 API；STS 行情按 AMA，届时另议）。
- `iOrderBsn` 簿号语义、`szOrderText` 扩展键值透传 —— 联调探索。

## 7. 整体移植 CSP/channel 的评估（2026-10-01，研究段）

**结论：抄逻辑，不抄架构** —— 不整体移植。理由：① 依赖树（boost::asio/format + `csp/util` 日志/错误域/配置 + Jamfile.v2/Boost.Build）与 STS FetchContent 钉版本（ADR-021）冲突；② `TradeChannelSpi` ≠ STS `TradeApi`，shim 层（`Impl : SPI`→值化→队列）代码量与自写适配器相当，省不掉；③ CSP 转换写在其自有归一化类型上，类型边界恰在转换函数处，可搬的是字段知识（本文档）不是函数体；④ 反例清单（上节）需修回；⑤ 与已交付的 AMA/QuantAPI 适配器形成双架构并存。若未来一次性接入 5+ 家 CSP 已生产运行的柜台且接受其地基，另立 ADR 评估。
