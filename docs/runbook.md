# 运行手册：部署、灾难恢复与演练（M3-4）

> 范围：单进程 paper 主程序（qts_server）的本地部署与恢复。真实资金、真实 Broker 与
> PG/ClickHouse/OTel 均未接入（ADR-021）；凡涉及生产资金的操作一律以
> “停止 + 人工审批” 为默认动作。

## 1. 本地启动

原生（已验证，主流程）—— 或用一键脚本（自动选 gcc-toolset-11）：

```bash
./tools/qts.sh all      # build + test + drill（提交前门禁）
./tools/qts.sh demo     # 长跑演示 + admin 摘要（见 §7）
./tools/qts.sh doctor   # 换机自检：缺什么依赖/密钥/网关，逐项给补全办法
# 子命令：build|test|drill|demo [ticks [ms]]|replay <report|verify>|debug-demo|admin ...|format|secrets|clean|all|doctor
```

等价手敲（产物统一落 `./bin`，配置在 `./bin/cfg`）：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./bin/qts_server bin/cfg/paper.example.yaml
```

容器（M3-4 记账时验证过镜像构建 + `up` drill；`./bin` 布局迁移后做过静态
核对，本轮补到 volume 对版 + 可跑验证，`up --build` 仍需 docker 主机重跑）：

```bash
cp deploy/paper.env.example deploy/paper.env  # 可选：paper 无需密钥
docker compose -f deploy/docker-compose.yml up --build qts-paper
# 有 docker 主机时跑一次 up、看到 drill 行即视为重新验证通过
```

> 本轮静态审计（2026-10-05，无 docker 主机）：`Dockerfile` Release 构建与
> `./bin` 布局一致（产物落 `./bin`，命令路径对版）；`compose` 改跑
> `bin/cfg/paper.docker.yaml`（账本/SQLite/快照/日志全落 `/var/lib/qts`
> volume，旧 `paper.example.yaml` 是内存 drill、volume 挂了也白挂），
> `restart: unless-stopped`（drill exit 0 不重跑；长跑 `ticks: 0` 配 SIGTERM
> 优雅停机）。volume 布局已用路径代换在本地实测（6/18/0 + 五件套落盘 +
> db 一致）。待办：docker 主机上 `up --build` 实跑；常驻 daemon
>（`ticks: 0` 长跑配置 + 日志卷 sizing）单独立项。

> 说明：Intel Mac（OrbStack，原生 linux/amd64）上构建约 16 分钟
> （base 拉取 ~2 分钟 + apt ~10 分钟 + FetchContent/编译 ~5 分钟）；
> ARM Mac 上如遇耗时请放后台执行，确认 `qts_server` 正常输出 drill 行
> 后再视为验证通过。

账本与券商序列文件落在命名卷 `ledger-data`（`/var/lib/qts`）；
升级前先备份该卷（`docker run --rm -v ledger-data:/data -v "$PWD":/bak ubuntu tar czf /bak/ledger-backup.tgz /data`）。
本地原生布局用 `./tools/qts.sh backup`（`bin/db` + `bin/log` + manifest），
恢复即解包覆盖后启动（启动恢复自动执行）。每季度至少跑一次
`./tools/qts.sh recover-drill`（备份→清空→恢复→校验 PASS/FAIL），输出
留存即演练记录。

## 2. 健康信号

主程序成功时同时输出三类信号（见 `apps/server_main.cpp`）：

1. `drill ok: submitted=6 ledger_events=18 open=0` —— 人读行；
2. `{"level":"info","component":"server",...}` —— 单行 JSON，可直接进日志采集；
3. `oms_*` Prometheus 文本 —— stdout 过渡格式；`metrics.listen` 打开后
   同一份口径改走采集端点（`curl 127.0.0.1:9090/metrics`，见 `paper.example.yaml`
   注释），长跑中可直接被 Prometheus 抓取，退出即消失（无历史保留，
   历史查账本/SQLite）。

任一缺失或 `open != 0` / `reconcile dirty` 即视为不健康，按第 4 节处理。
每次运行（含 drill）都会在 `bin/log/` 留下一个唯一命名的日志文件
（`qts-paper-<日期>-<时间>-pid<PID>.log`），与 stdout 同源。

## 3. 密钥

- 券商密钥只来自环境变量（`QTS_<PREFIX>_{CUST_ID,FUND_ACCOUNT_ID,PASSWORD}`），
  从不进 yaml、日志与账本（`qts/config/credentials.hpp`）。
- `deploy/paper.env` 填写后永不提交；`tools/secret_scan.sh` 与 CI `secrets` job
  拦截已暂存的疑似密钥（提交前本地先跑一次）。
- 轮换：更新环境/`paper.env` → 重启进程 → 确认启动日志打印
  `broker credentials loaded` 且值为 `set`（`redacted()` 永不吐明文）。

## 4. 灾难恢复

### 4.1 进程崩溃重启（标准流程）

1. 保留现场：拷贝 `ledger.jsonl` 与券商序列 sidecar（`AlternatePaperBroker` 的
   seq 文件），两者必须成对备份/恢复，缺一不可。
2. 从最近 `LedgerSnapshot` 恢复未终结订单（`OrderManager::open_orders` /
   `restore_order`），再用 `FileEventLedger::replay(last_event_id + 1)`
   重放后续事件（覆盖于 `tests/unit/file_ledger_test.cpp::file_backed_oms_restart`）。
3. 向 Broker 查询未终结订单并 `reconcile_open_orders`：只告警、不自愈，
   差异由人工确认后通过恢复驱动决策（`dev_scheme.md` §4）。
4. 当日首次启动、有文件账本时自动恢复：重放历史（坏账本拒绝启动）、
   查 Broker 未终结单并只告警不对账自愈（纸通道重启失忆，重建的活单报
   missing-at-broker 属预期内告警）；仍需人工决策见 `dev_scheme.md` §4。
   跨交易日：`trading_day.rollover`（默认 `00:01`，本地时）到点自动触发
   结算链（通道 books → OMS 去重集 → `trade_date` 重打标签），日志里以
   `trading day rolled` 为证。真进程重启仍是后置项（启动恢复已落地，
   见 §9 末）。

### 4.2 账本文件损坏

`FileEventLedger` 构造时逐行校验，坏行直接抛错（fail closed，绝不跳过）。
处理：停机 → 用备份卷恢复 → 重跑第 4.1 节；若无备份，宣布该账本不可信，
从 Broker 侧查询重建并全量对账，差异全部人工确认。

### 4.3 Kill switch 与断线

- 四级 kill switch（全局/账户/策略/标的）任一阻断新风险（`risk_engine`）；
  触发后只允许撤单与受控减仓通道。
- 运行时热加载：`kill -HUP <pid>` 重读配置的风控限额/开关/频率与日志
  级别（`config reloaded` 为证，坏文件保持旧配置）；通道/策略/账本等
  结构项仍需重启生效。
- Broker 断线：冻结新单，保留取消/查询通道；重连后必须走一次
  `request_open_orders` + 对账（`SimulatedBroker::disconnect/reconnect`
  语义，`AlternatePaperBroker` 另有关闭会话门）。
- M15 开关留痕：halt/resume 与 6 个 block/unblock 动词成功执行即记 `control_applied` 事件（含动词/目标/结果状态/来源 inbox）；下单撤单启停不重复记（订单事件已覆盖）。
- M13 自动刹车（`risk:` 四键，默认 3 次/30%+100 笔/30%，全 0 关闭，
  SIGHUP 同步热加载）：单策略连续拒单达限自动停策略（先撤活单，
  日志 `risk auto action` 留痕）；全局拒单率/日亏损比例达限全局 halt
  并全撤。机器只停不开——恢复 = 人改配置 → SIGHUP → `unblock` +
  重启策略（SIGHUP 会按既有语义整体替换开关表，运行时自动 block
  一并清除，属预期行为，不是 bug）。
- 真钱授权：无批准文件（ADR-051）；券商口令（env）即授权，网关真账户登录复验。

## 5. 演练清单（每季度至少一次，记录留痕）

| 演练 | 对应自动化覆盖 | 人工确认项 |
| --- | --- | --- |
| kill switch 阻断 | `qts_test_risk` | 拒绝可审计、减仓通道独立批准 |
| 断线 holding/重连 flush | `qts_test_paper` | 重连后对账干净 |
| 进程重启恢复 | `qts_test_recovery` + `qts_test_file_ledger` | 快照+回放+差异告警 |
| 密钥轮换 | `qts_test_ops` | 无明文落盘、无提交泄漏 |
| 备份恢复 | 本手册 4.1–4.2 | 卷备份可解开、seq 配对一致 |
| 盘口部分成交/再撮合/恢复收敛 | `qts_test_paper`（9 组 book 用例） | 余量留活可撤、新 OMS 收敛到 partial+累计 |

## 6. 缺口与后置项（诚实记录）

- 主程序账本二选一（M5-1）：`ledger_file` 为空走内存账本（确定性
  drill，默认）；设为 JSONL 路径则经 `FileEventLedger` 跨重启追加、
  启动时续 event id。生产接线即把该字段指向 `/var/lib/qts/ledger.jsonl`
  （与 `ledger-data` 卷同路径，备份时与券商 seq 文件成对）。
- `PG/ClickHouse/OTel/真实券商` 在 `deploy/docker-compose.yml` 中为注释槽，
  需 infra 就绪后单独立项（ADR-021）。PG 侧应用层已就绪：`EventStore`
  抽象 + 通用异步 sink + 后端工厂（`open_event_store`，二选一、fail-closed），
  `PgEventStore` 为 fail-fast 存根（无 libpq 依赖；接线任务见其头文件：
  建链 + 同形 SQL + compose 服务 + 迁移 job）。启用方式：`postgres.conninfo_env`
  指向存 DSN 的环境变量（DSN 永不进 yaml/日志/账本），与 `sqlite_file`
  互斥；env 未设则该节静默无事。
- 行情：交易两包（TradeAPI/QuantAPI）无行情，行情源为 AMA 包（语义映射见 `resources/broker-mappings/archforce-ama.md`；M7-0 快照归一化 + M7-1 逐笔序列跟踪已落地，均为 fixture 版）。真 SDK 接线代码已交付（`QTS_ENABLE_VENDOR_SDKS=ON` + `QTS_VENDOR_DIR` 指向 SDK，vendor 门构建 `ctest 33/33`，见 §9.3）：QuantAPI 只读一次通过，AMA 会话已通，快照流验证待开盘窗口。

## 7. 长跑演示（M9）

> 范围：单进程 paper 主程序（qts_server） + 确定性模拟行情（`SimulatedQuoteFeed`，
> seed  pin），一次性 drill 行为不变（`demo.enabled: false` 即老路径）。
> 真实资金、真实 Broker 与 PG 均未接入，演示单不代表任何交易建议。

配置见 `bin/cfg/demo.yaml`：`demo`（长跑开关/ticks/interval/seed）、
`log.file`（日志文件 pattern，stdout 同时保留；解析出的唯一文件名每日、
每次启动都不同，首行 stdout 的 `log_file` 字段即本次路径，落在 `bin/log/`）、
`sqlite_file`（异步 SQLite 查询投影，`bin/db/`）、`ledger_file`
（JSONL 恢复权威，跨重启追加，`bin/db/`）。相对路径相对 `bin/cfg/`
解析，与启动工作目录无关。

终端一（启动长跑，`ticks: 0` 则跑到 Ctrl-C）：

```bash
./bin/qts_server bin/cfg/demo.yaml
# 退出时打印 drill ok + report + drill finished（三信号与 §2 一致，
# submitted 在长跑里是累计值）；Ctrl-C 走优雅停机（终轮对账 → 报告 → exit 0）
```

终端二（运行时查询，读文件离线投影，与运行中主程序零 IPC，并发读安全）：

```bash
./bin/qts_admin --ledger bin/db/qts-demo-ledger.jsonl orders      # 逐单状态/累计/版本
./bin/qts_admin --ledger bin/db/qts-demo-ledger.jsonl positions   # 持仓/现金/已实现
./bin/qts_admin --ledger bin/db/qts-demo-ledger.jsonl tail -n 5   # 尾部事件
./bin/qts_admin --ledger bin/db/qts-demo-ledger.jsonl reconcile   # 未终结订单（干净 exit 0）
./bin/qts_admin $L settle-report   # M14 daily settle: broker snapshot vs book diff
./bin/qts_admin --sqlite bin/db/qts-demo.db db                    # 投影水位与表统计
./bin/qts_admin state --state-file bin/db/live-state.json         # 活状态：计数/未结单/策略持仓（50 tick 一刷）
./bin/qts_admin state --socket bin/db/qts-query.sock               # 同上，但按需现查（需 `control.socket` 打开，长跑 tick 内应答）
```

落盘验证（日志文件名每日、每次启动唯一，取最新者）：

```bash
ls -t bin/log/qts-demo-*.log | head -n 3   # 本次与历史日志文件
tail -f $(ls -t bin/log/qts-demo-*.log | head -n 1)  # 与 stdout 同源的 JSON 日志
sqlite3 bin/db/qts-demo.db "SELECT COUNT(*) FROM events; SELECT COUNT(*) FROM orders; SELECT COUNT(*) FROM executions;"
# 或用 DBeaver 打开 bin/db/qts-demo.db 看 events/orders/executions/positions 四表
# （orders/executions 带 strategy_id + trade_date；positions 按策略×品种记
# 数量/均价/已实现；`qts_admin db` 打印策略订单数与持仓行）
```

单单逐笔 debug（默认 `level: info` 无 debug 行；改 `demo.yaml` 中
`log.level: debug` 后重跑，可见全链路：`market/quote` →
`strategy/intent` → `execution/child_accepted` →
`broker/execution_report`）：

```bash
sed -e 's/level: info/level: debug/' -e 's/qts-demo-ledger/qts-dbg-ledger/' -e 's/qts-demo\.db/qts-dbg.db/' -e 's|/qts-demo\.log|/qts-dbg.log|' bin/cfg/demo.yaml > bin/cfg/debug-demo.yaml
./bin/qts_server bin/cfg/debug-demo.yaml | grep '"level":"debug"' | head
rm -f bin/cfg/debug-demo.yaml bin/db/qts-dbg-ledger.jsonl bin/db/qts-dbg.db bin/log/qts-dbg-*.log
```

文件行情回放（`demo.feed: file`）：一行一 tick 的 CSV
（`instrument,last,bid,ask,bid_qty,ask_qty`，整数 tick/lot；`#`
注释与空行忽略，其余坏行拒绝启动），按文件顺序逐行回放，EOF
提前优雅收尾（与 tick 预算到期的关机路径相同）：

