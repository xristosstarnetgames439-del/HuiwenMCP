# 汇文MCP · HuiwenMCP for Harmony

鸿蒙 PC 上的 PDF 与 Markdown 阅读、转换应用。本阶段先把文字型 PDF 转成沙箱内的 MD 文件。

## 许可证

本项目自有代码采用 [PolyForm Noncommercial License 1.0.0](LICENSE)，官方原文见 [PolyForm 网站](https://polyformproject.org/licenses/noncommercial/1.0.0)。这是源码公开的非商业使用许可。

允许在许可规定的用途范围内使用、学习、研究、修改和分发。商业产品开发、销售、商业服务等超出许可范围的用途，需要另行取得相关权利人的授权。许可对教育机构等组织另有允许用途规定，以 `LICENSE` 原文为准。

第三方源码、依赖和资源仍遵循各自的许可证及版权声明，不受本项目许可证替代。贡献代码前，请确认你有权将该贡献按本项目许可证授权。

## 当前状态

- 首页保持 ArkTS；银灰介绍区与主区域按约 30% / 70% 分配宽度，主区域提供文档预览、文档编辑、文档转化三个小正方形入口，普通字重的名称放在方块外下方。点击入口打开独立格式选择子窗口；预览和编辑仅展示 Word、PDF、MD 格式，转化窗口中的 PDF→MD 入口启动独立 Qt 工作区。
- 左侧介绍区上下内边距为 50vp、左右为 32vp；标题置顶，两段介绍组成间隔 20vp 的底部内容组，外层通过 `SpaceBetween` 分布并保持左对齐。
- 介绍区背景与黑色 ∞ 使用独立 SVG：银灰底图包含左上、右下的淡化文档及斜向柔光，∞ 的反光从中心向两侧连续衰减；底图等比例铺满，符号等比例居中，文字独立叠加，整体由 24vp 圆角裁切。本次素材与布局仅做静态检查和设计渲染，尚未编译、打包或设备验收。
- 首页右侧入口改为顶部排列，下方“最近工作区”默认展示三条任务目录记录；当前可恢复 PDF→MD 转化任务，加载原 PDF 和已有 MD，并在 Qt 载入成功后更新任务目录最近使用时间。本次最近列表、恢复与时间更新仅做格式检查，尚未编译、打包或真机验收。
- ArkTS 首页左侧有 60vp 半透明窄侧栏，底部 36vp 三横线 SVG 按钮可打开“关于、外观、截图、文件管理、设置”菜单。菜单通过实际布局坐标，让菜单底部与按钮底部对齐，菜单左边与按钮右边保持 8vp 间距；外观二级菜单提供浅色、深色、跟随系统，选择持久保存并应用于 ArkTS 首页和弹窗；Qt 工作区仍使用自身配色。关于弹窗读取实际应用版本号；截图、文件管理、设置本阶段只保留入口并提示暂未开放。首页内容背景与窗口容器分别设置透明，焦点态容器透明，非焦点态使用实色，右侧主内容保持不透明。
- Qt 开发依赖保存在 `third_party/qt/`，运行库从 `entry/libs/arm64-v8a/` 链接并随 HAP 分发；PDFium fork 保存在 `third_party/pdfium/`，Qt 左侧输入区域已接入其公开 C API 实现正文预览；已完成鸿蒙编译、HAP 打包；系统目录跳转沿用原有实现。
- DevEco Run 构建前执行 `PrepareNativeDependencies`，按当前 PDFium 源码增量生成 `libpdfium.so`；Qt/NAPI 的 C++ 编译后同步新库到 `libs`。没有保存源码自动构建、应用内动态替换库或保持进程状态的热重载。
- Qt 工作区默认最大化并保留系统标题栏和窗口边框，输入、输出区域解耦；中部分割线可拖动，悬浮“转化”按钮随分隔线移动。
- 输入文件卡片通过 DocumentStore 获取系统选取授权，再经 NAPI 调用 C++ harmony_sandbox 完成沙箱复制和路径安排。两侧操作统一到顶部“文件”菜单：查看模式默认预览，可切换为占位图；左侧由 PDFium 显示多页 PDF，右侧已有结果时由 CEF 显示 MD。转换成功自动切到预览并隐藏转化按钮；更换文件保留当前查看方式，清空旧结果并恢复按钮，取消选择保留原状态；尚未生成 MD 时仍显示等待提示。PDFium 后台串行渲染可见页，连续尺寸变化合并请求；位图缓存上限为 64 MiB，超长页面降低采样分辨率。Qt ConversionService 仍复用 PythonRunner 转换。本次默认预览及按钮显隐调整仅做格式检查，尚未编译、打包或真机验收。
- 原有 Python HNP 自检与 PDF 转换已由用户在真机验证成功。新增 Qt 窗口交接、选择与结果位置操作需要真机验收。
- 汇文 Python 包只公开 `pdf_to_md()`；后续格式在依赖和设备转换通过后再增加。
- Python 部署拆成稳定环境 HNP 与业务 `.pyz`：修改汇文 API、MarkItDown 源码或业务补丁只更新 pyz；修改解释器、目标依赖及环境打包配置才重建 HNP。本次拆分仅做语法和格式检查，首次迁移及设备转换尚待验证。

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
│   │   │   ├── components/FunctionSidebar.ets [已有] 首页半透明窄栏与底部菜单按钮
│   │   │   ├── components/HomeFeatureCard.ets [已有] 正方形功能卡片与 SVG 图标
│   │   │   ├── components/HomeFormatDialog.ets [已有] 独立格式选择子窗口与 PDF→MD 跳转事件
│   │   │   ├── components/RecentWorkspaceList.ets [已有] 固定列、文件名中部省略与最近记录点击
│   │   │   ├── pages/HomePage.ets      [已有] 首页功能切换与 Qt 窗口交接
│   │   │   ├── pages/Index.ets         [已有] QPA 固定加载的 Qt XComponent 承载页
│   │   │   ├── services/ConversionService.ets [已有] 页面调用原生转换的统一入口
│   │   │   ├── services/RecentWorkspaceService.ets [已有] 最近数量、模式文案、时间和恢复前校验
│   │   │   └── utils/DocumentStore.ets [已有] 系统选取授权与 NAPI 适配
│   │   └── cpp/
│   │       ├── CMakeLists.txt          [已有] 原生库构建配置
│   │       ├── napi_bridge.cpp        [已有] ArkTS → C++ 的 NAPI 接口
│   │       ├── python_runner.cpp/.h   [已有] 部署内置 pyz 到缓存，使用 HNP 解释器执行
│   │       └── types/                 [已有] 原生模块的 ArkTS 类型声明
│   ├── libs/arm64-v8a/                [生成] Qt、PDFium 与本项目动态库，不进入 Git
│   │   └── libQt5{Core,Gui,Widgets,Concurrent}.so、QPA、libpdfium.so、libhuiwen_{native,qt}.so
│   ├── build-profile.json5            [已有] HAP 构建与 ABI 配置
│   ├── hvigorfile.ts                   [已有] 按输入准备 Python HNP，并在 HAP 签名前注入
│   └── oh-package.json5               [已有] 模块依赖
├── converter/                         [已有] 可供桌面端复用的 Python 转换逻辑
│   ├── huiwen_converter/              [已有] 汇文公开 Python API
│   │   ├── __init__.py                 [已有] 仅导出已实现的 pdf_to_md()
│   │   ├── api.py                      [已有] 路径校验、写入 MD、返回输出路径
│   │   └── _markitdown_adapter.py      [已有] 唯一直接调用 MarkItDown 的适配文件
│   └── pdf2md/convert.py              [已有] pyz 的 __main__，也可供桌面直接执行
├── qt_app/                            [已有] Qt Widgets 工作区，鸿蒙共享库/桌面可执行入口
│   ├── CMakeLists.txt                 构建 huiwen_qt 并链接 PDFium；新增预览尚未编译验证
│   ├── main.cpp                       默认最大化并保留系统边框，显示成功后通知结束原首页
│   ├── tests/                         独立桌面预览回归检查，需宿主 Qt/PDFium
│   └── src/
│       ├── workspace/ConversionWindow.h/.cpp     主窗口与组件接线
│       ├── sourceView/
│       │   ├── SourceDocumentView.h/.cpp 输入卡片、预览切换与文件选择入口
│       │   └── PdfPreviewWidget.h/.cpp 后台 PDFium 渲染、分页滚动与尺寸自适应
│       ├── resultView/MarkdownResultView.h/.cpp 输出区域入口
│       ├── components/
│       │   ├── FileCard.h/.cpp         PDF/MD 共用占位卡片
│       │   ├── ConvertButton.h/.cpp    独立悬浮转化按钮
│       │   └── functionBar/
│       │       ├── FunctionBar.h/.cpp 顶部“文件”和“滚动设置”入口
│       │       ├── FileMenu.h/.cpp    查看方式、任务目录及当前业务模式菜单
│       │       └── SelectionMenu.h/.cpp 共用菜单样式、右侧蓝点与子菜单箭头
│       ├── services/ConversionService.h/.cpp    异步 PDF 转 MD API
│       ├── sandbox/
│       │   ├── DocumentItem.h         格式、路径、归档目录、缓存打开路径和使用时间
│       │   ├── SandboxStore.h/.cpp    三类目录、最近记录、编辑缓存和路径校验
│       │   └── SandboxService.h/.cpp  请求系统选择、接收沙箱路径和打开输出位置
│       └── platform/harmony/
│           ├── qt_bridge.h/.cpp              Qt ↔ ArkTS 线程桥
│           ├── harmony_sandbox.h/.cpp        鸿蒙沙箱规则与文件操作
│           └── harmony_sandbox_napi.cpp      异步导入与输出校验 NAPI
├── hnp/arm64-v8a/                     [生成] 鸿蒙 Python 运行环境，不进入 Git
│   └── huiwen_python.hnp               私有 HNP 归档，内部包含：
│       ├── bin/python3                 Python 解释器
│       ├── lib/python3.14/site-packages/ PDF 依赖及原生 .so，不含汇文 API 和 MarkItDown
│       └── share/huiwen/python_smoke.py 固定的解释器自检入口
├── entry/src/main/resources/rawfile/python/ [生成] 独立业务包，不进入 Git
│   └── huiwen_converter.pyz
│       ├── __main__.py                来自 converter/pdf2md/convert.py
│       ├── huiwen_converter/          汇文公开 API 与适配实现
│       ├── markitdown/                已应用业务补丁的第三方 Python 源码及包内资源
│       └── licenses/markitdown/       MIT 许可与第三方声明
├── packaging/                         [部分已有] 各平台打包脚本和配置
│   ├── ohos/                           [已有] HNP 打包脚本、依赖锁定文件及上游补丁
│   │   └── build_python_bundle.py      生成确定性业务 pyz，不变则保留成品与时间戳
│   ├── windows/
│   └── macos/
├── tests/pdf_samples/                 [规划] 文字、双栏、表格等转换样本
├── third_party/
│   ├── markitdown/                   [子模块] MarkItDown fork
│   ├── pdfium/                       [子模块] PDFium fork 与鸿蒙构建补丁
│   └── qt/                           [本机依赖] Qt 头文件、CMake 配置及宿主工具
├── entry/src/main/resources/rawfile/third_party_licenses/ [生成] Qt/PDFium 许可材料，随包分发
├── CMakeLists.txt                     [规划] Qt 桌面工程入口
├── build-profile.example.json5       [已有] 不含签名信息的全局构建配置模板
├── build-profile.json5               [本机] 签名配置，已被 Git 忽略
└── README.md                          [已有] 项目状态与维护说明
```

当前 Qt 转换链为 `ConvertButton → Qt ConversionService.convertPdfToMd() → PythonRunner → HNP Python 执行缓存 pyz/__main__.py → huiwen_converter.pdf_to_md() → MarkItDown → convert/<时间戳_文件名>/output/*.md`。Python API 与第三方库适配保持原有边界。原 ArkTS 转换接口仍保留；首页通过“文档转化 → PDF→MD”进入 Qt，不再选文件或直接执行转换。

鸿蒙跳转由 `HomePage.ets → startAbility(QtWorkspaceAbility) → QPA.startQtApplication() → libhuiwen_qt.so/main()` 完成。Qt 窗口实际显示后通过 NAPI 线程安全回调通知新 Ability；新 Ability 通过应用 EventHub 通知首页，只有发起跳转的首页消费一次通知并调用 `terminateSelf()`。这只结束首页 Ability，不结束应用进程。

返回链为 `ConversionWindow::closeEvent() → QtBridge.request("close") → QtWorkspaceAbility.returnHome()`：先启动首页，收到 `EntryAbility` 页面加载且进入前台的 `huiwen.home.ready` 后，经 `workspaceReply({action:"close"})` 通知 Qt 隐藏窗体并关闭浏览器实例；全部实例关闭后 Qt 回传 `closed`，工作区 Ability 才结束。QPA 已把系统标题栏关闭转为 Qt `closeEvent`，不重复注册关闭监听。CEF 平台的关闭请求也复用 `returnHome()`。

QPA 在进程内只启动一次 Qt 主循环，CEF 引擎也保留到应用退出。每次工作区注册/解除现有 NAPI 回调时，Qt 创建/释放对应的 `ConversionWindow`，下次进入仍使用 `showMaximized()`；关闭工作区不调用 `CefShutdown()`。窗口管理暂放 `main.cpp` 与 `closeEvent()`，后续再迁入窗口基类。本次仅做格式检查，连续往返和 CEF 展示仍需真机验收。

当前 QPA 固定加载 `pages/Index`，不能让该路由同时作为 ArkTS 首页。`EntryAbility` 加载独立的 `pages/HomePage`；`Index.ets` 仅创建 Qt 的 NODE 类型 XComponent。`QtWorkspaceAbility.newLocalStorage()` 提供存储，QPA 写入 `idName` 并传给承载页。若在承载页放首页事件监听，会在 Qt 就绪时把 Qt Ability 也关闭；缺少存储方法则会产生 `newLocalStorage not found`、`Need object` 等日志。

文件选择链为 `SourceDocumentView → SandboxService → QtBridge → QtWorkspaceAbility → DocumentStore`。Qt 请求进入 ArkTS 线程处理；结果经 NAPI 投递到 Qt 线程，不跨线程直接操作界面。新 Ability 使用自己的 Context，首页关闭后文件选择仍可继续。DocumentStore 只保留 Picker、URI 授权打开及关闭；目录、时间戳、命名、最近使用和缓存规则由通用 C++ `SandboxStore` 负责；鸿蒙授权 fd 复制与适配由 `HarmonySandbox` 负责。

仓库中的 `third_party/markitdown` 固定源码版本，**不会把整个第三方 Git 仓库装进应用**。`pack_python_hnp.sh` 只部署 Python 解释器、目标依赖及其原生 `.so` 和自检脚本；`build_python_bundle.py` 把 `markitdown/`、`huiwen_converter/`、固定转换入口及许可材料装入独立 `.pyz`，应用 `markitdown-no-magika.patch`，不修改子模块源码。两个产物随同一个 HAP 分发；NAPI、Qt 应用和最小 Qt 运行库仍按原有方式部署。

`EntryAbility` 和 `QtWorkspaceAbility` 在启动期调用 `configurePython(resourceManager, cacheDir)`。C++ 只读取固定的 `rawfile/python/huiwen_converter.pyz`，通过系统 ResourceManager 取出字节，以 SHA-256 命名保存到 `<应用 cacheDir>/huiwen-python/<sha256>.pyz`；缓存字节相同时不重写，缺失或内容不符时先写临时文件再替换。旧任务持有的旧版本文件不会在初始化新包时被覆盖，缓存可随应用缓存一起清理。

转换继续使用 `convertPdf(inputPath, outputPath)` / `RunPdfConversionResult()`，由 HNP 的 `bin/python3 -B -u <缓存 pyz> <PDF> <MD>` 启动固定业务入口。Python 从 pyz 导入业务模块，从 HNP 的 site-packages 加载依赖和原生扩展；不回退到 HNP 中旧的转换源码，不接受外部脚本路径。`pdf_to_md()`、stdout 最后一行 JSON 协议、错误码和页面调用方式保持原有边界。自检仍执行 HNP 的 `python_smoke.py`。

### 接口边界

| 位置 | 项目接口 | 调用者与职责 |
| --- | --- | --- |
| `converter/huiwen_converter/__init__.py` | `pdf_to_md(input_path, output_path) -> Path` | Python 公开业务接口；生成非空 MD 后返回路径，失败抛出异常。后续格式实现并验证后，才新增对应的显式接口。 |
| `converter/huiwen_converter/_markitdown_adapter.py` | 内部 `pdf_to_markdown()` | 唯一直接导入 MarkItDown 的文件；不作为业务 API。 |
| `entry/src/main/ets/services/ConversionService.ets` | `pdfToMd(inputPath, outputPath): Promise<string>` | ArkTS 页面调用；只转发到 NAPI，成功返回 MD 路径。 |
| `entry/src/main/cpp/napi_bridge.cpp` | `configurePython(resourceManager, cacheDir)` | 启动期部署应用内置 pyz；两个 Ability 都提供自己的 Context，不接受外部脚本路径。 |
| `entry/src/main/cpp/napi_bridge.cpp` | `convertPdf(inputPath, outputPath)` | 原生边界；异步用 HNP 解释器执行当前业务 pyz，不接受任意 Shell 命令。 |

## 沙箱平台分层

- `sandbox/SandboxStore.h/.cpp` 是 Qt Core 通用实现，负责 `preview`、`edit`、`convert` 三类目录、任务命名、最近记录、编辑缓存查询和目录归属校验，不引入 JSON、数据库或额外索引文件。鸿蒙把它编入 `libhuiwen_native.so`，Qt 工作区链接复用；桌面直接编译此实现。
- `SandboxService` 仍是 Qt 界面入口；鸿蒙通过 ArkTS Picker 获得授权，桌面使用 Qt 选择器。输入统一原子保存，失败只清理本次输入及空任务目录，不覆盖其他任务。
- `platform/harmony/harmony_sandbox.h/.cpp` 负责鸿蒙 fd 复制；NAPI 同步复制授权 fd 并设置子进程不继承，在工作线程以独立偏移分块读取，成功、失败和排队失败都释放 fd。`harmony_sandbox_napi.cpp` 的目录扫描也在工作线程运行。
- `harmonyImportDocument(filesDir, name, fd, kind, mode)` 返回 `Promise<HarmonySandboxDocument>`；`mode` 为 `preview`、`edit`、`convert`，取代原有布尔转换标记。支持 PDF、MD/Markdown、Word 导入归档；当前正式转换接口仍仅接 PDF→MD，归档支持 Word 不表示已经接入 Word 预览、编辑或转换。
- `harmonyValidateOutput(filesDir, path)` 接受新任务的 `convert|edit/<任务>/output/*.md`，兼容历史 `outputs/` 下的文件；仍检查真实路径、目录归属及非空可读 MD。历史数据保留原位，不批量迁移。
- `harmonyValidateTaskDirectory(filesDir, path)` 直接复用 `SandboxStore::validateTaskDirectory()`，只接受真实可读且不含符号链接或路径别名的 `edit|convert/<任务>/input|output` 目录。与 MD 文件校验分开，因此选取 PDF 后即可请求打开空输出目录；鸿蒙经 `openDirectory` 工作区事件调用系统文件管理器，桌面使用 `QDesktopServices`。

## Qt 文件菜单

顶部灰色功能条由 `FunctionBar` 组装，`FileMenu` 只维护菜单状态、发出请求，`ConversionWindow` 负责协调预览和沙箱服务；两侧区域不持有“预览／返回文件、更换文件、打开所在目录”工具按钮。`SelectionMenu` 统一绘制右侧 `#3983F7` 蓝点（直径 14px，圆心距菜单项右边缘 24px），子菜单箭头放在更靠右的位置，文件菜单与滚动设置保持对齐。

```text
文件
├── 查看模式
│   ├── 占位图模式
│   ├── 预览模式（默认）
│   └── 编辑模式（禁用，TODO）
├── 文件目录
│   ├── 输入目录
│   └── 输出目录
└── 切换模式
    ├── 预览模式（禁用，TODO）
    ├── 编辑模式（禁用，TODO）
    └── 转化模式（当前，右侧蓝点）
        └── 更换文件
```

- 查看模式同时控制左右区域，进入工作区和恢复任务时默认选中预览；未选择 PDF 时仍显示选择入口，未生成 MD 时右侧继续显示等待提示。选择新文件和开始转换保留当前查看方式，转换成功自动切回预览，并同步菜单蓝点；“切换模式”中的业务模式始终为转化。
- 仅当前“转化模式”拥有更换入口；取消保留原任务、结果、查看方式和按钮状态，选取成功创建新任务、清空旧结果并恢复转化按钮。按钮显隐直接读取 `MarkdownResultView::hasResultFile()`，不依赖 CEF 加载完成，不重复维护完成状态。选择或转换期间禁用查看方式切换与更换文件，避免任务串线。其他业务模式和编辑视图只保留 TODO，没有接入业务切换。
- 输入、输出目录均定位当前任务下的目录，未选择 PDF 时禁用；系统文件管理器是否准确定位仍需真机确认。原 MD 文件卡片的目录跳转继续使用非空文件校验。
- “滚动设置”的菜单选中项直接控制联动，不再保留隐藏复选框作为第二份状态。默认“同步滚动”：左右预览均就绪时，任意一侧滚动带动另一侧，优先按 PDF 页码对齐，无页码时按总进度同步；切回同步时按当前 PDF 位置对齐一次。“单向滚动”：只滚动当前操作区域，不向另一侧转发位置。更换文件、恢复任务和切换查看方式沿用当前滚动设置。本次接线仅做格式检查，切换效果仍需设备验收。

## 沙箱文件路径

鸿蒙根目录取自 `getApplicationContext().filesDir`；Windows/macOS 使用 Qt 的 `AppDataLocation`。设备实际物理映射仍以运行时为准，以下仅展示 `files/` 下的规则：

```text
files/
├── preview/
│   └── 20261024_173015_报告/
│       └── 报告.pdf
├── edit/
│   └── 20261024_173015_报告/
│       ├── input/
│       │   └── 报告.docx
│       └── output/
│           └── 报告.docx
└── convert/
    └── 20261024_173015_报告/
        ├── input/
        │   └── 报告.pdf
        └── output/
            └── 报告.md
```

- 任务目录名使用设备本地时间 `yyyyMMdd_HHmmss_文件名`（不含扩展名）；同秒同名以 ` (2)`、` (3)` 区分，输入输出共用此目录。普通文件名保留原样；非法路径字符拒绝，跨平台文件名特殊字符替换为下划线，过长文件名明确报错；仅目录中的文件名部分按 UTF-8 长度截短。
- 预览直接保存文件；编辑、转化创建 `input/` 和 `output/`。导入时仅生成输入副本及预定输出路径，输出文件在实际保存或转化时生成。失败重试沿用当前任务目录，重新选取文件创建新目录。
- 目录名时间表示创建时间，任务目录的修改时间表示最近使用时间。导入完成、PDF 成功打开、转换开始及结束都会显式更新任务目录修改时间；时间写入失败记录 `HUIWEN_SANDBOX stage=touch_failed`，不会把已经成功的转换改判失败。扫描列表和查询缓存本身不更新时间，也不修改目录名。
- 最近任务只扫描三类目录的一层任务子目录，合并后按 `lastUsedAt`（毫秒时间戳）降序，时间相同时按任务目录名排序。跳过无输入、失效目录、越界符号链接或不可读缓存；旧目录继续支持输出定位，但不伪造旧任务记录。
- 按文件名查询编辑缓存会返回全部同名候选，按最近使用排序。`openPath` 优先指向已有、可读的 `output/原文件名`，尚无输出时指向 `input/原文件名`；输出已存在但无效时跳过该候选。调用页面可对唯一结果直接打开、多个结果展示选择，成功打开或保存后再标记使用。

| ArkTS 入口（`utils/DocumentStore.ets`） | 用途 |
| --- | --- |
| `pickDocumentToSandbox(context, kind, DocumentMode.Preview|Edit|Convert)` | 系统选取、授权读取并按类型归档；取消不创建任务 |
| `getRecentDocuments(context, limit = 5, mode = 'all')` | 后台读取最近任务，数量为 1–100；可筛选任一类型 |
| `findEditCaches(context, name)` | 按原文件名查找所有同名编辑缓存，返回候选与 `openPath` |
| `markDocumentUsed(context, taskPath)` | 未来预览/编辑页面在实际打开或保存后更新时间 |

这些接口通过 NAPI 调用同一份 `SandboxStore`，返回名称、格式、模式、`taskPath`、`inputPath`、`outputPath`、`openPath` 和 `lastUsedAt`。Qt 侧可直接使用 `SandboxStore::recentDocuments()`、`findEditDocuments()`、`markUsed()`。首页已接最近工作区列表；同名候选选择 UI 和独立编辑工作区仍未接入，首页预览和编辑入口仍只展示格式选项。

## 首页最近工作区

- 右侧主区顶部保留“开始使用”和三个模式入口，顶部留白 44vp；提示文案为“请选择转化模式”，与三个按钮区域的间隔为 84vp（原 24vp 间距加按钮区顶部 60vp）。上区使用普通 `Column` 按内容高度排列，不使用 `Scroll` 或纵向 `layoutWeight(1)`；父容器顶部排列且间距为 0，最近工作区通过自身顶部外边距与上区保持 50vp 距离，不再贴底。16vp 标题在记录滚动区外固定显示；窗口大小及左侧介绍区比例不变。
- `RecentWorkspaceService.listRecentWorkspaces(context, limit = DEFAULT_RECENT_WORKSPACE_LIMIT)` 默认取三条，数量常量为 `3`，底层仍支持 1–100。扫描 `preview/edit/convert` 三类任务，按目录修改时间降序排列，不创建 JSON、数据库或新任务目录。
- 每行高度为 48vp，模式图标底框 28vp、内部 SVG 18vp；文件名、格式、时间字号分别为 14、12、11，加载、空列表及失败提示为 12。格式列为 104vp、时间列为 152vp，格式与时间间隔 8vp。文件名使用 `layoutWeight(1)` 占满剩余宽度，单行中部省略，鼠标悬停显示全名；固定列不参与收缩。时间显示设备本地 `yyyy-MM-dd HH:mm`，使用灰色文字。
- 仅记录区域使用 `Scroll`，标题不进入滚动容器；列表高度按条数计算并以 `maxListHeight` 限制，默认最大 240vp，可由调用方传参调整。默认三条时高度为 160vp，单条为 48vp，不补空行；更多记录超过最大高度后在内部滚动。
- 行点击直接通过 Want 的 `huiwenTaskPath` 传任务标识，跳过格式弹窗和选择器。首页先检查任务，Qt Ability 再经异步 `harmonyLoadTask(filesDir, taskPath)` 调用 `SandboxStore::loadTask()` 校验；只允许已接入的 PDF→MD，重新读取原输入和预定输出路径，不信任页面缓存的文件路径。其他模式显示对应图标和标签，但暂不开放恢复。
- `SandboxService::restorationRequested()` 通知 `ConversionWindow::restoreDocument()`；窗口正在选择或转换时拒绝替换，原 PDF 失效时也不覆盖当前任务。恢复时切到预览，加载原 PDF 和非空的已有 MD，并隐藏转化按钮；MD 不存在或为空时继续显示等待提示并保留按钮，不自动重新转换。
- Qt 载入后调用 `SandboxService::finishRestoration()` 更新时间，通过 `restoreResult` 回执通知 Qt Ability，成功后才发出首页关闭通知。读取列表、恢复前校验及恢复失败不更新时间；目录名的创建时间不变。新启动及 `onNewWant` 复用窗口都走同一恢复链。日志前缀为 `HUIWEN_QT_RESTORE_OK` / `HUIWEN_QT_RESTORE_FAILED`；系统实际交接与显示仍需真机验收。
- 首页查询和恢复前校验可能先于 `QApplication` 执行；任务命名校验使用字符检查，PDF 预读使用 C++ 标准文件流，避免在 NAPI 工作线程通过 `QFile`（`QObject`）或正则 JIT 提前初始化 Qt 线程数据，导致工作区鼠标事件触发主线程断言。
- 启动顺序回归检查独立于 PDFium：`cmake -S qt_app/tests/sandbox_startup -B /tmp/huiwen-sandbox-startup -DCMAKE_PREFIX_PATH=<宿主Qt5路径>`，然后 `cmake --build /tmp/huiwen-sandbox-startup`，最后 `QT_QPA_PLATFORM=offscreen /tmp/huiwen-sandbox-startup/sandbox_startup_check`。检查先在线程中查询、校验任务，再创建 `QApplication` 并分发鼠标事件；它不能替代鸿蒙真机恢复验收。

当前 Qt PDF→MD 选择链改为传递 `DocumentMode.Convert`，使用 `convert/<任务>/input` 与 `output`。转换成功才展示 MD 卡片，扫描件仍按原有错误提示处理；Python 继续接收具体输入、输出路径。点击 MD 卡片仍通过 `openLink('filemanager://openDirectory')` 传入校验后的结果 URI，打开真实输出目录，不创建应用内文件浏览器。

## 应用图标

统一使用 `.native-tools/design/huiwen-icon-rounded-final.png` 的最终版图标，原图为 1024×1024 PNG，保留透明圆角。正式资源直接随仓库保存，构建使用以下文件：

- `AppScope/resources/base/media/app_icon.png`：应用级图标，由 `AppScope/app.json5` 引用。
- `entry/src/main/resources/base/media/app_icon.png`：ArkTS 首页与 Qt 工作区 Ability 图标，以及各自的启动页图标。
- `qt_app/resources/branding/app_icon.png`：Qt 窗口图标，通过现有 QRC 注册为 `:/branding/app_icon.png`，在 `main.cpp` 中统一设置。

三份资源均直接复制最终版 PNG；更新品牌图标时同步替换这三份文件。

## 本地构建

工程当前配置为 HarmonyOS PC、`arm64-v8a`，目标 SDK 为 `6.1.1(24)`。首次克隆后，将 `build-profile.example.json5` 复制为本机的 `build-profile.json5`，需要安装到设备时再在 DevEco Studio 配置签名。

macOS ARM64 开发机首次 Run 会按 `third_party/prebuilt/manifest.json` 下载、校验并恢复固定的
Qt/CEF `.so`、头文件、CMake 配置、宿主工具和浏览器核心资源；后续 Run 校验本地文件后复用。
下载地址对应的依赖包须先发布，离线机器可通过 `HUIWEN_PREBUILT_ARCHIVE` 指定同一压缩包。
包哈希或本地依赖不匹配会停止构建，不覆盖已修改的库。详见 [固定依赖包](third_party/prebuilt/README.md)。

维护者准备或升级 Qt SDK 时，仍可导入同一套鸿蒙 Qt 5.15.12 开发文件和五个运行库：

```sh
python3 packaging/ohos/prepare_qt.py \
  --source /绝对路径/Qt5.15.12-ohos17-arm64-v8a \
  --license-source /对应的/qtbase源码目录
```

`--source` 也可由原有 `qt.sdk.dir` 或 `QT_PREFIX` 指定，但它们只用于准备/升级依赖，正常 CMake 构建使用项目内 `third_party/qt/`。库的唯一部署位置是 `entry/libs/arm64-v8a/`，Qt 开发包内通过相对符号链接引用；宿主工具必须匹配构建电脑。修改 Qt 内核后需先编译新的 SDK，再执行准备命令同步整套依赖。

PDFium fork 为 `xristosstarnetgames439-del/pdfium` 的 `huiwen-dev` 分支，首次同步提交记录官方源码 SHA；主项目固定子模块提交。首次构建会准备 DEPS 锁定的 Git 依赖、GN 和 Clang，之后由 Ninja 增量编译当前工作树，不重置用户的源码修改：

```sh
git submodule update --init third_party/pdfium
python3 packaging/ohos/build_pdfium.py --sync
```

脚本使用鸿蒙 NDK sysroot、归档器和链接驱动，生成 `aarch64-linux-ohos` 动态库；内部 libc++ 静态链接，关闭 V8、XFA、Skia 和 PartitionAlloc，保留公开渲染、编辑及保存接口。通过 ELF 架构、必要导出和动态依赖检查后，原子更新 `entry/libs/arm64-v8a/libpdfium.so`，同时生成源码/构建参数/校验值记录及分发许可材料。构建失败不会覆盖有效旧库，但会阻止新 HAP 生成。可设置 `OHOS_NDK_HOME` 指向 `native/`。

DevEco Run 的顺序为 `PreBuild → PrepareNativeDependencies（恢复固定 Qt/CEF，再增量构建 PDFium）→ PrepareMarkdownResources → ConfigureCmake / CompileResource → BuildNativeWithNinja → ProcessLibs → PackageHap → RepackHapWithHnp → SignHap`。PDFium 编译和 Qt/NAPI 库同步在收集原生库之前完成；未改动的源码复用产物。这里只实现运行构建时更新，没有文件监听器，也没有在运行进程内卸载和替换 C++ 库。

Markdown 另由 `PrepareMarkdownResources` 在 `PreBuild` 后、`ConfigureCmake`、`CompileResource` 和 `PackageHap` 前准备。
同一资源准备任务还使用宿主 Python 执行 `build_python_bundle.py`：收集当前转换代码与许可、应用补丁、固定 zip 成员顺序/时间/权限；新旧 pyz 字节相同则输出 `[HUIWEN_PY_BUNDLE] CACHED` 并保留时间戳，否则原子替换并输出 `GENERATED`。不复制运行环境或目标 wheel，也不将本机字节码和原生扩展放入 pyz。
它复用 Hvigor 的 Node/npm，首次 Run 联网按锁文件自动安装依赖，之后每次构建并比对
`entry/src/main/resources/rawfile/markdown/` 下的 JS/CSS、页面、许可证和校验清单；仅原子替换内容变化的文件，其余保留时间戳，避免 Qt RCC 无意义重编。全部相同时输出 `[HUIWEN_MD] CACHED`，有更新时输出 `GENERATED`，资源随 HAP 收集；
安装或构建失败会停止打包。通用预览模板还经 Qt qrc 编入工作区动态库，直接接收 MD 原文，
不为每篇 MD 另存 HTML。CEF 运行库使用项目内预编译版本，仅其包装层参与 C++ 编译；
轻量 HAR 模块已固定在 `third_party/cef/web_engine/`，OHPM 首次同步无需等候大依赖下载。
Python HNP 的运行时路径仍按下文配置，本次 Qt/CEF 自动准备不替代该配置。详见 [Markdown 构建说明](third_party/markdown/README.md)
和 [CEF 环境说明](third_party/cef/README.md)。

需要核对更新链路时，可运行 `python3 packaging/ohos/check_run_update.py`。它临时向 PDFium 加入两版构建探针，分别执行 HAP 构建并核对 `libs` 与 HAP 中的实际库内容，最后移除探针并重建最终包；不会安装或启动设备应用。该检查需要已配置可用的 HAP 签名，也会执行原有 Python HNP 打包。

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

再取得鸿蒙 arm64 Python 3.14 的已解压安装目录，要求包含 `bin/python3` 和 `lib/python3.14/site-packages/`。原有自检使用 [Harmonybrew 的预编译 Python 3.14.6](https://github.com/Harmonybrew/ohos-python/releases/tag/3.14.6) 通过。在被 Git 忽略的 `local.properties` 中配置本机路径：

```properties
python.runtime.dir=/绝对路径/python-3.14.6-ohos-arm64
# 可选：指定本机带 pip 的 Python，避免 DevEco 的 PATH 找不到解释器。
python.host.path=/绝对路径/宿主/python3
```

也可用 `HUIWEN_PYTHON_RUNTIME` 和 `HOST_PYTHON` 环境变量覆盖上述配置。相对配置路径按项目根目录解析。正常点击 DevEco Run 并执行 HAP 构建安装时，签名前的 `RepackHapWithHnp` 任务先检查 HNP 输入，未变化时复用成品；首次准备、输入变化或产物缺失/校验不符时重新生成。仅热重载不保证执行此任务。也可手动执行同一检查和打包流程：

```sh
python3 packaging/ohos/build_python_bundle.py
bash packaging/ohos/pack_python_hnp.sh /绝对路径/python-3.14.6-ohos-arm64
```

脚本使用本机 Python 的 pip 下载 `requirements-pdf.txt` 中锁定的鸿蒙 arm64/musl wheel。目标依赖按清单 SHA-256 缓存在被 Git 忽略的 `.hnp-cache/`，清单变化时重新安装。环境成品指纹保存在 `.hnp-cache/hnp-build.sha256`，只覆盖运行时目录、目标依赖清单、自检脚本、`hnp.json`、环境打包脚本和 `hnpcli`；按文件内容、路径、权限及符号链接目标计算，并校验已有 HNP 的 SHA-256。汇文 API、MarkItDown、转换入口和业务补丁的修改只影响独立 pyz；ArkTS、Qt 和 PDFium 修改也不会使环境 HNP 失效。

需要重建时只复制运行时与目标依赖，清除运行时目录中可能残留的汇文 API、MarkItDown 和旧转换入口，再用 SDK 的 `hnpcli` 打包；成功后原子替换 `hnp/arm64-v8a/huiwen_python.hnp`，再写入新指纹。任一步失败仍停止构建。删除 `.hnp-cache/hnp-build.sha256` 或成品 HNP 可强制重打包；删除整个 `.hnp-cache/` 可连同目标依赖一起重新准备。首次迁移到环境/业务拆分会重建一次 HNP，之后仅修改业务代码不会重建环境。若 SDK 工具不在默认位置，可设置 `HNPCLI=/绝对路径/hnpcli`。HNP 的 `name/version` 固定为 `huiwen_python/1.0.0`。

MarkItDown 官方源码保持在子模块的 `v0.1.8` 提交；它采用 MIT 许可，可修改并商用，但分发时需保留许可与第三方声明。其官方包强制依赖 Magika/ONNX Runtime，而当前目标架构无法直接解析 ONNX Runtime wheel。仓库中的补丁只让 Magika 可选；本阶段输入已由选择器限定为 `.pdf`，按扩展名交给 PDF 转换器。补丁不改动子模块原始源码。pip 能解析目标架构 wheel 只说明可获取包，不代表鸿蒙设备能加载其中的原生 `.so`；需在设备上完成导入及实际 PDF 转换验收。

Hvigor 的默认 `PackageHap` 不会自动带入根目录的 HNP。`entry/hvigorfile.ts` 在 `PackageHap` 后、`SignHap` 前先准备 HNP，再调用 SDK `app_packing_tool.jar --hnp-path` 重打未签名 HAP，并检查环境 HNP 和 `resources/rawfile/python/huiwen_converter.pyz` 均存在；复用环境不跳过注入和签名。构建日志中的 `[HUIWEN_HNP] CACHED/GENERATED` 与 `[HUIWEN_PY_BUNDLE] CACHED/GENERATED` 分别表示两个产物的状态。SDK 不在默认位置时设置 `DEVECO_SDK_HOME` 或 `OHOS_SDK_HOME`。设备日志筛选 `HuiwenPython`：`PYTHON_BUNDLE` 显示缓存业务包哈希及路径，`PYTHON_START` 显示 HNP 解释器与当前 pyz，`RUNTIME` 显示实际导入模块及算法标识。

本次拆分仅做语法和格式检查，未执行构建、打包或设备运行。后续验收：首次迁移允许环境 HNP 和业务 pyz 各生成一次；原样复跑应同时命中缓存；只改 Python API 或 MarkItDown 时应仅见业务包 `GENERATED`、环境 `CACHED`；改目标依赖清单才应重建环境。安装后确认 `PYTHON_START` 的脚本来自缓存 pyz，实际完成一次 PDF 转换；可再运行 `packaging/ohos/check_workspace_hap.py <signed.hap>` 检查 pyz 入口和许可。对比构建耗时，安装和无线传输另行计量；HAP 仍携带完整环境，并非设备差量安装。

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
- 不提交 `entry/libs/` 和 `third_party/qt/` 中的本机开发依赖；NAPI、Qt 应用与 PDFium `.so` 按源码增量生成，Qt 运行库由准备脚本导入。主仓库只固定 PDFium 子模块提交，fork 修改在其独立仓库维护。

## ArkTS 首页功能

- 窗口大小和窄侧栏保持原有设置。除侧栏、外边距及分区间距外，介绍区与主区域使用 `layoutWeight(3)` / `layoutWeight(7)` 分配宽度，主区域取消固定 360vp 宽度。
- “开始使用”下三个入口保持等宽列布局，浅灰正方形边长为所在列宽的 50%，即上一版方块边长的一半；方块内只放居中的 SVG，文档预览、文档编辑、文档转化名称以普通字重显示在方块外下方。`HomeFeatureCard` 只负责显示和点击事件，`HomePage` 保存当前功能选项。
- 点击入口后，按 `docs/自定义弹窗模版.md` 使用 `getUIContext().getPromptAction().openCustomDialog()` 打开居中的独立格式选择子窗口，采用 `showInSubWindow: true`、`isModal: false` 的组合（该接口禁止子窗口同时启用模态，也不支持 `CustomDialogController` 的 `customStyle` 参数）；首页保存返回的弹窗 ID，通过 `closeCustomDialog(id)` 统一关闭，并处理打开失败、外部取消及首页退出期间的异步清理。系统背板与内容组件 `HomeFormatDialog` 共用 `HOME_FORMAT_DIALOG_WIDTH = 360`，内外宽度统一固定为 360vp，不再使用 90% 限制；高度随内容。只由系统背板绘制浅灰背景、16vp 圆角和阴影，关闭背景模糊，内部保持透明，避免宽度不一致留下透明空白。三个弹窗共用内容布局，内容居中；左右及底部内边距为 24vp，顶部为 40vp，为右上角关闭按钮留位；格式选项自身内边距为 12vp，转换入口使用透明背景，不再额外叠加白底和边框。弹窗不在首页内展开或预留面板高度，右上角仅保留 28×28vp 关闭按钮，内含 18×18vp 的 × SVG，也可点击外部取消。预览和编辑窗口仅展示 Word、PDF、MD，暂不选择文件或跳转；转化窗口展示 PDF→MD 组合图标入口。点击该入口后由 `HomeFormatDialog.onConvertRequested` 通知首页，关闭子窗口成功后再启动现有 Qt 工作区，仍在 Qt 窗口就绪后结束首页 Ability。启动期间禁用重复操作，失败显示错误。
- `HomeFormatDialog` 通过 `title` 参数接收首页传入的标题，预览为“请选择要预览的格式”，编辑为“请选择要编辑的格式”，转化为“请选择要转化的格式”。标题以 18vp 普通字重显示在内容左上方，与格式选项间隔 16vp；保留右上角关闭按钮、360vp 内外统一宽度及原有背景设置。
- 功能 SVG 使用 Feather `book-open`、`edit-3`、`repeat`，转化方向使用 `arrow-right`，弹窗关闭使用 `x`，许可随 `rawfile/licenses/feather-icons-LICENSE.txt` 分发。格式 PNG 原样复用 Qt 已有图标，来源见 [文件格式图标说明](qt_app/resources/file-icons/README.md)。所有图标从应用资源加载，无需运行时下载。

## ArkTS 首页侧栏

- 侧栏固定在启动首页左侧，宽度 60vp；底部仅一个三横线按钮，尺寸 36×36vp，距底部 20vp。图标为 `resources/base/media/sidebar_menu.svg`，来自 [Feather Menu](https://github.com/feathericons/feather/blob/main/icons/menu.svg)，保留三横线几何形状，颜色跟随首页外观，以 24×24vp 的 `Image` 显示；[上游 MIT 许可](entry/src/main/resources/rawfile/licenses/feather-icons-LICENSE.txt) 随 rawfile 分发。按钮保留零内边距，不依赖字符字体或文本省略行为。功能卡片显示在侧栏右侧的主区域。
- `EntryAbility` 在页面加载后，先设置 `setWindowBackgroundColor('#00000000')`，再独立设置 `setWindowContainerColor('#00000000', '#FFF5F7FB')`；后者需要 `ohos.permission.SET_WINDOW_TRANSPARENT`，已在模块中声明。焦点态容器透明，非焦点态使用不透明底色。两个步骤各自处理异常，设置失败时保留必要错误日志，不阻断首页使用。根布局透明，侧栏背景为 `#fae5e9ec`（约 98% 不透明度），主内容背景仍为实色；只调整背景透明度，按钮保持清晰，不额外增加模糊效果。
- 侧栏组件通过 `FunctionSidebar.onMenuRequested` 打开首页菜单，并通过 `onMenuAnchorChanged` 同步按钮锚点；Qt 转换工作区不增加侧栏。

## Qt 工作区验收

本阶段页面入口为 `qt_app/src/workspace/ConversionWindow.cpp`。未创建后续侧栏目录；新增侧栏时使用 `sidebars/`，避免与输入/输出区域混淆。

1. 点击 ArkTS 首页“文档转化”，再点击独立格式选择窗口中的“PDF → MD”，格式子窗口关闭后打开 Qt；Qt 默认最大化并保留系统标题栏、窗口边框及窗口控制按钮。查看 `QtWorkspace` 标签下的 `HUIWEN_QT_START`、`HUIWEN_QT_READY`，确认只有 `EntryAbility` 随后结束，`QtWorkspaceAbility` 保持运行；不应再出现 `newLocalStorage not found`、`Need object`。
2. 左右文档区域占满状态栏上方的可用高度，底部状态栏保持紧凑；还原窗口并调整大小时布局应同步变化。悬停中间细线出现水平分割光标；拖动改变两侧宽度，底部小型“转化”按钮保持居中在线上。
3. 进入工作区应默认选中“文件 → 查看模式 → 预览模式”，业务仍是“转化模式”。点击左侧虚线加号选择 PDF，默认直接显示 PDF 预览；手动切到占位图时显示文件卡片。拖动分隔线或缩放窗口后，页面应按当前可用宽度重排。“文件 → 切换模式 → 转化模式 → 更换文件”应保留查看方式、清空旧 MD、恢复转化按钮，预览方式下从新 PDF 第一页开始。取消选择应保留原文件、MD、查看方式及按钮状态。选择或转换期间不得重复更换；损坏、加密文件应提示原因。两侧不再显示旧工具按钮。
4. 点击“转化”，操作期间不能重复选择或提交。即使转换前处于占位图模式，成功后也自动切到预览并更新菜单蓝点，直接展示 MD、隐藏转化按钮，底部显示 `code=0`、成功提示和完整路径。失败显示错误码及处理建议，保留“重新转化”按钮；重试成功后同样隐藏。选择新文件后按钮恢复，取消选择保留原状态；切换查看方式、拖动分隔线及缩放窗口不能重新显示已有结果的转化按钮。
5. 转换成功后底部显示完整 MD 路径；切到占位图模式后点击 MD 卡片，沿用系统文件管理器打开文件夹，文件夹内能看到真实 MD。不存在、为空或不可读取的结果应在跳转前明确报错，不能误报转换成功。
6. 日志搜索 `HUIWEN_OUTPUT`：`import_success` 为导入落盘路径，`convert_start` 为输入/输出路径，`verified` 为磁盘验证及字节数，`convert_success` 为成功通知，`locate_start` 为点击时重新检查，`locate_requested` 为鸿蒙桥接已提交。系统跳转结果继续查看原有 `HUIWEN_QT_OPEN_REQUEST`、`HUIWEN_QT_OPEN_REQUEST_ACCEPTED` 或 `HUIWEN_QT_OPEN_FAILED`。
7. 新导入生成 `convert/yyyyMMdd_HHmmss_文件名/input/原文件.pdf` 与 `output/原文件.md`；同秒同名只对任务目录追加编号，文件名保持一致。首页默认展示最近三条记录，长文件名中部省略且不挤压固定列；点击应复用原任务、载入已有结果，不弹选择器、不新建目录。检查成功恢复、PDF 打开、转换开始及结束后任务目录修改时间会更新；查询和失败不更新时间。分别验证全新 Qt 窗口、已有窗口、转换中拒绝替换、PDF 被删除、尚无 MD 的任务。同一任务重试路径不变，历史 `outputs/md/` 继续支持原有目录跳转。
8. 从最近工作区进入，应直接预览 PDF 与已有非空 MD，转化按钮隐藏；MD 不存在或为空时显示等待提示并保留按钮。点击系统标题栏关闭应回到首页并结束工作区 Ability。连续重复进入和关闭，包含 MD 仍在加载时关闭，确认无残留白窗、整体退出或 CEF 失效；每次进入仍最大化，首页在工作区载入成功后结束。

Python 固定入口在 stdout 最后一行返回 `HUIWEN_CONVERT_RESULT {"code":0,"message":"转化成功，Markdown 文件已生成。","outputPath":"/沙箱/convert/时间戳_文件名/output/文件.md"}`。失败时 `code` 非零、`outputPath` 为空、进程退出码为 1；详细异常写入日志，界面展示中文原因和处理建议。C++ 独立保存完整结果行，日志尾部缓存仍限制为 4096 字节，结果不受日志截断影响。成功仍保留 `HUIWEN_CONVERT_OK` 标记，原 ArkTS 接口继续成功返回路径、失败拒绝 Promise。

错误码：`1001` 参数或格式错误，`1002` 输入不存在，`1003` 加密或禁止提取，`1004` PDF 损坏或不支持，`1005` 无可提取文字（暂不支持 OCR），`1006` 读写权限不足，`1007` 存储不足，`1008` 组件缺失，`1099` 其他转换异常；`2001` 原生进程或结果协议异常，`2002` 输出文件校验失败。C++ 同时验证字段类型、退出码、预期输出路径及真实非空文件，不能仅凭成功字段通知界面。

HAP 包结构校验：

```sh
python3 packaging/ohos/check_workspace_hap.py entry/build/default/outputs/default/entry-default-signed.hap
```

Windows/macOS 的同一 Widgets 源码提供桌面入口，尚未编译验证和制作安装包；CMake 需通过 `PDFIUM_LIBRARY` 指定与宿主架构匹配的 PDFium 动态库。桌面转换需安装同一 Python 依赖、设置 `PYTHONPATH` 为仓库的 `converter/`，`HUIWEN_CONVERT_SCRIPT` 为 `converter/pdf2md/convert.py` 的绝对路径；可用 `HUIWEN_PYTHON` 指定解释器。

PDF 预览桌面回归检查（独立工程，不加入 HAP；需要桌面 Qt 5.15 和对应宿主架构 PDFium）：

```sh
cmake -S qt_app/tests -B /tmp/huiwen-preview-check \
  -DCMAKE_PREFIX_PATH=/桌面Qt目录 -DPDFIUM_LIBRARY=/本机PDFium动态库
cmake --build /tmp/huiwen-preview-check
QT_QPA_PLATFORM=offscreen /tmp/huiwen-preview-check/pdf_preview_check
```

该检查生成两页彩色 PDF，验证导入后按钮、真实页面像素、多页滚动、调整宽度、更换文档与忙碌状态。鸿蒙 C++ 编译及 HAP 打包已通过。
