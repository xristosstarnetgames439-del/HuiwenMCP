# 汇文 MCP

鸿蒙 PC 上的 PDF 与 Markdown 阅读、转换应用。本阶段先把文字型 PDF 转成沙箱内的 MD 文件。

## 当前状态

- 首页保持 ArkTS；PDF/MD 预览按钮仍只导入文件，PDF 转 MD 按钮改为启动独立 Qt 工作区。
- Qt 工作区默认最大化并保留系统标题栏和窗口边框，输入、输出区域解耦；中部分割线可拖动，悬浮“转化”按钮随分隔线移动。
- 输入文件卡片调用现有 DocumentStore 选择 PDF、复制沙箱并安排输出路径。Qt ConversionService 在工作线程复用 PythonRunner 转换，成功后显示 MD 文件卡片。本阶段不渲染 PDF/MD 内容。
- 原有 Python HNP 自检与 PDF 转换已由用户在真机验证成功。新增 Qt 窗口交接、选择与结果位置操作需要真机验收。
- 汇文 Python 包只公开 `pdf_to_md()`；后续格式在依赖和设备转换通过后再增加。

## 工程结构

以下以当前仓库根目录为基准。`已有`表示工作区中存在，`生成`表示本机打包产物，`规划`表示后续阶段的目标目录或文件；规划项尚未创建。

```text
HuiWenMCP/
├── AppScope/                          [已有] 应用名称、图标和版本
├── entry/                             [已有] 鸿蒙 HAP 模块
│   ├── src/main/
│   │   ├── module.json5               [已有] 设备类型、Ability 与私有 HNP 声明
│   │   ├── ets/
│   │   │   ├── abilitystage/QtAbilityStage.ets [已有] Qt 平台初始化
│   │   │   ├── entryability/EntryAbility.ets  [已有] ArkTS 启动入口
│   │   │   ├── entryability/QtWorkspaceAbility.ets [已有] Qt 窗口及系统文件操作
│   │   │   ├── pages/HomePage.ets      [已有] ArkTS 首页和功能入口
│   │   │   ├── pages/Index.ets         [已有] QPA 固定加载的 Qt XComponent 承载页
│   │   │   ├── services/ConversionService.ets [已有] 页面调用原生转换的统一入口
│   │   │   └── utils/DocumentStore.ets [已有] 文件选择、沙箱副本与输出路径
│   │   └── cpp/
│   │       ├── CMakeLists.txt          [已有] 原生库构建配置
│   │       ├── napi_bridge.cpp        [已有] ArkTS → C++ 的 NAPI 接口
│   │       ├── python_runner.cpp/.h   [已有] 定位 HNP、启动固定自检或转换脚本
│   │       └── types/                 [已有] 原生模块的 ArkTS 类型声明
│   ├── libs/arm64-v8a/                [生成] 仅最小 Qt 运行库，不进入 Git
│   │   └── libQt5{Core,Gui,Widgets,Concurrent}.so、libplugins_platforms_qopenharmony.so
│   ├── build-profile.json5            [已有] HAP 构建与 ABI 配置
│   ├── hvigorfile.ts                   [已有] 在 HAP 签名前注入 Python HNP
│   └── oh-package.json5               [已有] 模块依赖
├── converter/                         [已有] 可供桌面端复用的 Python 转换逻辑
│   ├── huiwen_converter/              [已有] 汇文公开 Python API
│   │   ├── __init__.py                 [已有] 仅导出已实现的 pdf_to_md()
│   │   ├── api.py                      [已有] 路径校验、写入 MD、返回输出路径
│   │   └── _markitdown_adapter.py      [已有] 唯一直接调用 MarkItDown 的适配文件
│   └── pdf2md/convert.py              [已有] HNP 固定的 PDF 命令入口
├── qt_app/                            [已有] Qt Widgets 工作区，鸿蒙共享库/桌面可执行入口
│   ├── CMakeLists.txt                 构建 huiwen_qt；当前已验证鸿蒙编译
│   ├── main.cpp                       默认最大化并保留系统边框，显示成功后通知结束原首页
│   └── src/
│       ├── workspace/ConversionWindow.h/.cpp     主窗口与组件接线
│       ├── sourceView/SourceDocumentView.h/.cpp 输入区域入口
│       ├── resultView/MarkdownResultView.h/.cpp 输出区域入口
│       ├── components/
│       │   ├── FileCard.h/.cpp         PDF/MD 共用占位卡片
│       │   └── ConvertButton.h/.cpp    独立悬浮转化按钮
│       ├── services/ConversionService.h/.cpp    异步 PDF 转 MD API
│       ├── sandbox/
│       │   ├── DocumentItem.h         文件格式、名称和输入/输出路径
│       │   └── SandboxService.h/.cpp  请求系统选择、接收沙箱路径和打开输出位置
│       └── platform/harmony/qt_bridge.h/.cpp   Qt ↔ ArkTS 线程桥
├── hnp/arm64-v8a/                     [生成] 鸿蒙 Python 运行环境，不进入 Git
│   └── huiwen_python.hnp               私有 HNP 归档，内部包含：
│       ├── bin/python3                 Python 解释器
│       ├── lib/python3.14/site-packages/
│       │   ├── huiwen_converter/       汇文对外 Python API
│       │   ├── markitdown/             第三方运行源码，不含其 Git 仓库
│       │   └── PDF 依赖及原生 .so       pdfplumber、Pillow、pdfium 等
│       └── share/huiwen/convert_pdf.py NAPI 启动的固定命令入口
├── packaging/                         [部分已有] 各平台打包脚本和配置
│   ├── ohos/                           [已有] HNP 打包脚本、依赖锁定文件及上游补丁
│   ├── windows/
│   └── macos/
├── tests/pdf_samples/                 [规划] 文字、双栏、表格等转换样本
├── third_party/markitdown/            [子模块] Microsoft MarkItDown v0.1.8 原始源码
├── third_party_licenses/             [规划] 产品分发所需的许可材料
├── CMakeLists.txt                     [规划] Qt 桌面工程入口
├── build-profile.example.json5       [已有] 不含签名信息的全局构建配置模板
├── build-profile.json5               [本机] 签名配置，已被 Git 忽略
└── README.md                          [已有] 项目状态与维护说明
```

