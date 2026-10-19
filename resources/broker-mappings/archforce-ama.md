# Archforce（华锐）行情语义映射：AMA V4.5.8

研究输入（只读头文件，未链接 SDK；路径为本机绝对路径）：

- `/Users/wuxw/Workspace/archforce_ama_demo/c++/include/`（`ama.h`、`ama_struct.h`、`ama_datatype.h`、`ama_tools.h`、`ama_serialize.h`、`ama_property.h`，另有 `cfets/`、`hkex/`、`usa/`、`query/` 子目录）
- `/Users/wuxw/Workspace/archforce_ama_demo/c++/demo_src/ama_demo_tcp.cpp` / `ama_demo_fpga-tcp.cpp`（回调用法与线程注释）
- 版本：`revision` 为 `V4.5.8.250331-rc1.4`（`IAMDApi::GetVersion()` 可运行时确认）；指南见 `doc/` 下三份 PDF（本次未读，只做头文件映射）

范围：沪深北现货快照 + 逐笔委托/成交（`MDSnapshot`/`MDTickOrder`/`MDTickExecution`）。CFETS 银行间、商业港股、美股、指数/期权/衍生数据只记录存在性，暂不映射。

## 1. 已验证成立的设计

| STS 假设 | AMA 现实 | 结论 |
| --- | --- | --- |
| 行情定点整数 | 全 int64：价 `Price6`、量 `Qty2`、金额 `Amt5`、费率 `Rate6/Rate7`（扩大倍数以转码指南为准，头文件只标类型名） | 成立；适配器侧按 §3 换算 |
| 快照盘口表达 | `MDSnapshot` 自带 10 档 `bid/offer_price[10]` + `bid/offer_volume[10]` | 与 `QuoteEvent.bid/ask_levels` 同构，一档另进 `bid/ask_price/quantity` |
| 回调串行化（archforce.md §3.4） | demo 注释：禁在回调内直接业务处理，应入队 + 异步线程处理释放，否则阻塞上游被踢；批量 `ptr+cnt`，内存归 AMA，用后必须 `IAMDApi::FreeMemory` | 同构且更严格：适配器必须经队列喂给策略/risk，释放与投递同线程串行 |
| 恢复靠查询补全 | 断线补漏靠快照覆盖 + `QueryMDTick(channel_no, begin/end_appl_seq_num)` 回查当日逐笔（`IQuerySpi` + `is_last` 分页）；`kIncompleteData` 事件表示数据不全 | 与交易侧恢复（未终结查询 + 对账告警）同构，可直接映射 |
| 订阅模型 | `SubscribeData(kSet/kAdd/kDel/kCancelAll, market/security_code)`，空代码 = 全市场 | 与 `QuoteApi::subscribe/unsubscribe` 同构 |
| 密钥不进仓库 | `Cfg` 内 `username/password` 明文，UMS 双备服务器 | 沿用 `BrokerCredentials` 纯 env 政策，映射目录禁放账号 |

## 2. 类型/字段映射

### 2.1 市场映射（`MarketType`，`ama_datatype.h:129`）

| AMA | STS `Exchange` |
| --- | --- |
| `kSSE = 101` | `sse` |
| `kSZSE = 102` | `szse` |
| `kNEEQ = 2` | `bse` |
| `kHKEx = 103`（商业港股） | 无对应，缺口（见 §4.5） |
| `kCFETS = 104`、`kNASDAQ = 12` 等 | 范围外，适配器拒绝并告警 |

注意：交易侧映射（archforce.md §2）写“`104`北”，那是 ATP 交易接口的编号；AMA 行情里北交所是 `kNEEQ = 2`。两套编号体系不同，适配器不得复用交易侧的市场映射表。

`InstrumentId` 合成：`security_code` 是裸代码（`char[16]`），适配器按 `"<code>.<SSE|SZSE|BSE>"` 组装； tick/lot/价格带等静态规则走 STS `InstrumentMaster` 注入，不依赖 AMA 代码表（`GetCodeTableList` 系列仅为可选静态参考）。

### 2.2 `MDSnapshot` → `QuoteEvent`（`ama_struct.h:516`）

| AMA 字段 | STS 字段 | 换算 |
| --- | --- | --- |
| `bid_price[0]` / `bid_volume[0]` | `bid_price` / `bid_quantity` | 价 `/100`，量 `/100`（§3.1） |
| `offer_price[0]` / `offer_volume[0]` | `ask_price` / `ask_quantity` | 同上 |
| `bid_price[i]` / `bid_volume[i]`、`offer_price[i]` / `offer_volume[i]`（10 档） | `bid_levels` / `ask_levels` | 同上，10 档全带 |
| `last_price` | `last_price` | 价 `/100` |
| `total_volume_trade` | `cumulative_volume` | 量 `/100` |
| `total_value_trade` | `cumulative_turnover` | 金额 `/10`（§3.1） |
| `high_limited` / `low_limited` | `limit_up` / `limit_down` | 价 `/100`；0 按“无静态带”处理 |
| `orig_time`（`YYYYMMDDHHMMSSsss`，毫秒，交易所时间） | `exchange_time` | 需解析为 `Timestamp`（纳秒）；AMA 无平台到达时间 |
| ——（适配器自打戳） | `receive_time` | 回调入队时刻；质量门的陈旧判断全靠它（§3.2） |
| ——（快照无序号，见 §3.3） | `sequence` | 适配器合成单调序号 |
| `trading_phase_code[8]` | `market_phase` | §2.3 |
| 快照覆盖语义 | `quality` 初始 `valid`，再过 `assess_quote_quality` | §3.2 |

