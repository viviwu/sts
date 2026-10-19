# 实现路线图

## M0：工程与模型基线（2 周）

- 建立 CMake、编码规范、静态检查、单元测试和 CI。
- 固化核心类型、订单状态机、事件信封和配置模型。
- 实现内存事件总线、模拟时钟、模拟 Broker 与测试夹具。

验收：干净环境可构建/测试；订单状态机与风险规则单测覆盖主路径和非法转换。

## M1：单 Broker 交易闭环（4–6 周）

- 实现标准行情接口、一个 Broker 的 paper/sandbox 交易与行情适配器。
- 完成 OMS 的提交、取消、去重、回报处理与本地事件账本。
- 完成交易前限额、价格带、仓位、频率、账户/策略 kill switch。
- 提供一个简单策略和基础模拟撮合，支持端到端演练。

验收：可在模拟环境稳定运行；断线、重复回报、重启恢复和 Broker 对账均有自动化测试。

## M2：研究与回放一致性（4 周）

- 引入 Parquet 历史数据、可控模拟时钟和延迟/滑点/手续费模型。
- 策略、执行算法与风控在回放和模拟交易中复用同一接口。
- 生成订单、成交、PnL、回撤、风险违规等报告。

验收：同一事件记录的回放结果可重复；策略实验可由配置和数据版本重现。

## M3：多 Broker 与生产运行能力（6–8 周）

- 新增第二个 Broker 适配器，并验证合约、订单类型、状态码与精度映射。
- 完成多路由选择、统一持仓、外部对账、权限、密钥轮换、指标与告警。
- 引入 PostgreSQL/ClickHouse、OpenTelemetry、故障演练和发布流程。

验收：一个策略可在不同适配器间切换；路由、风控、审计和对账保持一致。

## M4：高级执行与规模化（持续）

- TWAP、VWAP、POV、冰山、智能路由和订单簿仿真。
- 组合优化、参数服务、策略隔离、多进程部署和容量压测。
- 只在设定并测量延迟 SLA 后进行 CPU 绑核、内存池、无锁队列等优化。

## M5：生产加固（已完成，零依赖）

- M5-1 网关账本二选一：`ledger_file` 为空走内存 drill，设路径经 `FileEventLedger` 跨重启追加。
- M5-2 热路径吞吐基线：`qts_bench_throughput`，只记录不设硬门禁。
- M5-3 tidy 转硬门禁：CI 六 job 全绿关闭。

验收：`ctest 17/17`（`WERROR`+UBSan），网关 drill `6/18/0` 不变。

## M6：订单簿仿真（已完成，零 infra 先行）

- M6-0 盘口深度（opt-in）：限价只吃盘内档、市价扫单、余量留活可撤；执行 ID 按笔加后缀防 OMS 去重坍缩。
- M6-1 静置单再撮合：新深度到达按提交序重吃；恢复查询对部分成交重报 partial + 累计。
- 智能路由/多进程后置，需真实需求或第二真实 channel 出现再立项（ADR-025）。

验收：`ctest 17/17`（`WERROR`+UBSan），网关 drill 不变。

## M7：AMA 行情适配（M7-0/M7-1 完成，真接线后置）

- 语义映射：AMA V4.5.8 只读头文件评审（ADR-026），见 `resources/broker-mappings/archforce-ama.md`。
- M7-0 快照归一化：`MDSnapshot` → `QuoteEvent`，录制 fixture，不链 vendor `.so`。
- M7-1 逐笔链路：序列跟踪 + 方向/类型映射（ADR-028：逐笔不进 `QuoteEvent`，缺口走 `QueryMDTick` 回补）。
- 后置（需外部就绪后单独立项）：M7-2 真 SDK 接线（vendor `.so` + UMS + 联调）、港股 `Exchange` 枚举扩展。

验收：`ctest 19/19`（`WERROR`+UBSan），网关 drill 不变；核心禁引 vendor 头文件。

## M8：智能路由（M8-0/M8-1/M8-2 完成，真实验证后置）

- M8-0 路由选择 + 故障转移：`ChannelStats` 配置输入 + 加权 `RoutingPolicy` + `Router : TradeApi`（OMS 零改动）。
- M8-1 Router 内拆单：pro-rata 合成父级回报（ADR-031）。
- M8-2 EWMA 在线测算：按单成交率/首 ack 时延，种子回退，`ewma_alpha_bps` 可配，实测驱动选择。
- 后置：真实费率表与真实双通道验证。

验收：`ctest 20/20`（`WERROR`+UBSan），网关 drill 不变。

## M9：演示线与 SQLite 落库（已完成，与 M7-2 并行）