当前 Qt 转换链为 `ConvertButton → Qt ConversionService.convertPdfToMd() → PythonRunner → convert_pdf.py → huiwen_converter.pdf_to_md() → MarkItDown → outputs/md/*.md`。Python API 与第三方库适配保持原有边界。原 ArkTS 转换接口仍保留，但首页按钮原来的即时转换调用已注释。

鸿蒙跳转由 `HomePage.ets → startAbility(QtWorkspaceAbility) → QPA.startQtApplication() → libhuiwen_qt.so/main()` 完成。Qt 窗口实际显示后通过 NAPI 线程安全回调通知新 Ability；新 Ability 通过应用 EventHub 通知首页，只有发起跳转的首页消费一次通知并调用 `terminateSelf()`。这只结束首页 Ability，不结束应用进程。

当前 QPA 固定加载 `pages/Index`，不能让该路由同时作为 ArkTS 首页。`EntryAbility` 加载独立的 `pages/HomePage`；`Index.ets` 仅创建 Qt 的 NODE 类型 XComponent。`QtWorkspaceAbility.newLocalStorage()` 提供存储，QPA 写入 `idName` 并传给承载页。若在承载页放首页事件监听，会在 Qt 就绪时把 Qt Ability 也关闭；缺少存储方法则会产生 `newLocalStorage not found`、`Need object` 等日志。

文件选择链为 `SourceDocumentView → SandboxService → QtBridge → QtWorkspaceAbility → DocumentStore`。Qt 请求进入 ArkTS 线程处理；结果经 NAPI 投递到 Qt 线程，不跨线程直接操作界面。新 Ability 使用自己的 Context，首页关闭后文件选择仍可继续。

仓库中的 `third_party/markitdown` 是固定版本的上游源码，**不会把整个 Git 仓库装进应用**。`pack_python_hnp.sh` 仅把运行所需的 `markitdown/`、`huiwen_converter/`、Python 依赖及其原生 `.so` 放入 HNP 的 `lib/python3.14/site-packages/`，固定命令脚本放在 `share/huiwen/`；HNP 同时包含 Python 解释器。构建 HAP 时，`libhuiwen_native.so`、`libhuiwen_qt.so` 和最小 Qt 运行库进入 HAP，生成的 `huiwen_python.hnp` 也随 HAP 分发。ArkTS 不能直接导入 HNP 内的 Python 函数，必须经应用自己的 NAPI 桥接调用。