### 2.3 交易阶段 → `MarketPhase`

沪深编码不同体系（`ama_struct.h:516` 注释）：上海 8 位字符（第 0 位 `S/C/T/E/P/M/N/U`），深圳 2 位（第 0 位 `S/O/T/B/C/E/H/A/V`）。映射方向：开盘集合竞价 → `opening_auction`；连续（`T`）→ `continuous`；收盘集合竞价（沪 `U`/深 `C`）→ `closing_auction`；停牌（沪 `P`/深 `H`、全天停牌位）→ `halted`；闭市 → `closed`；其余 → `unknown`。完整对照表由适配器落地时固定并单测锁定。

### 2.4 逐笔（`MDTickOrder:623` / `MDTickExecution:650`）

逐笔带连续序号：`channel_no` + `appl_seq_num`（频道索引）+ `biz_index`（业务序号）；`bid/offer_appl_seq_num` 可关联委托—成交。方向：深 `1`买/`2`卖（`G`借/`F`贷），沪 `B/S`；深圳另有 `order_type`（市价/限价/最优）与成交 `exec_type`（`4`撤/`F`成交）。

缺口（ADR-028 端口决策，M7-1 已锁定）：`QuoteEvent` 保持快照语义，逐笔不进入；一期只做序列连续性（`AmaTickSequenceTracker`，缺口输出 `TickQueryItem` 形回补区间）与方向/类型锁表；完整本地簿重建（`orig_order_no` 跟踪、价位聚合、`'4'`-撤销关联）无真实 feed 不可验证，归 M7-2 联调。另：`MDOrderQueue`（50 明细委托队列）是簿重建的备选输入，联调时二选一。

## 3. 适配器落地前置（写代码前必须定）

1. **精度换算**：`Price6 /100`→`Price`（1e-4 元），`Qty2 /100`→`Quantity`（股），`Amt5 /10`→`cumulative_turnover`。整数除法截断方向要单测锁定（与交易侧“适配器侧换算”同责）。`Rate6/Rate7` 暂无 STS 对应字段，需要时再扩展。
2. **陈旧判断靠自打戳**：`assess_quote_quality` 的 stale 用 `receive_time`（`instrument.hpp:73`），AMA 只给交易所时间。适配器必须在回调入队瞬间打戳；`max_staleness` 默认 3s 对快照推送频率是否合适，由联调实测定，不得硬编码改默认值而不留配置。
3. **`sequence` 合成**：快照只有深 `channel_no`、无序号。建议按 instrument 维护本地单调计数器（收到即 `++`），简单且满足 `sequence <= last → out_of_sequence` 的语义；`orig_time` 回退的快照会被判 `crossed` 以外的 valid——乱序快照的取舍（覆盖 vs 丢弃）由适配器显式决策并单测。逐笔启用后，`(channel_no, appl_seq_num)` 复合键另行设计，不得与快照计数器混用同一序列空间。
4. **回调线程**：与交易侧 §3.4 同责——队列 + 专用投递线程 + `FreeMemory` 同线程释放；回调内只做浅拷贝入队。`Cfg::is_thread_safe` 只管“接口集线程安全”，不替代投递串行化。
5. **FPGA 通道字段缺失**：头文件多处 `note:FPGA 通道上海市场不披露此字段`（如 `IOPV`、`md_stream_id`、撤单统计）。适配器对缺失数值字段填 0 并降级可观测（不得当有效值用）；`IOPV` 等暂无 STS 字段，直接丢弃。
6. **会话映射**：`Init` 只校验配置，鉴权登录异步经 `OnEvent` 返回。`QuoteApi::connected()` ← `kUMSLogonSuccess` 置 true，`kChannelTCPSessionClosed` / `kChannelTCPHeartbeatTimeout` / `kUMSHeartbeatTimeout` 置 false 并冻结新订阅（类比交易侧断线语义）；`kIncompleteData` → 告警 + `QueryMDTick` 补逐笔 + 快照覆盖对齐。
7. **订阅粒度**：STS 按 instrument 订阅，AMA 支持空代码全市场订阅。适配器默认按需订阅（`kAdd` 精确代码），全市场订阅需显式配置开关（流量与 `ha_mode` 过滤成本）。

## 4. 暂不动

- 指数/期权/基金 IOPV/盘后/债券专场等几十个 `OnMD*` 回调：端口只有快照语义，需要哪个品种再逐个加映射。
- 港股 40 档（`MDHKExOrderSnapshot`）与经纪席位（`OrderBrokerSnapshot`）：`QuoteEvent.levels` 可装 40 档，但 `Exchange` 无港股枚举，需领域先扩展。
- CFETS/美股结构与 RDI 静态查询：范围外。
- `SubscribeDerivedData`（服务端委托簿/分钟 K）：等逐笔链路（二期）再定。
- `property::*` 亲和性/压缩/轮询调优：联调时按延迟基线再定，不前置优化。
