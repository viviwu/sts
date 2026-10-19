# 类型体系参考：CSP 成熟 A 股系统对照

> 背景：本项目类型设计参考了维护者此前开发的成熟 A 股交易系统 CSP 的
> gateway API（本机路径 `/Users/wuxw/Workspace/csp/code/csp/gateway/include/csp/gateway/api/`，
> 核心域类型在 `csp/util/trade_struct.h` 与 `csp/util/types.h`）。
> 决策与落地范围见 ADR-032；本文是逐项对照表，新增字段前先查这里。

## 1. 采纳项

| CSP 定义 | 含义 | QTS 落地 | 状态 |
| --- | --- | --- | --- |
| `biz_order_id`（柜台委托号，区别于 `cl_order_id` 本地 / `sys_order_id` 系统） | 三级委托 ID：撤单要原单柜台号（QuantAPI `OrigClOrdNo`），重启后对账靠它 | `ExecutionReport.broker_order_id`（append，OMS 不解释、适配器专用） | 已落地（ADR-032） |
| `reject_reason`(int) + `reject_msg`(string) | 结构化拒单，告警按码分类 | `ExecutionReport.reject_code`（0=无）+ 既有 `reason` 文本 | 已落地（ADR-032） |
| `Position.today_pos/yestd_pos` | 今昨仓拆分（T+1：今日买入不可卖） | `PositionSnapshot.today_quantity`（0=适配器不上报拆分） | 已落地（ADR-032），填充随 M7-2 适配器 |
| `OrderStat`：`trade_price` 成交均价 / `trade_amount` 累计金额 / `cancel_volume` 撤单量 / create·update·cancel 时间 | 委托全量状态快照（查询对账用） | 拟新增控制面 `OrderSnapshot`，不动热路径 `ExecutionReport` | 随 M7-2 控制面验证后立项 |
| 查询响应统一 `is_last` 分页标志 | 委托/成交/持仓/资金查询全是分页推送 | `TradeApi` 控制面 `request_open_orders` 等需明确多次回调 vs 结束语义 | 随 M7-2 端口语义定稿 |
| `Account.buy_frozen_amt` 与 `frozen_amt` 分开 | 买入冻结 vs 总冻结 | `TradeAccountSnapshot.frozen_cash` 现为合计 | 后置（对账出现差异再拆） |
| `QuerySecurityInfoReply.is_suspended / is_t0` | 停牌状态、T+0 标志 | `AshareInstrumentRules` 缺这两项（tick/涨跌停/单笔上限已有） | 后置 |
| `Account.withdraw_amt` 可取资金 | 总/可用/冻结/可取四分 | `TradeAccountSnapshot.withdrawable_cash` 已有（M0 即按同类要求设计） | 既有，CSP 印证设计 |

## 2. 明确不采纳

| CSP 做法 | 不采纳原因 |
| --- | --- |
| `PriceType`/`AmountType` = `double` | 守 ADR-003 定点整数（`Price`/`Quantity` 为 int64，避免浮点误差） |
| `typedef int32_t OrderTypeType` 等魔法整型枚举 | QTS 用 `enum class` 类型安全；CSP 该风格源自 C API 兼容 |
| `DateTimeType`（YYYYMMDDHHMMSSsss 整数） | 守 ADR-010 纳秒 `Timestamp`；交易所整数时间在适配器侧解析（AMA `orig_time` 即如此） |
| `pos_effect` 开平标识 | A 股现货无开平概念（期货才有）；两融由 `TradingBusiness` 覆盖 |
| Request/Reply 固定数组（`kMaxAccountNoCnt`） | QTS 端口用 handler + `std::vector` |
| `cl_term_id` 终端指纹进领域类型 | 归配置层：QuantAPI `kClientFeatureCode`（见 `archforce.md`） |

## 3. channel 模块对照（broker 对接参考，2026-10-01）

维护者提供的参考实现：`/Users/wuxw/Workspace/csp/code/csp/channel`（KCBP/KDVIP 柜台通道，
`TradeChannelApi` 请求面 + `TradeChannelSpi` 回调面）。M7-2c QuantAPI 适配器对照结论：

| channel 模块做法 | QTS 对应 | 取舍 |
| --- | --- | --- |
| `RequstStuff` 关联表（req_id → 原始请求 + 归属 API） | `inflight_req_`（request_id → `client_order_id`）+ sidecar `seq/map` 持久化 | 采纳并加强：跨重启归因（CSP 心跳断线即清表、无持久化） |
| 装饰 SPI 先归因/补全再转发用户 SPI | vendor 回调 → 值 fixture → `Impl::on_*` → 事件队列 | 同构（fixture 即装饰层，且是 seam 注入点） |
| "回调顺序、不并发" + `is_last` 查询契约 | 单泵 `drain()` FIFO + `is_last` 驱动 recovery/快照交付 | 采纳；另加 metrics 计数（CSP 无任何计数器） |
| 每请求恰一次回调（错误路径也合成回复） | submit/cancel 每路径恰一条报告（形状拒/通道不可用/网关拒/撤单失败） | 采纳 |
| 布尔标志生命周期（`is_login_ && is_logging_` 竞态可能） | 显式 `start()/stop()` 门控 + `logged_in_` 原子 | 刻意差异（CSP 此处为反例） |
| 固定间隔重连且连接失败即停、无退避 | M7-2 不做自动重连（运维 `start/stop` 重启），断连由回调计数 + 查询补单兜底 | 刻意差异；后续若引入重连必须带退避与上限 |
| 未知 `biz_order_id` 推送只 LOG 后丢弃 | 丢弃但计数（`unattributed_rtns`/`status_unknown_drops`）；RTN 先于 ack 到达先暂存、ack 绑定后 flush | 加强（可观测性 + 乱序兜底） |
| 测试靠活券商按委托数量约定驱动 + `#define private public` | vendor 回调全转值 fixture，`inject_*` seam 白盒注入（不改访问级） | 加强（CI 与 Linux 环境均可离线复现） |
| 整个 channel 模块整体移植进 QTS | 逐家 `adapters/<vendor>` + 逐函数复用纯逻辑（分页驱动/`CastOrderPrice`/状态表） | **不移植**（2026-10-01 维护者确认，"抄逻辑不抄架构"）：依赖树（boost/Jamfile）与 FetchContent 钉版本冲突、SPI≠端口须 shim、反例清单需修回、双架构并存；评估全文见 `resources/broker-mappings/kdvip.md` §7，推翻需 ADR |

## 4. 关联

- ADR-032：`dev_scheme.md` §6
- 柜台字段语义：`resources/broker-mappings/archforce.md`（QuantAPI）、`archforce-ama.md`（AMA 行情）