```bash
printf 'TEST,1000,950,1050,100,100\nTEST,950,900,1000,100,100\n' > bin/cfg/quotes.csv
cp bin/cfg/demo.yaml bin/cfg/file-demo.yaml
sed -i -e 's/feed: simulated/feed: file/' -e 's|# file: quotes.csv|file: quotes.csv|' -e 's/ticks: 500/ticks: 2/' bin/cfg/file-demo.yaml
./bin/qts_server bin/cfg/file-demo.yaml   # 启动打 "quote file loaded"，耗尽打 "quote file exhausted"
rm -f bin/cfg/file-demo.yaml bin/cfg/quotes.csv
```

手工下单（control inbox 的 `order` 命令）：操作员意图直进 OMS，走与
策略子单**同一道同步风控**（拒单进账本、可审计）；空策略归一到
`passthrough` 篮子；纸通道与 QuantAPI 通道同路径（切通道只改 yaml）：

```bash
cp bin/cfg/demo.yaml bin/cfg/manual-demo.yaml
sed -i -e 's|^# control:$|control:|' -e 's|^#   inbox:.*|  inbox: ../db/manual-inbox.jsonl|' -e 's/qts-demo-ledger/manual-ledger/' -e 's/qts-demo\.db/manual.db/' bin/cfg/manual-demo.yaml
./bin/qts_server bin/cfg/manual-demo.yaml   # 另开终端
./bin/qts_admin send --inbox bin/db/manual-inbox.jsonl order --id m-1 --account paper --instrument TEST --side buy --type limit --quantity 100 --price 900 --exchange sse --security A1
./bin/qts_admin --ledger bin/db/manual-ledger.jsonl orders | grep m-1   # status=live/cum
./bin/qts_admin send --inbox bin/db/manual-inbox.jsonl cancel --id m-1   # 撤单走同一通道
rm -f bin/cfg/manual-demo.yaml
```

