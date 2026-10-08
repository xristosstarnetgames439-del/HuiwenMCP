# 汇文 MCP

鸿蒙 PC 上的 PDF 与 Markdown 阅读、转换应用。本仓库目前处于界面和原生桥接搭建阶段。

## 当前状态

- 鸿蒙端使用 ArkTS 首页，提供 PDF 预览、MD 预览、PDF 转 MD 三个功能入口；按钮目前只显示选中提示，实际功能尚未接入。
- `libhuiwen_native.so` 提供 `ping()`，用于验证 ArkTS 到 C++ NAPI 的调用链。
- 原有 Qt 示例和 Qt 运行库暂时保留在工程中；当前首页不再启动 Qt。
- Python HNP、PDF 转换和文件预览尚未接入。每完成一个阶段，请同步更新本节。

## 工程结构

以下以当前仓库根目录为基准。`已有`表示工作区中存在，`规划`表示后续阶段的目标目录或文件；规划项尚未创建。

```text
HuiWenMCP/
├── AppScope/                          [已有] 应用名称、图标和版本
├── entry/                             [已有] 鸿蒙 HAP 模块
│   ├── src/main/
│   │   ├── module.json5               [已有] 设备类型、Ability 与后续 HNP 声明
│   │   ├── ets/
│   │   │   ├── entryability/EntryAbility.ets  [已有] ArkTS 启动入口
│   │   │   └── pages/Index.ets         [已有] 首页和三个功能入口
│   │   └── cpp/
│   │       ├── CMakeLists.txt          [已有] 原生库构建配置
│   │       ├── napi_bridge.cpp        [已有] ArkTS → C++ 的 NAPI 接口
│   │       ├── main.cpp               [已有] 保留的 Qt 示例入口
│   │       └── types/                 [已有] 原生模块的 ArkTS 类型声明
│   ├── libs/arm64-v8a/                [已有] Qt 运行库等随 HAP 分发的 .so
│   ├── build-profile.json5            [已有] HAP 构建与 ABI 配置
│   └── oh-package.json5               [已有] 模块依赖
├── converter/                         [规划] Windows、macOS 与鸿蒙共用的 Python 转换逻辑
│   ├── pyproject.toml
│   └── pdf2md/convert.py              PDF → Markdown 调用入口
├── qt_app/                            [规划] Windows、macOS 的 Qt 桌面界面
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── qml/                            主窗口、PDF 与 MD 预览组件
│   └── src/                            转换任务控制与 Python 启动器
├── hnp/arm64-v8a/                     [规划] 鸿蒙 Python 运行环境
│   └── python-pdf2md.hnp               打包生成物，包含 Python 与转换依赖
├── packaging/                         [规划] 各平台打包脚本和配置
│   ├── ohos/
│   ├── windows/
│   └── macos/
├── tests/pdf_samples/                 [规划] 文字、双栏、表格等转换样本
├── third_party_licenses/             [规划] 产品分发所需的许可材料
├── CMakeLists.txt                     [规划] Qt 桌面工程入口
├── build-profile.example.json5       [已有] 不含签名信息的全局构建配置模板
├── build-profile.json5               [本机] 签名配置，已被 Git 忽略
└── README.md                          [已有] 项目状态与维护说明
```

当前调用链是 `ArkTS 首页 → libhuiwen_native.so → ping()`。鸿蒙 UI 已选用 ArkTS；规划中的 Qt UI 面向 Windows、macOS，共用 `converter/` 的转换逻辑。`libhuiwen_native.so` 由 CMake 构建并装入 HAP；Python 及其原生扩展将随 HNP 部署，现有 `entry/libs/arm64-v8a/` 主要存放 Qt 运行库。

## 本地构建

工程当前配置为 HarmonyOS PC、`arm64-v8a`，目标 SDK 为 `6.1.1(24)`。首次克隆后，将 `build-profile.example.json5` 复制为本机的 `build-profile.json5`，需要安装到设备时再在 DevEco Studio 配置签名。构建仍会编译保留的 Qt 示例，因此还需检查 `entry/build-profile.json5` 中的 `QT_PREFIX` 是否指向本机的 Qt 5.15.12 鸿蒙构建目录。

```sh
cd entry && ohpm install
cd ..
hvigorw assembleHap --mode module -p product=default -p module=entry@default
```

也可以在 DevEco Studio 中使用 **Build Hap(s)/APP(s)**。修改代码后至少执行 `git diff --check`；需要验证设备行为时，再安装生成的签名 HAP 并启动 `EntryAbility`。本机签名文件已被 Git 忽略，推送前用 `git status` 确认它没有进入提交。

## 维护约定

- 新增功能时更新“当前状态”，区分已运行的能力与尚未接入的入口。
- 调整模块、原生库或打包方式时更新“工程结构”和“本地构建”。
- 不把生成的 HAP、构建缓存或本机 SDK 路径写成跨机器通用的依赖。
