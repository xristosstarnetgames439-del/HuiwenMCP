# 汇文 MCP

鸿蒙 PC 上的 PDF 与 Markdown 阅读、转换应用。本仓库目前处于界面和原生桥接搭建阶段。

## 当前状态

- 鸿蒙端使用 ArkTS 首页，提供 PDF 预览、MD 预览、PDF 转 MD 三个功能入口；按钮目前只显示选中提示，实际功能尚未接入。
- `libhuiwen_native.so` 提供 `ping()` 和异步 `checkPython()`；后者保留为内部自检接口，不显示在首页。
- 鸿蒙模块只保留 ArkTS 与 NAPI 代码；Qt 桌面界面计划在独立的 `qt_app/` 中实现。
- Python HNP 的打包和调用链已在真机通过自检，返回 `HUIWEN_PYTHON_OK 3.14.6`。PDF 转换和文件预览尚未接入。每完成一个阶段，请同步更新本节。

## 工程结构

以下以当前仓库根目录为基准。`已有`表示工作区中存在，`生成`表示本机打包产物，`规划`表示后续阶段的目标目录或文件；规划项尚未创建。

```text
HuiWenMCP/
├── AppScope/                          [已有] 应用名称、图标和版本
├── entry/                             [已有] 鸿蒙 HAP 模块
│   ├── src/main/
│   │   ├── module.json5               [已有] 设备类型、Ability 与私有 HNP 声明
│   │   ├── ets/
│   │   │   ├── entryability/EntryAbility.ets  [已有] ArkTS 启动入口
│   │   │   └── pages/Index.ets         [已有] 首页和三个功能入口
│   │   └── cpp/
│   │       ├── CMakeLists.txt          [已有] 原生库构建配置
│   │       ├── napi_bridge.cpp        [已有] ArkTS → C++ 的 NAPI 接口
│   │       ├── python_runner.cpp/.h   [已有] 定位 HNP、启动固定自检脚本
│   │       └── types/                 [已有] 原生模块的 ArkTS 类型声明
│   ├── build-profile.json5            [已有] HAP 构建与 ABI 配置
│   ├── hvigorfile.ts                   [已有] 在 HAP 签名前注入 Python HNP
│   └── oh-package.json5               [已有] 模块依赖
├── converter/                         [规划] Windows、macOS 与鸿蒙共用的 Python 转换逻辑
│   ├── pyproject.toml
│   └── pdf2md/convert.py              PDF → Markdown 调用入口
├── qt_app/                            [规划] Windows、macOS 的 Qt 桌面界面
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── qml/                            主窗口、PDF 与 MD 预览组件
│   └── src/                            转换任务控制与 Python 启动器
├── hnp/arm64-v8a/                     [生成] 鸿蒙 Python 运行环境，不进入 Git
│   └── huiwen_python.hnp               私有 HNP，包含 Python 与自检脚本
├── packaging/                         [部分已有] 各平台打包脚本和配置
│   ├── ohos/                           [已有] pack_python_hnp.sh、hnp.json、python_smoke.py
│   ├── windows/
│   └── macos/
├── tests/pdf_samples/                 [规划] 文字、双栏、表格等转换样本
├── third_party_licenses/             [规划] 产品分发所需的许可材料
├── CMakeLists.txt                     [规划] Qt 桌面工程入口
├── build-profile.example.json5       [已有] 不含签名信息的全局构建配置模板
├── build-profile.json5               [本机] 签名配置，已被 Git 忽略
└── README.md                          [已有] 项目状态与维护说明
```

内部自检调用链是 `checkPython() → libhuiwen_native.so → 私有 HNP Python → python_smoke.py`；首页只保留三个业务入口。鸿蒙 UI 已选用 ArkTS；规划中的 Qt UI 面向 Windows、macOS，共用 `converter/` 的转换逻辑。`libhuiwen_native.so` 由 CMake 构建并装入 HAP，不需要在 Git 中保存编译好的 `.so`；当前 HNP 包含 Python，后续转换依赖的原生扩展也应随 HNP 部署。

## 本地构建

工程当前配置为 HarmonyOS PC、`arm64-v8a`，目标 SDK 为 `6.1.1(24)`。首次克隆后，将 `build-profile.example.json5` 复制为本机的 `build-profile.json5`，需要安装到设备时再在 DevEco Studio 配置签名。鸿蒙模块的 NAPI `.so` 由 CMake 从源码构建，无需下载 Qt 运行库。

先取得鸿蒙 arm64 Python 的已解压安装目录，要求包含 `bin/python3` 和 `lib/`。自检阶段可用 [Harmonybrew 的预编译 Python 3.14.6](https://github.com/Harmonybrew/ohos-python/releases/tag/3.14.6)；它仅用于证明 HNP 调用链，后续转换依赖需单独验证。文章提供的社区 Python 3.12.9 [下载链接](https://gitcode.com/OpenHarmonyPCDeveloper/cmd-pkgs/releases/download/pkgs/python-3.12.9-ohos-aarch64.tar.gz) 也可作为输入，但本机访问该链接返回 401。将下载的压缩包解压后执行：

```sh
bash packaging/ohos/pack_python_hnp.sh /绝对路径/python-3.14.6-ohos-arm64
```

脚本使用本机 SDK 的 `hnpcli`，在 `hnp/arm64-v8a/` 生成 `huiwen_python.hnp`。这属于本机生成物，已被 Git 忽略；从 GitHub 新克隆后需先生成它。若 SDK 工具不在默认位置，可设置 `HNPCLI=/绝对路径/hnpcli`。HNP 的 `name/version` 固定为 `huiwen_python/1.0.0`，更换 Python 版本时不必改 NAPI 路径。

当前 Hvigor 的默认 `PackageHap` 不会自动带入根目录的 HNP。`entry/hvigorfile.ts` 在 `PackageHap` 后、`SignHap` 前调用 SDK `app_packing_tool.jar --hnp-path` 重打未签名 HAP，并检查其中是否存在 `hnp/arm64-v8a/huiwen_python.hnp`；缺少 HNP 时构建会失败。SDK 不在 DevEco Studio 默认安装目录时，设置 `DEVECO_SDK_HOME` 或 `OHOS_SDK_HOME` 指向 SDK 根目录。

```sh
cd entry && ohpm install
cd ..
hvigorw assembleHap --mode module -p product=default -p module=entry@default
```

也可以在 DevEco Studio 中使用 **Build Hap(s)/APP(s)**。修改代码后至少执行 `git diff --check`；需要验证设备行为时，再安装生成的签名 HAP 并启动 `EntryAbility`。Python 运行时自检已在真机通过；后续若更换运行时或 HNP 内容，可临时调用保留的 `checkPython()` 再验证。本机签名文件已被 Git 忽略，推送前用 `git status` 确认它没有进入提交。

## 维护约定

- 新增功能时更新“当前状态”，区分已运行的能力与尚未接入的入口。
- 调整模块、原生库或打包方式时更新“工程结构”和“本地构建”。
- 不把生成的 HAP、构建缓存或本机 SDK 路径写成跨机器通用的依赖。
- 不提交 `entry/libs/` 下的本机 Qt 运行库；鸿蒙端需要的 NAPI `.so` 在构建时生成。