市价单把 `--type` 换成 `market` 并加 `--style best5-ioc|best5-limit|own-best|cp-best`
（`--price` 此时为保护限价，主板可为 0）。风控拒单（限额/白名单/开关/
T+1/购买力）时 `send` 只追加、服务端判后记 `control refused`，原因查
账本 `risk_rejected` 事件；重单号直接拒收，永不双下。

## 8. 离线重放（qts_replay）

> 范围：读账本文件的离线工具（零 IPC，可与运行中主程序并发读），无 Broker、
> 无行情源。`report` 是 `summarize_ledger` 纯函数；`verify` 把全量事件重
> 吃一遍真 OMS 状态机（未知单/重复提交/非法迁移即失败），并要求两次重建
> 逐字段一致 —— M2 回放一致性验收的可执行版。

```bash
./bin/qts_replay report --ledger bin/db/qts-demo-ledger.jsonl   # 成交/毛利/回撤/版本
./bin/qts_replay verify --ledger bin/db/qts-demo-ledger.jsonl   # 严格重放 + 双重建一致
./tools/qts.sh replay verify    # 同上（账本路径默认演示文件）
```

配对约束（与 §4.1 seq 文件配对同理）：`ledger_file` 与 `sqlite_file`
必须成对备份/清理 —— 删账本重跑时必须同时删库，否则投影水位高于
账本（旧投影残留）。`qts_admin db` 的 watermark 与 `tail` 的最大
event_id 对不上时，即是此原因。备份别忘了同目录的
`ledger-snapshot.json`（关机快照）与 `archive/`（已轮转历史）。