### 接口边界

| 位置 | 项目接口 | 调用者与职责 |
| --- | --- | --- |
| `converter/huiwen_converter/__init__.py` | `pdf_to_md(input_path, output_path) -> Path` | Python 公开业务接口；生成非空 MD 后返回路径，失败抛出异常。后续格式实现并验证后，才新增对应的显式接口。 |
| `converter/huiwen_converter/_markitdown_adapter.py` | 内部 `pdf_to_markdown()` | 唯一直接导入 MarkItDown 的文件；不作为业务 API。 |
| `entry/src/main/ets/services/ConversionService.ets` | `pdfToMd(inputPath, outputPath): Promise<string>` | ArkTS 页面调用；只转发到 NAPI，成功返回 MD 路径。 |
| `entry/src/main/cpp/napi_bridge.cpp` | `convertPdf(inputPath, outputPath)` | 原生边界；异步启动 HNP 中的固定命令，不接受任意 Shell 命令。 |

## 沙箱文件路径

当前设备用户号为 `100` 时，应用私有沙箱的预期物理路径如下。程序通过 `getApplicationContext().filesDir` 获取运行时目录，不写死用户号和包路径；设备上的实际映射需在验收时核对。

```text
/data/app/el2/100/base/cn.com.HuiWenMCP/
└── files/
    ├── inputs/
    │   ├── pdf/                 PDF 预览、PDF 转 MD 选入的副本
    │   └── md/                  MD 预览选入的副本
    └── outputs/
        └── md/                  PDF 转换生成的 Markdown
```

| 按钮 | 选取后记录的路径 | 本阶段行为 |
| --- | --- | --- |
| PDF 预览 | `files/inputs/pdf/<唯一编号>.pdf` | 打开系统选择器、复制文件、留在首页 |
| MD 预览 | `files/inputs/md/<唯一编号>.md`（选择 `.markdown` 时保留该后缀） | 打开系统选择器、复制文件、留在首页 |
| PDF 转 MD | 输入：`files/inputs/pdf/<唯一编号>.pdf`；输出：`files/outputs/md/<原文件名>-<唯一编号>.md` | 首页打开 Qt；Qt 输入区域选取文件，点击转化后显示 MD 卡片 |

系统选择器返回的 URI 只用于读取选中文件；应用内部后续使用沙箱副本路径。转换成功后才生成非空 `.md` 文件并显示路径；文字无法提取的扫描件会提示需要 OCR，不生成空结果。本阶段不渲染文件内容。MD 卡片先尝试让系统文件管理器打开 `outputs/md/`；系统不允许显示应用私有目录时，回退系统保存选择器导出 MD 副本。这是访问限制下的回退操作，不改变应用内输出路径；实际定位能力需真机验证。

## 本地构建

工程当前配置为 HarmonyOS PC、`arm64-v8a`，目标 SDK 为 `6.1.1(24)`。首次克隆后，将 `build-profile.example.json5` 复制为本机的 `build-profile.json5`，需要安装到设备时再在 DevEco Studio 配置签名。鸿蒙模块的 NAPI `.so` 由 CMake 从源码构建，Qt 运行库从本机鸿蒙 Qt 5.15.12 SDK 复制。配置方式任选其一：

```properties
# 被 Git 忽略的 local.properties，使用你自己的 SDK 绝对路径
qt.sdk.dir=/绝对路径/Qt5.15.12-ohos17-arm64-v8a
```

或为构建进程设置 `QT_PREFIX`。CMake 只复制 Core、Gui、Widgets、Concurrent 与 QPA 五个运行库到 `entry/libs/arm64-v8a/`；不会上传整套 Qt。库应来自同一 ARM64 SDK。

首次克隆时连同固定版本的 MarkItDown 子模块一起下载：

```sh
git clone --recurse-submodules https://github.com/xristosstarnetgames439-del/HuiwenMCP.git HuiWenMCP
cd HuiWenMCP
git submodule status third_party/markitdown
```

