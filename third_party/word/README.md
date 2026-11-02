# PortOne Word 核心与项目构建适配

`source/` 是 <https://github.com/xristosstarnetgames439-del/docx-editor> 的源码子模块，
固定主项目记录的提交。PortOne 核心修改在子模块提交、推送到我们的 fork。
外层 `library.ts`、`build.mjs`、`check_build.mjs`、依赖锁文件及本说明属于主仓库。

首页“文档预览 → Word”进入单栏 Qt 工作区，选择 DOCX 后由 PortOne 的 `readOnly` 模式显示正文；
选择 DOC 时先显示加载提示，后台转成临时 DOCX 后复用同一预览器。
支持更换文件、占位图／预览切换、所在目录及 DOC／DOCX 最近任务恢复。
Word 和 Markdown 通过 `CefDocumentPreviewWidget` 共用 CEF 运行时、原生窗口定位与关闭流程。
库显示连续纸张和近似分页线；本阶段不开放编辑、批注修改、保存或手动格式转换。
上游 Apache-2.0 许可、第三方声明和实际打入包的依赖许可随资源分发。

## 克隆和 Run

在项目根目录执行：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

缺少源码时 Run 停止并提示该命令，不自动执行 Git 克隆或更新。
`PrepareMarkdownResources` 在 MD 构建后调用本目录 `build.mjs`，
构建失败阻断 HAP，签名前检查五个 Word 资源是否入包。

构建直接使用 `source/src/` 的 TypeScript 和 CSS，复用 DevEco Node／npm，
无需安装 pnpm、Node 22 或上游测试／站点工具。首次按锁文件准备独立的运行时依赖和 esbuild；
React 固定为 18.3.1，依赖完整时可离线构建。`showPreview()` 收到 DOCX 字节的 Base64 后才挂载只读实例，
`onReady`／导入错误通过 CEF 子进程消息回传，正文未就绪不会报告文档打开成功。
Qt 侧限制文件不超过 16 MiB，浏览器页面仅使用内联脚本／样式和文档内图片。

输出到 `entry/src/main/resources/rawfile/word/`：

- `huiwen-word.js`：编辑器、DOCX 接口、React 和 `createRoot`。
- `huiwen-word.css`：编辑器及 ProseMirror 样式。
- `preview.html`：内联 JS/CSS 的离线预览页，同时经 `word.qrc` 编入 Qt 工作区。
- `THIRD_PARTY_LICENSES.txt`：完整许可证／声明。
- `build.json`：源码、版本锁和产物的 SHA-256。

这些是 Web 资源，不生成额外 `.so`；每次 Run 构建，内容相同保留时间戳。

```sh
node third_party/word/build.mjs
node third_party/word/check_build.mjs
```

自检使用临时副本，不修改真实 fork。本次页面接入只做格式和静态检查，
尚未执行资源构建、C++ 编译、HAP 打包或设备运行。
验收需重新 Run，检查正文、表格／图片、窗口缩放、切换文档、占位图切换、错误提示及最近任务重开。

## 独立 DOC 转换接口

DOC → DOCX 接口位于 `converter/huiwen_converter`，说明见
[独立静默转换](../../converter/word2docx/README.md)。
该接口不修改本目录的 JS、资源构建、CEF 参数或现有 DOCX 预览流程。
真实 DOC 的解析／生成核心现已单独打入 `huiwen_doc2docx.hnp`，复用已有鸿蒙 Python；
Qt 工作区允许选择 DOC／DOCX。DOC 先显示加载提示，后台生成任务内临时 DOCX，再交给原有预览器；
DOCX 直接沿用原流程。临时副本在更换文件／关闭工作区时清理，最近任务保留 DOC 并在恢复时重新转换。
本目录 JS／CEF 渲染逻辑保持原样，设备安装与执行仍需自行 Run 验证。