磁盘保留（`retention` 段，默认 `log_files: 20`、`ledger_rotate_mb: 64`）：
每个日志 pattern 只留最新 20 个文件；账本超限在**优雅关机**时快照后
归档到 `archive/<时间>/`（有活单则延迟并告警，绝不丢单）。长跑建议
定期优雅重启一次，让轮转有机会执行。

## 9. 通道选择（accounts + routing）

> 主程序按 `accounts[].broker` 建通道（`simulated`/`alternate`，未知名直接
> 拒绝），无 `routing` 时只允许单通道，有 `routing` 则经 `Router` 下单
> （OMS 零改动）。切通道只改 yaml；alternate 的会话由工厂打开，其
> `client_seq` sidecar 落在账本同目录（`alt-<通道>.seq`，成对备份）。

```bash
./bin/qts_server bin/cfg/dual-paper.yaml   # simulated + alternate 经 Router（alternate 按 tick50/lot200 拒单，账本可见原因）
./tools/qts.sh dual 60 0                    # 短版双通道演示 + 对账 + verify
./bin/qts_admin --ledger bin/db/dual-ledger.jsonl tail -n 3  # 执行 ID 带 simulated:/alternate: 通道命名空间
```

在线测算（M8-2）：成交率/时延按单 EWMA（`routing.ewma_alpha_bps`，默认
2000），昨日配置值作种子与回退 —— 无时钟、尚无样本、重启恢复噪声下
保持种子。选择前刷新统计，拆分子单按通道分别计量；`channel_stats()`
供查询。真实费率表与真实双通道验证仍后置（需真实流量/账号）。

## 9.3 真实通道接线（vendor 构建，M7-2）

> 范围：`qts_server` 直连真实 AMA 行情 + QuantAPI 交易（`QTS_ENABLE_VENDOR_SDKS=ON`
> 构建；默认构建见 vendor 名即拒）。行情走 `ama:` 段（服务端直连，非账户
> 通道），交易走 `accounts[].broker: quantapi`（factory hook，一个 broker
> 一 TradeApi）。密钥永不进 yaml（见下表），缺配/坏配/无会话全部拒绝启动。

> 依赖与 SDK 输入（零拷贝）：broker SDK（ama/quantapi 头 + `.so`）随仓在
> `./vendor/`（ADR-040，仅限私有仓；demo/`*.lic`/密码永不收录，见
> `vendor/README.md`），第三方 pin 版源码走 FetchContent 自动下载
> （`QTS_OSS_ROOT` 仍可指向仓外离线覆盖，见根 `CMakeLists.txt`）。

```bash
source /opt/rh/gcc-toolset-11/enable
cmake -S . -B build-vendor -DQTS_ENABLE_VENDOR_SDKS=ON -DCMAKE_BUILD_TYPE=Debug -DQTS_ENABLE_WARNINGS_AS_ERRORS=ON
cmake --build build-vendor -j$(nproc) && ctest --test-dir build-vendor  # 33/33
```

