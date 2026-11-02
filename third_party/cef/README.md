# 项目内 CEF 环境

本目录迁入通达 OpenQtCef 工程当前持有的鸿蒙 ARM64 CEF 环境。
这不是 v114.14.12 发布包，也不是完整 Chromium 源码；版本以 `environment.json`
中的头文件版本、来源提交和运行库校验值为准。来源可能包含尚未提交的修改，
`sdk/files.json` 记录每个迁入文件的实际 SHA-256，提交号不能代替该清单。

## 文件位置

| 位置 | 内容 |
| --- | --- |
| `sdk/include` | CEF 公开和内部头文件，不包含通达的 Qt 副本及业务 stub |
| `sdk/cmake`、`sdk/libcef_dll` | 同套 CMake 配置和 C/C++ 包装源码 |
| `sdk/ohos`、`sdk/log` | 鸿蒙嵌入适配源码，以及该包装源码引用的日志实现 |
| `sdk/web_engine` | ArkTS 适配模块、原生类型声明和浏览器核心资源 |
| `web_engine` | 随 Git 固定的轻量 HAR 适配源码和类型声明，供首次 OHPM 同步 |
| `sdk/Release` | 指向 `entry/libs/arm64-v8a` 的相对库链接 |
| `sdk/licenses` | 从同套 CEF 头文件保留的 BSD 许可声明 |
| `sdk/files.json` | 逐文件来源相对路径、大小及 SHA-256 |

运行库的唯一部署位置为 `entry/libs/arm64-v8a/`，包含 `libcef.so`、`libadapter.so`、
`libadapter_c.so`、`libc++_shared.so`。`libadapter_c.so` 来自通达的已剥离构建产物，
本次没有重新编译，因此尚未验证它与迁入源码的构建一致性。

## 恢复与升级

macOS ARM64 宿主首次 Run 由 `third_party/prebuilt/prepare.py` 自动恢复固定 SDK 和运行库，
不再要求克隆者提供通达工程。版本和哈希见 `third_party/prebuilt/manifest.json`。
维护者更换依赖来源时仍可使用以下迁入命令，再更新固定包及对应 HAR 源码：

```sh
python3 third_party/cef/prepare.py --source /已有工程/OpenQtCef
```

脚本先检查必需文件、ARM64 ELF 架构和动态依赖，再复制并逐文件校验。
已有同名运行库的内容不一致时停止，不覆盖现有 Qt 或其他版本的运行库。
SDK 文件及大体积运行库遵循现有本地依赖策略，不进入 Git；准备脚本、说明和
`environment.json` 进入 Git。清单不保存本机绝对来源路径。

## 汇文 Markdown 预览接入

SDK 迁入后会调用 `third_party/cef/prepare_preview.py`。Run 在恢复固定包后同样调用该脚本，
将 CEF 核心数据同步到 `entry/src/main/resources/resfile/`。HAR 源码已纳入 `web_engine/`，
固定其 JS 依赖并清除来源模块的权限声明；Run 不再生成或覆盖 HAR 源码。
应用在 `entry/src/main/module.json5` 中单独声明 CEF 使用的 `INTERNET` 和 `GET_NETWORK_INFO`。
SDK 原文件和逐文件来源清单保持不变；生成副本及核心数据被 Git 忽略。

`QtAbilityStage` 注册平台服务，`QtWorkspaceAbility` 注册窗口代理；使用汇文自己的代理，
不继承来源的 `WebQtAbility`，因此不调用其无边框设置或通用权限申请。
原生子进程按通达的库名约定加载 `libcefsimple.so`，调用其导出的 `CefMain`。
该入口只调用 `CefExecuteProcess`，不创建 Qt 应用或再次启动浏览器主进程。
旧版 ArkTS 子进程路径仍保留 `ets/process/CustomChildProcess.ets` 固定入口。
右侧点击预览后，Qt 才启动 CEF 消息线程、创建原生子视图；关闭时先释放浏览器再退出消息循环。

CMake 导入现有 `libcef.so` 和 `libadapter_c.so`，编译公开 C++ 包装层及最小子进程入口库。
`libcefsimple.so` 随原生构建更新并同步到 `entry/libs/arm64-v8a/`，由 Hvigor 收集进 HAP。
包装层日志接到汇文 HiLog，不使用来源工程的日志目录；Qt 和 CEF 本体不重编。
`HuiwenCEF` 日志记录子进程类型及退出码、浏览器创建、主页面加载和渲染进程退出状态。
`PrepareMarkdownResources` 会在 CMake／资源编译前刷新页面和 CEF 资源。

## 当前边界

- 已连接 CMake、`web_engine`、Ability、Run 资源准备和 MD 预览的源码链路；
  尚未执行 C++ 编译、HAP 打包或设备运行，不将其视为运行验收通过。
- 未迁入通达 OA/IM、登录、数据库、托盘业务，以及 Qt 运行库、宿主工具、
  `unstripped` 调试库、构建缓存、浏览器示例网页和测试资源。
- 原始 SDK 仍保留来源工程的适配代码；汇文入口绕开无边框、权限申请和业务日志行为。
- 已验证的动态依赖存在于迁入库或本机 ARM64 SDK；这不证明设备兼容或浏览器可运行。
- 来源未提供独立的 Chromium 和厂商完整许可证材料。本次只保存能找到的声明；
  分发前仍需从对应发布包补齐，不将该环境标记为许可证材料齐全。