已经克隆了主仓库的开发者，在仓库根目录补拉子模块：

```sh
git submodule update --init third_party/markitdown
git submodule status third_party/markitdown
```

再取得鸿蒙 arm64 Python 3.14 的已解压安装目录，要求包含 `bin/python3` 和 `lib/python3.14/site-packages/`。原有自检使用 [Harmonybrew 的预编译 Python 3.14.6](https://github.com/Harmonybrew/ohos-python/releases/tag/3.14.6) 通过；本阶段的 PDF 原生依赖还需设备验证。执行：

```sh
bash packaging/ohos/pack_python_hnp.sh /绝对路径/python-3.14.6-ohos-arm64
```

脚本使用本机 Python 的 pip 下载 `requirements-pdf.txt` 中锁定的鸿蒙 arm64/musl wheel，复制 MarkItDown 子模块和汇文 Python API，应用 `markitdown-no-magika.patch`，最后用 SDK 的 `hnpcli` 在 `hnp/arm64-v8a/` 生成 `huiwen_python.hnp`。它是被 Git 忽略的本机产物；新克隆后必须先生成 HNP 再构建 HAP。若 SDK 工具不在默认位置，可设置 `HNPCLI=/绝对路径/hnpcli`；若本机 `python3` 无 pip，可设置 `HOST_PYTHON=/绝对路径/python3`。HNP 的 `name/version` 固定为 `huiwen_python/1.0.0`。

MarkItDown 官方源码保持在子模块的 `v0.1.8` 提交；它采用 MIT 许可，可修改并商用，但分发时需保留许可与第三方声明。其官方包强制依赖 Magika/ONNX Runtime，而当前目标架构无法直接解析 ONNX Runtime wheel。仓库中的补丁只让 Magika 可选；本阶段输入已由选择器限定为 `.pdf`，按扩展名交给 PDF 转换器。补丁不改动子模块原始源码。pip 能解析目标架构 wheel 只说明可获取包，不代表鸿蒙设备能加载其中的原生 `.so`；需在设备上完成导入及实际 PDF 转换验收。

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
- 不提交 `entry/libs/` 下的本机 Qt 运行库；NAPI 与 Qt 应用 `.so` 从源码构建，Qt 运行库从本机 SDK 复制。

## Qt 工作区验收

本阶段页面入口为 `qt_app/src/workspace/ConversionWindow.cpp`。未创建后续侧栏目录；新增侧栏时使用 `sidebars/`，避免与输入/输出区域混淆。

1. 点击 ArkTS 首页“PDF 转 MD”，Qt 默认最大化并保留系统标题栏、窗口边框及窗口控制按钮；查看 `QtWorkspace` 标签下的 `HUIWEN_QT_START`、`HUIWEN_QT_READY`，确认只有 `EntryAbility` 随后结束，`QtWorkspaceAbility` 保持运行；不应再出现 `newLocalStorage not found`、`Need object`。
2. 左右文档区域占满状态栏上方的可用高度，底部状态栏保持紧凑；还原窗口并调整大小时布局应同步变化。悬停中间细线出现水平分割光标；拖动改变两侧宽度，底部小型“转化”按钮保持居中在线上。
3. 点击左侧虚线加号选择 PDF，取消应保留原状态；选取后显示 PDF 占位图和原文件名。
4. 点击“转化”，操作期间不能重复选择或提交。成功后右侧显示 MD 占位图和输出文件名；失败在底部显示错误。再选一个 PDF 检查重复导入。
5. 点击 MD 卡片检查系统文件管理器定位；若系统拒绝私有目录，检查保存选择器能导出同一个 MD 文件。

HAP 包结构校验：

```sh
python3 packaging/ohos/check_workspace_hap.py entry/build/default/outputs/default/entry-default-signed.hap
```

Windows/macOS 的同一 Widgets 源码提供桌面入口，尚未编译验证和制作安装包。桌面转换需安装同一 Python 依赖、设置 `PYTHONPATH` 为仓库的 `converter/`，`HUIWEN_CONVERT_SCRIPT` 为 `converter/pdf2md/convert.py` 的绝对路径；可用 `HUIWEN_PYTHON` 指定解释器。