| 键 | 来源 | 说明 |
| --- | --- | --- |
| `ama.enabled/ums_servers/staleness_ms` | yaml | `1..8` 个 `ip:port`；与 `demo.feed ama\|file` 互斥；订阅主数据全量 |
| `QTS_AMA_CUST_ID/PASSWORD` | env | UMS 用户名/口令（`FUND_ACCOUNT_ID` 占位，三元组缺一不可，行情通道维持启动即登） |
| `quantapi.locations/order_way/sidecar_path` | yaml | 网关地址；`order_way` 默认 `"4"`；sidecar 缺省落账本旁 |
| `quantapi.agw_user/agw_password` | yaml | 网关用户（ADR-053 会话参数，非交易口令），可选、但必须成对；私有文件持有真值 |
| 客户号/资金号/交易口令 | 网关登录时交易员输入 | server 永不索要（ADR-053）：`login account=<客户号或资金号>` + 口令 → 柜台登录（客户号模式、再资金号模式）；成功绑柜台资金号，会话随进程常驻 |
| 终端识别码 | 生成 | `#<APP>IIP:<ip>;MAC:<mac>;HD:<hd>` 自动生成（缺项留空、格式保持）；`QTS_QUANTAPI_CLIENT_FEATURE_CODE` 显式 pin 优先（自动化） |
| `QTS_QUANTAPI_CUST_ID/FUND_ACCOUNT_ID/PASSWORD` | env（仅自动化） | 无 client 的单进程模式走网关侧代配，同一登录路径预登；C/S 模式不需要 |

fail-closed 矩阵（交易日窗口前即可验）：`locations` 缺失、坏 UMS、`ama`+`file` 混用 → `invalid config`（exit 2）；agw 只配一半 → 启动拒绝；默认构建配 vendor 名 → `unknown broker ... need a vendor-gated build`（exit 2）。vendor-only（无纸通道）仍拒绝（策略要 book），首个联调形态 = paper + quantapi（`routing` 下单）或 paper + live AMA 行情；柜台登录失败（错口令/错号）网关回 `bad credential`（失败次数计入柜台，确认账号类型后输入）。

### 9.4 真钱授权（ADR-053，server 不再索要口令）

> server 启动只待命（建通道、连网关就绪，不登柜台、不问口令）。交易员经网关
> 登录传入账户 + 口令，柜台登录成功才建会话并绑定资金号；会话随进程常驻
> （登出/被顶替/客户端退出不断柜，策略照跑）。paper 通道永不受影响。
> `qts_admin approve` 已退役（调用即拒绝并指路）。

```bash
# 私有 live 配置（绝不提交）：paper-book（策略 book）+ 真账户 + routing 下单；
# 策略可全 enabled: false（v1 无自启，交易员经网关 strategy_start 启动）
# accounts:
#   - {id: paper-book, broker: simulated}
#   - {id: "10014939", broker: quantapi}
# routing.channels: [{channel: quantapi, ...}]
# quantapi: {locations: ..., agw_user: ..., agw_password: ...}  # 网关用户放文件
# control.approval_file 如仍写着会被忽略（ADR-051）。
./bin/qts_server /tmp/live.yaml
# {"message":"quantapi standby (TCP ready, counter login on gateway login)",...}
# {"message":"live channels up (standby: counter login on gateway login)",...}
# 终端2：./bin/qts_cli → login（account=客户号或资金号 + 交易口令）
```

### 9.5 柜台主表与资金节拍（ADR-054）

> server 待命，柜台登录成功后：全市场证券信息扫一次（沪/深 ALL，约 5 万条，
> `is_last` 分页，`drain()` 每 tick ≤2000 行批量进 SQLite `instruments_cache`
> 投影，可重建、不进账本）；换日重扫（价格带是日度的）；资金/资产快照默认
> 60s 一轮（可配节拍，超时丢弃本次、不判失败）。
>
> 消费：risk 主表柜台优先（仅股票 `type=1`，tick/手数/价格带；yaml 表兜底未覆
> 盖项，STAR/创业板保护价规则缺板信息暂跳过）；网关 `cash` 对无 book 的 live
> 账户读最近快照（`shared:false` + `snapshot_time` 标注，不装实时）；节拍附带
> 柜台快照变更日志（现金/持仓数/总资产），对账仍只 diff 告警（ADR-046 口径）。

策略与执行可配：`strategies[]` 声明策略参数（`grid` 网格区间/手数，或
`rebalance` 目标权重 `targets[]` + 偏离带 `band_bps`；首个 `enabled: true`
生效，无启用直接拒绝启动），`execution.algo` 在 immediate/twap/pov/vwap/iceberg
间切换（各带默认参数，开箱即跑）：

```bash
./tools/qts.sh demo 30 0    # 默认 immediate；改 demo.yaml 切 twap 看切片下单
```

盘口与有序停机：`demo.depth_*` 打开 M6 盘口仿真（对称梯队，部分成交、
静置单续吃）；演示结束前自动全撤未结单再对账（drill 保持严格不自动撤）：

```bash
./tools/qts.sh demo 60 0    # 尾部可见 cancel_requested/cancelled 对，db 水位与账本对齐
```

换日结算：`trading_day.rollover`（各配置默认 `00:01`）到点自动结算，
不停机、持仓不断；重启恢复（重放 + Broker 对账告警）已落地，真重启
切日不再需要外部编排（账本 + alt seq 照旧成对备份）。

断线与故障演练：全断时冻结新单（父单在算法里排队，撤单/查询/行情不断），
恢复后重查对账（只告警）。`paper_sim.chaos_*` 可定时断指定通道：
Router 自动故障转移，单通道则冻结 —— 演练窗口别盖住最后一 tick，
否则以 dirty 退出告警（符合预期行为）。

## 9.1 严格 A股业务演示（demo-ashare.yaml）