- M9-0 持续模拟行情 + 长跑（opt-in，SIGINT 优雅停机）→ M9-1 日志文件 sink → M9-2 `qts_admin` 离线查询 → M9-3 SQLite 异步投影（JSONL 恢复权威不变）。
- 后续自研线上：自研 logger（debug 分级/过滤/时间戳/按日按启动唯一文件/跨天轮转）→ `qts_replay` 离线重放验证 → 统一 `./bin` 运行布局 → 网关通道选择（`accounts` + `routing` yaml 驱动）→ 主程序改名 `qts_server`。

验收：`ctest 21/21`（`WERROR`+UBSan），默认 drill `6/18/0` 不变。

## M10：生产化（进行中 —— 零真实通道依赖，见 dev_scheme 台账）

- 风控通电：持仓/频率/开关/T+1 在实盘循环生效 + SIGHUP 热加载 + T+1 可卖强制（opt-in）+ 每日亏损熔断（只许纯减仓）+ 购买力门（opt-in）+ 策略级 T+1（按策略篮子预留）。
- 策略/执行可配：网格参数 + 五算法切换 + 第二策略类型（rebalance 再平衡）；策略单号跨重启唯一。
- 运行能力：通道选择 yaml 驱动、启动恢复、换日结算、盘口实盘化、有序停机、在线状态快照（文件 + socket 活查询）、纸通道保真度可配（资金/延迟/滑点/费率）、断线冻结/恢复/混沌演练、磁盘保留与快照轮转、自动备份与恢复演练。
- 通道与操作面：服务端 vendor 接线（ama/quantapi 配置段 + factory hook）+ 文件行情源（CSV 回放）+ 手工下单入口（control order 直进 OMS 同道风控）+ 交易员策略启停（strategy start/stop/list）+ 仿真实盘（live-sim，文件行情 + 纸 book + Router 全量 QuantAPI 仿真柜台）。
- 可观测与存储：Prometheus 采集端点 + 存储抽象（EventStore 端口 + 通用异步 sink，PgEventStore 存根）+ 容器槽位对版。
- 策略归属域模型：单单必属一策略（Pass-Through）、持仓按策略分篮子、交易日期落库（ADR-036）。
- 数据库：SQLite 继续（PG 正式期后置，schema 可移植）。

验收（滚动）：双构建 `ctest 31/31`（`WERROR`+UBSan，另有 ASan/TSan job），`FORMAT_CLEAN`，CI 七 job 全绿，drill `6/18/0` 不变。

## M11：模拟业务保真（已落地，零真实通道依赖）

- M11-0 L1 行情保真 + 质量门（auction/halt/stale/crossed 全 opt-in，脏报价 hold 新单）。
- M11-1 标的主数据 + 期初库存（空主数据=宽松老路径兼容）。
- M11-2 AMA 双源（`demo.feed: simulated|ama`，真 L10 深度实盘化）。
- M11-3 多策略并发 + 执行真实轮转（`open_parent_ids` + 停机经执行层扇出）。
- M11-4 在线干预（`control.inbox` 零 IPC，cancel 进账本，开关记日志）。
- M11-5 A 股费率表（整数 ppm，佣金/印花/过户）→ M11-6 费用进账本 + 独立 `fee_file`。
- M11-7 注资进账本（`funding_deposited`，fresh ledger 追加/重启不重复；附带修 `EventBus` 查询悬空订阅重启 segfault）。
- M11-8 多标的多策略演示（AMA 双标的 × 3 策略）+ 按策略注资 + T+1 卖出预留 + 关机撤单排序。

验收：双构建 `ctest 31/31`（`WERROR`+UBSan，另有 ASan/TSan job），`FORMAT_CLEAN`，CI 七 job 全绿，drill `6/18/0` 不变。

## 后置（需外部条件，需单独立项）

- M7-2 收尾：AMA 快照流待开盘窗口验证；M7-2d 真资金受控交易需单次人工批准（仿真范围已由 live-sim 覆盖，真资金仍禁行）；live-sim 纸 book 不跟真 fills（已知诚实缺口，见 runbook §11）。
- 真实双通道验证、真实费率表（M8-2 在线测算已落地，待真实流量喂养）。
- PG libpq 真接线 + compose 启用 + 迁移 job（应用侧 `EventStore`/`PgEventStore` 存根已就绪；ClickHouse/OTel 仍整体后置，需 infra）。
- KDVIP 等通道按 `adapters/<vendor>` 立项（研究完成，方式"抄逻辑不抄架构"，见 `resources/broker-mappings/kdvip.md`）。
- 容器 `up` 实跑与常驻 daemon（待 docker 主机）。

