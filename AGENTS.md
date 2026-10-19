# QTS 协作约定

## 项目边界

- QTS 是事件驱动交易系统（C++ 上限 C++17，当前单进程部署）；当前仅支持 paper/simulation，禁止将任何改动表述为可用于实盘。
- Broker SDK 和私有字段只能存在于 `adapters/`，并受 `QTS_ENABLE_VENDOR_SDKS` 控制；`core`、`strategy`、`execution`、`oms`、`risk` 和 `qts_core` 不得引用它们。
- `strategy` 只能生成 `OrderIntent`；`execution` 负责拆单；OMS 是唯一订单状态迁移点；每笔新单和撤单到达 `TradeApi` 前必须同步经过风控。
- 热路径（行情 → 策略 → 执行 → 风控 → OMS → adapter）不得增加同步文件、数据库、网络 I/O 或无界等待。JSONL 是恢复权威，SQLite/PG 仅为可重建投影；超时表示结果未知，必须对账，不能当作失败。

## C++ 与数据约束

- C++ 以 C++17 为上限（兼容老版本操作系统和编译器；`filesystem` 直接用标准库，不引入第三方），禁用 C++20+ 特性；价格和数量使用既有定点 `Price`/`Quantity`，禁止引入浮点价格或 epsilon 比较。
- 使用明确所有权：拥有资源优先 `std::unique_ptr`，借用关系使用引用或非拥有指针。
- 订单状态迁移必须合法、可追溯且幂等；外部回报按 `(broker, account, execution_id)` 去重，不能重复计入成交。
- 配置、账本和 Broker 输入一律 fail closed；仅对已有账本字段的兼容默认值可例外，并须有测试。
- `channel` 是一套交易接入点及凭据/会话/费率；`venue` 仅指交易所市场规则，不要混用。

## 修改与验证

- 先定位相邻模块、现有测试和文档，再做最小范围改动；行为变化须补充或更新测试。
- 不添加第三方依赖，除非维护者明确批准；依赖必须固定版本/哈希，不能将密钥、许可证或环境文件提交入库。
- 新增 `.h`、`.hpp`、`.c`、`.cpp` 文件遵循仓内既有文件头和 `clang-format` 风格。
- 默认验证命令：

  ```bash
  ./tools/qts.sh test
  ./tools/qts.sh drill
  ./tools/qts.sh format
  ./tools/qts.sh secrets
  ```

- 若改动涉及账本、订单、风险、恢复或策略，额外运行相关集成测试及 `qts_replay verify`；若无法运行，明确说明原因与未验证范围。
- 用户可见的启动、配置、恢复或运维行为变化，同时更新 `README.md` 或 `docs/runbook.md`。

## 变更纪律

- 保持 `bin/cfg/` 为可提交配置；`bin/db/`、`bin/log/` 和凭据只作为本地运行产物。
- 不使用真实凭据、真实账户或真实 Broker 做验证，除非维护者在当前任务中明确授权并给出范围。
- 不用“大规模重构”掩盖小问题；保持改动可审查、可回放，并说明执行过的验证。