> `demo.yaml` 为宽松兼容态（主数据/T+1/费用/库存全 opt-in）；本配置把四项
> 默认打开，是模拟通道下全场景业务的基准：标的主数据 + `enforce_sellable`
> + `fee_file: fees-ashare.yaml` + 期初库存 10000 股。

```bash
./tools/qts.sh ashare 200 0   # 200 tick（默认），先清账本起新 epoch，对账 + verify + state 一并打印
./bin/qts_admin --ledger bin/db/ashare-ledger.jsonl positions   # 现金/持仓与 broker 真相对上（含期初注资）
./bin/qts_admin state --state-file bin/db/ashare-state.json     # broker 真相（含期初库存）
```

## 9.2 多标的多策略演示（demo-multi.yaml）

> 双标的（`600000.SSE` + `000001.SZSE`，AMA 形态模拟源，真 L10 深度）× 三策略
> （两网格 + 跨标的再平衡），严格业务姿态：主数据 + T+1（含卖出预留，
> 见下）+ 费率文件 + 按策略注资。种子 pin 住（`seed: 7`，震荡型 walk；
> 趋势型种子下固定权重买方天然追涨，属策略行为非系统问题）。

```bash
./tools/qts.sh multi 200 0    # 先清账本起新 epoch，对账 + verify + state 一并打印
./bin/qts_admin --ledger bin/db/multi-ledger.jsonl positions
./bin/qts_admin --sqlite bin/db/multi.db db   # 按策略×品种分篮子
```

- 按策略注资：`starting_positions[].strategy` 具名归属（空=passthrough）。
  组合感知策略只能卖自家篮子可卖库存，注资必须与其目标匹配（注多了开局
  即砍仓，注少了无仓可调）；注资事件同样携带策略，回放与盘子一致。
- T+1 卖出预留（策略级）：活单未成余量按（策略，标的）预留可卖 —— 网格
  及再平衡的并发活卖不再超卖任一篮子。pod 级纪律，严于券商账户门；
  网格自家篮子须有注资（`demo-*.yaml` 已配，drill 的宽松路径不受影响）。
  预留每 tick 按活单绝对重算、提交路径同步刷新，零泄漏；热路径门控
  （非 T+1/非卖单跳过，吞吐基线 0.24s 不变）。
- 购买力门：限价买单名义须装进同步现金减活单买入预留；市价单无价不可
  计量而跳过（venue/lot 门仍在）。`enforce_cash` 默认关，`demo-*.yaml`
  打开；纸通道现金至此不再打负。
- 关机撤单顺序已排序，同种子两次运行账本逐事件一致（含尾部）。

注资进账本（M11-7）：`paper_sim.starting_cash`（显式写出时）与每笔
`starting_positions` 在 fresh ledger 上追加为 `funding_deposited` 事件
（现金 leg + 逐笔库存 leg），之后 admin/SQLite/verify 重放即得完整账本，
`--cash` 只在无注资老账本上起作用。重启不重复注资（账本非空即跳过，
账本是权威；`starting_cash` 是否显式由解析器记录，未动配置保持静默，
drill 账本逐字节不变）。
多 epoch 语义：重启追加的账本累计回放（连续账户视图），而失忆纸通道
每进程按配置重新注资 —— 与 §4.1 纸通道重启失忆一致，属预期内现象；
要干净 epoch 用 `qts.sh ashare`（先清后跑）。

## 10. 在线干预（control inbox，M11-4）

> 零 IPC：`qts_admin send` 向 inbox 文件追加命令，运行中主程序每 tick
> 拾取执行。撤单走执行层扇出（与停机路径同一份代码），进账本审计；
> 拉闸类只记结构化日志（暂无账本事件类型，诚实缺口，见下）。

```bash
./bin/qts_server bin/cfg/demo.yaml   # 另开终端
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl cancel-all
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl block-strategy --id grid-1
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl halt     # 全局停新单
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl resume
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl cancel --id <子单或父单号>
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl replace --id <活子单号> --quantity 400 --price 0
```

动词：`cancel <id>`（父单扇出，否则直撤）、`cancel-all`、
`block/unblock-strategy|instrument|account <id>`、`halt/resume`、
`replace <活子单id> --quantity N [--price P]`（0 保留原价，经
`replace_parent` 代际重规划，被召回子单同步撤单）。
`control.inbox` 为空即关闭（默认）；坏行只报错跳过，永不卡死主循环；
文件被替换则偏移复位重读。开关类命令的账本事件类型尚未立项，
审计以日志为准 —— 需要时单独立项。

## 11. 交易员策略启停（strategy_start/stop/list）与仿真实盘（live-sim）

> 两条线共用一套机制：策略实例平时由 yaml `strategies[]` 在启动时自动
> 创建（`live-sim.yaml` 的 `sim-grid` 即开箱测试策略）；交易员也可在
> 运行中用 `send` 按参数创建真实策略实例（`strategy_start`），停用
> （`strategy_stop`：先风控拉闸、再按 `<策略>-` 父单前缀定向撤单），
> 查看（`strategy_list`）。新实例进同一扇出向量、同一执行队列，单号
> 带代际前缀（重启不撞单），T+1 篮子按 tick 自动纳管。

```bash
./bin/qts_server bin/cfg/demo.yaml   # 另开终端（inbox 见该配置 control 段）
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl strategy-list
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl strategy-start --id my-grid --strategy-type grid --account paper --instrument TEST --quantity 100 --lower 900 --upper 1100 --grids 4 --start 1000
./bin/qts_admin send --inbox bin/db/control-inbox.jsonl strategy-stop --id my-grid
```

