# DOC → DOCX 核心与预览业务流程

本模块负责把旧版二进制 DOC 静默转换为 DOCX，供已有 Word 只读预览器使用。
转换核心采用纯 Python 标准库实现，不调用 Office、LibreOffice 或浏览器。

## 源码与维护边界

- 自有 fork：[xristosstarnetgames439-del/doc2docx](https://github.com/xristosstarnetgames439-del/doc2docx)。
- 上游：[HuiTurn/doc2docx](https://github.com/HuiTurn/doc2docx)，MIT 许可，分发时保留 LICENSE。
- 当前版本：`0.36.6`，固定提交 `47f64ec103c4abf2c576fb0343a1abbeac3931d7`，位于 [source/](source/)。
- 核心修改在 `source/` 子模块提交、推送到自己的 fork，再由主项目更新子模块指针。
- 本说明、转换适配、Qt 业务代码和 HNP 打包脚本由主仓库维护；项目业务说明不写入上游核心 README。

克隆主项目后，在项目根目录初始化固定版本的全部第三方源码：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

## 用户操作与业务流程

```text
首页 → 文档预览 → Word → 单栏 Qt 工作区 → 选择文件 → 复制到沙箱任务目录
  ├─ DOCX：直接交给现有 DOCX 只读预览器
  └─ DOC ：显示“正在转换 DOC，请稍候…”并暂停重复选取
           → 后台生成临时 DOCX → 校验成功 → 交给同一预览器
           → 更换文件／关闭工作区时释放临时副本
```

选择器取消时保留原有文档。DOC 转换失败时清除加载提示、解除忙碌状态并显示错误；
保留原始 DOC 归档，可重新选择或从最近任务重开。
查看模式仍沿用工作区的预览／占位图设置，不增加专门的转换弹窗。

| 环节 | 处理内容 | 主项目文件／核心库 |
| --- | --- | --- |
| 打开工作区 | 首页按预览模式和 Word 类型启动 Qt Ability | [HomePage.ets](../../entry/src/main/ets/pages/HomePage.ets)、[QtWorkspaceAbility.ets](../../entry/src/main/ets/entryability/QtWorkspaceAbility.ets) |
| 选择文件 | Qt 发起选取请求，ArkTS 打开系统选择器，允许 `.doc`／`.docx` | [SandboxService.cpp](../../qt_app/src/sandbox/SandboxService.cpp)、[DocumentStore.ets](../../entry/src/main/ets/utils/DocumentStore.ets) |
| 导入沙箱 | 读取授权文件句柄，复制到任务目录；失败时撤销未完成导入 | [harmony_sandbox.cpp](../../qt_app/src/platform/harmony/harmony_sandbox.cpp)、[SandboxStore.cpp](../../qt_app/src/sandbox/SandboxStore.cpp) |
| 格式分流与加载 | DOC 显示加载提示并禁用重复选取；DOCX 直接进入预览 | [ConversionWindow.cpp](../../qt_app/src/workspace/ConversionWindow.cpp)、[DocumentView.cpp](../../qt_app/src/documentView/DocumentView.cpp) |
| 后台转换调度 | `ConversionService::convertDocToDocx()` 使用 QtConcurrent，创建任务内临时目录并调用原生转换函数 | [ConversionService.cpp](../../qt_app/src/services/ConversionService.cpp) |
| 启动转换进程 | `RunDocConversion()` 定位两个私有 HNP，用已有 Python 执行固定 `convert_word.pyz`，读取结果协议 | [python_runner.cpp](../../entry/src/main/cpp/python_runner.cpp) |
| 实际解析和生成 | 固定入口调用 `doc_to_docx()`，适配层校验输入、调用 `doc2docx.convert()` 并校验 DOCX 后发布 | [convert.py](../../converter/word2docx/convert.py)、[_word_adapter.py](../../converter/huiwen_converter/_word_adapter.py)、[核心 converter.py](source/src/doc2docx/converter.py) |
| 展示正文 | Qt 读取 DOCX 字节，经 CEF 页面交给 PortOne 的 `readOnly` 模式显示 | [WordPreviewWidget.h](../../qt_app/src/documentView/renderers/word/WordPreviewWidget.h)、[CefDocumentPreviewWidget.cpp](../../qt_app/src/documentView/renderers/web/CefDocumentPreviewWidget.cpp)、[Word 页面入口](../word/library.ts)、[PortOne 核心](../word/source/) |
| 成功／失败收尾 | 成功后替换加载提示；失败后解除忙碌状态并显示错误；恢复任务等待转换结果后回执 | [ConversionWindow.cpp](../../qt_app/src/workspace/ConversionWindow.cpp) |
| 清理临时副本 | `releaseDocPreview()` 释放目录；工作区销毁时释放服务持有的目录；后台任务持有自己的共享引用直到结束 | [ConversionService.cpp](../../qt_app/src/services/ConversionService.cpp)、[ConversionService.h](../../qt_app/src/services/ConversionService.h) |
| 最近任务恢复 | 列出并校验原始 DOC／DOCX 任务；DOC 重开时重新转换，不依赖上次临时副本 | [RecentWorkspaceService.ets](../../entry/src/main/ets/services/RecentWorkspaceService.ets)、[SandboxStore.cpp](../../qt_app/src/sandbox/SandboxStore.cpp)、[ConversionWindow.cpp](../../qt_app/src/workspace/ConversionWindow.cpp) |

当前界面由 Qt 直接调用 `RunDocConversion()`；没有额外的 ArkTS DOC 转换桥接。
Python 的稳定公开接口 `huiwen_converter.doc_to_docx()` 保留，供源码入口及自检使用。

## 文件位置与清理

预览任务中的目录关系如下，具体任务名由 `SandboxStore` 生成：

```text
files/preview/<任务目录>/
  原文件.doc                         保留的沙箱原始输入
  .word-preview-<随机字符>/           Qt 管理的临时目录
    preview.docx                     转换成功后供预览使用
    .docx-<随机字符>/                转换过程的内部暂存目录，返回前自动清理
```

两个临时目录承担不同生命周期：`.docx-*` 负责转换暂存、校验及原子发布；
`.word-preview-*` 负责整个 DOC 预览期间的副本。
更换文件时先清理旧渲染内容，再释放旧临时目录；DOCX 直接预览时不生成该副本。
关闭期间若转换仍在执行，后台任务的共享引用防止目录提前被删，结束后再释放。
进程崩溃或被强制终止时不保证析构清理；最近任务只使用原始输入，重开时生成新副本。

## 依赖职责与 Run 打包

| 模块 | 职责与分发形式 |
| --- | --- |
| [doc2docx](source/) | 解析二进制 DOC、生成 DOCX；只收集 API 运行模块与 LICENSE，打入 `huiwen_doc2docx.hnp` |
| 已有 Python HNP | 提供鸿蒙 Python 解释器；转换包不复制第二套解释器或 PDF 依赖 |
| [PortOne](../word/source/) | 解析并展示 DOCX，由 [Word 构建入口](../word/build.mjs) 生成离线 JS／CSS／HTML 资源 |
| Qt／CEF | Qt 负责工作区、加载和后台任务；CEF 承载预览页面，继续使用现有固定 SDK／`.so` |

Run 的 [entry/hvigorfile.ts](../../entry/hvigorfile.ts) 执行以下步骤：

1. `PrepareNativeDependencies` 检查转换核心等子模块是否存在，缺少源码时停止并提示初始化命令。
2. `PrepareMarkdownResources` 调用 [pack_word_hnp.py](../../packaging/ohos/pack_word_hnp.py)，准备 `hnp/arm64-v8a/huiwen_doc2docx.hnp`；同时沿用原有 Word 离线资源构建。
3. HNP 打包排除上游 CLI、测试和开发脚本，收集项目固定入口、转换适配、核心 API 模块及许可证；按输入内容和成品摘要检查缓存，未变化时复用。
4. [module.json5](../../entry/src/main/module.json5) 声明 `huiwen_python.hnp` 与 `huiwen_doc2docx.hnp` 为私有包。
5. `RepackHapWithHnp` 在签名前将两个 HNP 收入 HAP，并检查转换包、原有 Python 业务包与 Word 资源。

转换核心以 Python 源码打包，不额外生成 DOC 转换 `.so`，也不重编 Qt／CEF 内核。
私有包的名称、版本由 [hnp-word.json](../../packaging/ohos/hnp-word.json) 固定。

## 边界与验证

- 输入、输出各限制 16 MiB；原始文件只读，已有输出拒绝覆盖。
- 真正的二进制 DOC 解析后生成 DOCX；内容其实为 DOCX、后缀误标为 DOC 的文件校验后复制。
- 加密、损坏或不支持的输入会返回错误；复杂绘图、旧版排版等保真限制由核心诊断报告记录到后台日志。
- 用户已确认当前 DOC 选取、加载、转换和预览基本链路可用；不能据此认定所有文档保真或关闭异常场景均已验收。
- 转换、加载状态、清理接口和输出校验均保留实际调用；临时副本不能替代原始 DOC 归档。

在项目根目录可手动执行以下命令；它们不连接设备：

```sh
python3 packaging/ohos/pack_word_hnp.py
python3 converter/word2docx/check_conversion.py
python3 packaging/ohos/check_word_hnp.py /绝对路径/样本.doc
```

HNP 自检在宿主机隔离执行包内代码，检查源码摘要、输出和原文件未改变；
未提供样本时只检查包结构，不等同于鸿蒙安装及运行验证。
源码直接调用及桌面调试见 [转换说明](../../converter/word2docx/README.md)。
