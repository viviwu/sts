# vendor/ — 仓内 broker SDK（私有仓限定，ADR-040）

本目录收录券商专有 SDK 的构建输入（头 + 预编译库），随 STS
git 携带，换机零拷贝。**仅限私有仓**：AMA 头文件含
`Redistribution ... are not permitted` 声明，公开即违约。

## 内容清单（整版替换，不要零散拼文件）

| 路径 | 内容 | 版本 | 来源 |
| --- | --- | --- | --- |
| `ama/include`, `ama/lib/Lnx64` | AMA 头全集 + 6 个 `.so`（`libz.so.1` 为 SDK 自带，原样保留） | V4.5.8.250331 | 华锐 AMA demo 包 `c++/` |
| `quantapi/include`（4 头）, `quantapi/lib/libatpquantapi.so` | QuantAPI 头 + 主库 | V2.7.1.2 | `ATPQuantAPI_STD_rhel_gcc-4.8.5_x64_Institution_QuantApi_V2.7.1.2_20250619` |

## 铁律（违反即 revert）

- 只收 `include/` + `lib/`：demo 源码、`*.lic` 授权文件、账号密码**永不进仓**
  （demo 源码里硬编码了测试密码，进仓触发密钥扫描；`.gitignore`
  仅放行上表 7 个 `.so`，其余二进制一律拦下）。
- 第三方 pin 版源码（yaml-cpp/json/sqlite）**不进仓**：走 FetchContent
  自动下载（ADR-035 不变）；`STS_OSS_ROOT` 仍可指向仓外离线覆盖。
- 更新整版替换 + vendor 门构建 `ctest` 通过后再用；旧版包留仓外归档。

## 用法

```bash
# vendor 门构建（SDK 默认即仓内 vendor/，STS_VENDOR_DIR 可覆盖）：
cmake -S . -B build-vendor -DSTS_ENABLE_VENDOR_SDKS=ON \
  -DCMAKE_BUILD_TYPE=Debug -DSTS_ENABLE_WARNINGS_AS_ERRORS=ON
cmake --build build-vendor -j$(nproc) && ctest --test-dir build-vendor
```