约束（fail-closed）：id 不可与在运行实例重名；type 限 grid|rebalance
（rebalance 读服务端配置的目标权重，lower/upper/grids 忽略）；
grid 要求 `0 < lower < upper`、grids/quantity 为正。无注册表（drill
路径）的上下文直接拒绝。

仿真实盘（M7-2d 仿真范围）：`live-sim.yaml` = 文件行情 + 纸通道 book +
Router 全量指向 QuantAPI 仿真柜台（`10014939`/`172.16.109.17:32001`，
密钥走 `QTS_QUANTAPI_*` env）。需 vendor 门构建；`qts.sh live-sim`
一键跑（含对账 + verify）。持仓/现金口径诚实缺口：策略快照与风控
T+1 用纸 book（期初库存对齐柜台真相），真成交以账本
`execution_applied` + 柜台查询为准，纸 book 不跟真 fills。

## 12. PostgreSQL 投影（ADR-038，正式期数据库）

> JSONL 账本仍是恢复权威；PG 与 SQLite 同为异步投影查询库（同一
> `EventStore` 契约、同一表/同一 payload 字节，一致性由
> `qts_test_pg` 跨后端对照锁定）。两者二选一、互斥（fail-closed）。

```bash
# 前提：系统 libpq + PG 门构建（默认构建零系统依赖不变）
sudo dnf install -y libpq-devel postgresql-server
cmake -S . -B build-pg -DCMAKE_BUILD_TYPE=Debug -DQTS_ENABLE_WARNINGS_AS_ERRORS=ON -DQTS_ENABLE_PG=ON
cmake --build build-pg -j$(nproc)

# 建库（密码只进 env，永不进仓库）
sudo -u postgres psql -c "CREATE ROLE qts LOGIN PASSWORD 'xxx' CREATEDB;"
sudo -u postgres psql -c "CREATE DATABASE qts_demo OWNER qts;"

# 配置：sqlite_file 留空 + postgres.conninfo_env 指向存 DSN 的 env 名
# sqlite_file: ""
# postgres:
#   conninfo_env: QTS_PG_DEMO_CONNINFO
export QTS_PG_DEMO_CONNINFO="host=localhost dbname=qts_demo user=qts password=xxx"
./bin/qts_server bin/cfg/<pg-config>.yaml   # 需 PG 门二进制

# 验证：账本 ↔ PG 对数，离线查询走同一 admin 入口
./bin/qts_admin db --pg-env QTS_PG_DEMO_CONNINFO
psql "host=localhost dbname=qts_demo user=qts" -c \
  "SELECT (SELECT COUNT(*) FROM events) AS events, (SELECT COUNT(*) FROM orders) AS orders, (SELECT COALESCE(MAX(event_id),0) FROM events) AS watermark;"
```

- 自迁移：空库首次连接自动建表（`events/orders/executions/positions`），无独立迁移 job；重连自动恢复，会话无状态（无预编译、无手工事务），水位续传收敛。
- 一致性测试：`QTS_PG_TEST_CONNINFO` 指向**空库**后跑 `./bin/qts_test_pg`（PG 门构建才有；未设该 env 则 skip，不红）。
- compose：`postgres:16` 服务已启用（`pg-data` 卷），客户端 DSN 经 `paper.env` 的 `QTS_PG_CONNINFO` 注入；PG 门镜像需另装 `libpq-dev`（见 Dockerfile 注释）。

## 13. 下游网关（M12-2，`qts::gateway`）

> QTS 作为 server：上接 broker 行情/交易 channel，下游前端应用（`qts_cli`/`qts_app`/…）经本入口接入。协议见 `include/qts/gateway/gateway_transport.hpp`（一行一 JSON，请求带 `req`、回执原样 echo、行情/回报走无 `req` 的推送帧）。

```bash
# 零参数开箱（bin 下直接跑，读 <exe-dir>/cfg/ 默认配置）：
./qts_server          # 读 cfg/qts.yaml（paper/simulated + 网关 socket）
./qts_cli             # 读 cfg/qts_app.yaml（连上面那台 + account/strategy 预设）
# 私有调整：拷一份改（绝不提交凭据）：
cp cfg/qts.yaml /tmp/live.yaml   # 在 live 文件里改通道，再 ./qts_server /tmp/live.yaml（口令只放 env）
```
raw 协议（socket/TCP 同形，一行一 JSON，`req` 回显；推送帧无 `req`）：
`{"req":1,"op":"login","account":"10014939","password":"..."}` → 会话（真 broker 账户；paper 免登）；
`{"req":2,"op":"cash","account":"paper"}` → 资金；submit/fills/
strategy_progress/open_orders/subscribe_* 同理（完整表见 `dev_scheme §9.6`）。

- 接入纪律（ADR-050/053）：网关不存任何口令。paper 账户免密（socket 文件权限即访问控制）；真 broker 账户（如 quantapi）由交易员在登录时传入账户（客户号/资金号/股东号均可）+ 券商口令，网关只做常量时间比对、不记日志，成功即登柜台并绑定资金号全权、无 scope，越户拒绝。匿名全局视图在有真账户时拒绝（防混入真数据）。密码只走 env/TTY 隐藏输入（`$QTS_GW_PW` 是网关登录口令的搬运，不是 server 启动口令）。异地登录顶替：同一账户同时只持一个会话，新登录即踢掉旧连接（旧端看到连接关闭）；策略跑在 server 侧，客户端退出/被顶替均不受影响（任务完成自退或收盘随 server 退出）。
- 能力现状：手动单/策略启停/资金持仓订单查询/行情、成交与策略三路推送 + 常驻 `qts_cli` 已通；P0 新增 `login/logout/whoami`、`fills`（成交明细，新→旧，`limit` 默认 200/上限 1000）、`strategy_progress`（子账户进度计数），`cash/open_orders/positions_for` 支持可选 `account` 过滤（未知 account 拒绝）。真分账：`cash/positions` 读账户所属通道 book（`shared:false`，各通道 fills 独立记账）；无 book 接线（旧单机/无名账户视图）仍为共享视图（`shared:true`），有 book 但未接线的已知账户诚实拒绝。
- 在线用法（`qts_admin live`：单次动词直调；真账户加 `--account-id NAME --credential-env ENV` 登录一次）：
  ```bash
  L="live --socket /tmp/qts-gateway.sock"
  T="live --tcp 127.0.0.1:19001"   # TCP 同协议，跨机用（回环外无 TLS，慎重）
  A="--account-id live --credential-env QTS_GW_PW"
  ./bin/qts_admin $L ping
  ./bin/qts_admin $L $A whoami
  ./bin/qts_admin $L $A cash --account paper
  ./bin/qts_admin $L $A list
  ./bin/qts_admin $L $A order --id gw-1
  ./bin/qts_admin $L $A positions --strategy grid-1 --account paper
  ./bin/qts_admin $L $A fills --account paper --strategy grid-1 --limit 50
  ./bin/qts_admin $L $A progress --id grid-1
  ./bin/qts_admin $L $A submit --id m-1 --account paper --strategy grid-1 --instrument TEST --side buy --type limit --quantity 100 --price 900 --exchange sse --security A1
  ./bin/qts_admin $L $A cancel --id m-1
  ./bin/qts_admin $L $A start --id my-grid --strategy-type grid --account paper --instrument TEST --quantity 100 --lower 900 --upper 1100 --grids 4 --start 1000
  ./bin/qts_admin $L $A stop --id my-grid
  ./bin/qts_admin $L $A watch --id strategies -n 5
  ```
  推送语义：帧 FIFO，请求的触发式推送排在回执前，客户端按 `req` 关联（见传输头协议注释）；`watch` 只收流不收回执（`quotes|reports|strategies`，Ctrl-C 停）。
- 常驻用法（`qts_cli`：REPL + 后台推送同屏，stdin 即脚本；`--tcp host:port` 可替 `--socket`）：
  ```bash
  export QTS_GW_PW=<你的口令>
  ./bin/qts_cli --socket /tmp/qts-gateway.sock <<'EOF'
  login account=10014939
  cash account=paper
  progress grid-1
  fills account=paper strategy=grid-1 limit=50
  watch what=cash interval=2 count=3
  sub what=reports
  submit id=m-1 account=paper strategy=grid-1 instrument=TEST side=buy type=limit quantity=100 price=900 exchange=sse security=A1
  order m-1
  stop id=my-grid
  unsub all
  logout
  quit
  EOF
  ```
  参数一律 `k=v`（裸 token 填动词默认键：单号/`strategy`/`account`/`sub`/`menu`/`what`）；`login` 取 `password=` 或 `$QTS_GW_PW`（tty 隐藏输入兜底）；`watch what=cash|orders|positions|fills|progress [interval=2] [count=5]` 轮询查询；`quick strategy|buy|sell` 按菜单引导补参数（tty 逐项提示回车取预设，脚本须 `confirm=y`），预设来自 `cfg/qts_app.yaml`（account/strategy/instrument + `quick:` 票面默认；价格为定点整数，4.20 元 = 42000）；`quick strategy` 另有 `stock`/`stock_avg`（划拨库存，区别于起跑价 `start`；yaml 静态库存走 `paper_sim.starting_positions`）；`sub` 可叠加，推送以后台 `<< ...` 行同屏打印；动词拒收只报错不掉线（`error: ...`），`quit`/EOF 退出。锁纪律：打印锁只包 `cout`，永不横跨网络等待（否则泵线程饿死造成 socket 背压、回执全超时——M12-4 实测抓到一次）。

## 14. 每日结算对账（M14，ADR-046）

> 口径铁律：柜台为准，QTS 只自述不自证。QTS 费用按 `fee_file` 逐笔累加披露，
> 柜台不公布费用累计——费用差只能经现金差倒轧，报告头写死该口径。

```bash
export QTS_GW_PW=<你的口令>
L="live --socket /tmp/qts-gateway.sock --account-id live --credential-env QTS_GW_PW"
A="live --socket /tmp/qts-gateway.sock --account-id live --credential-env QTS_GW_PW"
./bin/qts_admin $L settle-report    # 1. bounded 5 秒拉快照，输出三张表 + 快照时间 + 账本水位
# 2. 人工核对（clean 才往下走；dirty 先查原因——见证只记“看过什么”，不判谁对）
./bin/qts_admin $A settle-confirm --snapshot-time <TIME> --watermark <N> --cash-dirty 0 --position-dirty 0 --note "daily close"   # 3. 见证需 admin scope
./bin/qts_replay verify --ledger bin/db/qts-demo-ledger.jsonl   # 4. 见证事件重放跳过建仓，照过
```

paper 通道上 book 与柜台是同一对象，差异恒零——验收的是机器链条；
真差异能力由 `qts_test_reconcile` 合成单测锁定。
