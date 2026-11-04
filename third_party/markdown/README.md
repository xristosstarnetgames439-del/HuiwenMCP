# Markdown 核心与主项目页面适配

`source/` 子模块来自 <https://github.com/xristosstarnetgames439-del/huiwen-markdown>，
统一维护 CodeMirror、Lezer Markdown、markdown-it 及富文本扩展组件。
版本、上游来源、许可证及原始文件校验值以子模块中的 `SOURCES.json`、
`UPSTREAM_SHA256.json` 和各组件许可文件为准，不再在主项目复制维护核心源码快照。

外层 `library.js`、`library.css`、`preview.html` 是汇文的页面与同步滚动适配，属于主仓库。
`build.mjs`、`check_build.mjs`、工具依赖及锁文件也属于主仓库。
修改核心代码在 `source/` 提交、推送；修改页面／桥接／构建适配在 HuiwenMCP 提交。

## 克隆和 Run

在项目根目录执行：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

缺少源码时 Run 提示并停止，不自动克隆、拉取最新版本或覆盖本地改动。
初始化后，`PrepareMarkdownResources` 每次调用 `build.mjs`，资源编译和 HAP 打包等待其成功。
构建复用 DevEco／Hvigor 的 Node 与 npm，首次按锁文件安装 esbuild，之后复用工具缓存。
运行时代码通过子模块 `aliases.mjs` 指向仓库内源码，包含传递依赖；不从 npm 安装另一个核心副本。

输出到 `entry/src/main/resources/rawfile/markdown/`：

- `huiwen-markdown.js`、`huiwen-markdown.css`：面向 Chromium 114 的离线浏览器资源。
- `preview.html`：内联资源的通用模板，经 Qt qrc 编入应用动态库。
- `THIRD_PARTY_LICENSES.txt`：根据子模块依赖清单收集的完整许可证。
- `build.json`：输入源码、来源清单和产物校验值。

每次重新构建，内容相同保留产物时间戳；失败不替换有效旧资源，并阻止后续打包。
Qt／CEF 继续使用固定 SDK／`.so`，不在此重编内核。

```sh
node third_party/markdown/build.mjs
node third_party/markdown/check_build.mjs
```

检查脚本在临时副本验证源码更新、失败阻断、页面渲染及同步滚动，不修改真实子模块。
检查阶段与设备验收应分别记录，宿主页面测试不代表鸿蒙运行结果。

## 富文本、页码和离线验证

核心解析和富文本增强在 `source/rich-markdown.ts`；`library.js` 只负责页面插入、代码复制、
PDF 页码布局和已有同步滚动接口。核心语法详见 `source/README.md`。

转换产物中的 `pdf-watermark` JSON 围栏显示在页面正文之前；`pdf-logo` 显示在 `--第N页--` 之后。
只有顶层连续递增的 PDF 页码标记创建页区；代码块中的相同字符串不产生页码。
图表、公式字体和图片改变高度时继续使用既有 ResizeObserver 恢复页内滚动位置。
Mermaid 异步任务串行处理，切换文档后旧任务不能覆盖新正文；无效流程图显示原文。

本地浏览器检查需要 Playwright 和 Chromium，可指定已经安装的路径，不用于设备运行：

```sh
node third_party/markdown/build.mjs
HUIWEN_PLAYWRIGHT=/absolute/path/to/playwright \
HUIWEN_CHROME=/absolute/path/to/chrome \
node third_party/markdown/check_preview.mjs
```

检查包含实际流程图／时序图、公式、代码高亮、表格换行、任务列表、脚注、页首水印、页码后 Logo、
恶意 HTML/链接、无效扩展、异步切页和页码滚动定位；拦截所有网络请求并要求零请求。
`check_build.mjs` 继续验证构建链与失败保持旧产物；`source/check.mjs` 验证独立核心构建。


## 同目录图片与损坏占位

新转换的 MD 使用 `![说明](图片文件名.png)`；图片和 MD 同在 output 目录，水印／Logo 扩展同样只保存相对路径。
`rich-markdown.ts` 解析路径，`library.js` 将它映射到浏览器内部地址 `https://huiwen-md.invalid/image?path=…`。
该地址由 `CefDocumentPreviewWidget.cpp` 的资源回调拦截读取本地文件，不启动服务器、也不发送网络请求。
每个 MD 预览客户端绑定当前文档目录；Word 客户端没有图片读取目录。只读取目录内最多 32 MiB 的
PNG／JPEG／WebP／GIF／BMP 文件，拒绝越界、外部链接及指向目录外的符号链接。

图片读取或解码失败时，在原图片位置显示破裂图片图标、说明与文件名；不影响正文或页码。
图片加载与占位造成的高度变化仍走已有 ResizeObserver，同步滚动接口不变。
旧 MD 内嵌图片保持可读；本次不自动改写历史文档或修改文件导出／删除业务。

`check_preview.mjs` 通过宿主拦截模拟原生读取，检查中文／空格路径、损坏、缺失、越界和页尾 Logo。
可设置 `HUIWEN_TEST_MARKDOWN=/absolute/path/to/document.md` 检查真实文档引用；
`HUIWEN_IMAGE_SCREENSHOT=/absolute/path/to/screenshot.png` 保存损坏占位截图。
本地浏览器模拟不等于鸿蒙 CEF 回调的设备验收。

## Markdown 字符与预览效果对照

本表描述当前预览器支持的输入语法。PDF 转换器只自动生成其中一部分；转换条件和字符输出详见
[MarkItDown 转换 README](../markitdown/source/README.md#汇文-pdf--markdown-格式对照)。
核心解析在 `source/rich-markdown.ts`，分页、图片失败占位、代码复制和滚动同步在 `library.js`。

| MD 中的字符／结构 | 预览效果 | PDF 转换器是否自动生成 |
| --- | --- | --- |
| 普通文字、段落空行 | 正文段落 | 是，按文字基线及段落规则提取 |
| `# 标题` 至 `###### 标题` | 一至六级标题 | 是，依据相对字号、长度和标点条件 |
| `**文字**` | 加粗 | 是，依据字体名称特征 |
| `*文字*` | 斜体 | 暂未依据 PDF 字形生成 |
| `~~文字~~` | 删除线 | 暂未自动识别 |
| `- 内容` | 无序列表 | 是，可归一化部分原项目符号 |
| `1. 内容` | 有序列表 | 保留标准编号时可渲染；`1、` 等不保证成为列表 |
| 管道符表格与 `---` 分隔行 | 表格 | 是，仅依据真实格线；输出首行作为表头 |
| `<br>` | 行内或表格单元格换行 | 是，主要用于表格单元格 |
| `<small>文字</small>` | 小字号文字 | 是，用于符合位置与字号条件的页脚 |
| 单反引号包裹文字 | 行内代码 | 暂未自动生成 |
| 反引号代码围栏，附语言名 | 代码框、语言标签、复制按钮；已知语言高亮 | 是，满足代码区域条件时生成；未知语言保留原文 |
| `> 内容` | 引用块 | 转换器用于 OCR 提示；普通引用语义未自动判断 |
| `[名称](https://example.com)` | 链接；普通网址也可自动识别 | 不恢复 PDF 链接注释 |
| `![说明](图片文件名.png)` | 读取同目录图片；失败显示破裂占位 | 是，涵盖嵌入图、扫描原图和版式核对图 |
| `- [ ] 待办`、`- [x] 完成` | 禁用交互的任务复选框 | 暂未自动识别 PDF 复选框 |
| `[^1]` 与 `[^1]: 注释` | 脚注、引用跳转 | 暂未自动建立脚注关系 |
| `==文字==` | 高亮 | 暂未依据 PDF 高亮自动生成 |
| `H~2~O`、`x^2^` | 下标、上标 | 暂未依据 PDF 字形位置自动生成 |
| 术语下一行以 `: ` 开头 | 定义列表 | 暂未自动判断 |
| `$公式$`、`$$公式$$` | 行内公式、块公式 | 暂不从 PDF 还原 LaTeX |
| `\(...\)`、`\[...\]` | 行内公式、显示公式 | 暂不从 PDF 还原 LaTeX |
| `math`／`latex` 代码围栏 | 块公式 | 暂不从 PDF 自动生成 |
| `mermaid` 代码围栏 | 流程图、时序图等 Mermaid 图表 | 暂不从 PDF 图形反推；转换保留图片 |
| `<!-- PDF page N -->` | 创建页区，页末显示 `--第N页--` | 是；源标记仍位于该页内容之前 |
| `pdf-watermark` JSON 围栏 | 页首水印提示，可展开 OCR 原图 | 是，倾斜文字层或符合条件的重复轮廓 OCR |
| `pdf-logo` JSON 围栏 | 当前页页码下方的重复图案 | 是，符合跨页重复等条件时生成 |

### 代码、公式、流程图字符示例

````markdown
```python
def hello():
    print("你好")
```

行内公式：$E=mc^2$

$$
\frac{a}{b}
$$

```mermaid
flowchart LR
  A[上传PDF] --> B[转换Markdown]
  B --> C[预览]
```
````

公式使用 KaTeX，代码高亮使用 highlight.js，图表使用 Mermaid；资源及字体离线打包。
Mermaid 语法错误保留源码；不支持的代码语言按普通代码显示。图表能渲染，不代表 PDF 中的
图形能自动变成上述 `mermaid` 源码；公式同理。

### 水印、Logo 和分页字符示例

````markdown
<!-- PDF page 1 -->

~~~pdf-watermark
{"text":"内部资料","angle":45,"source":"text-layer"}
~~~

正文。

![原文插图](文档-p001-image-标识.jpg)

~~~pdf-logo
{"image":"文档-p001-logo-标识.png","alt":"原文重复图案","id":"图案标识"}
~~~
````

转换器将水印放在源页内容前；预览在页区正文之后绘制 `--第1页--`，再显示 Logo。
OCR 水印使用 `source: "outline-ocr"`，可增加 `confidence` 和相对路径 `image` 字段，
预览显示“水印（OCR）”并提供原图核对。普通 Markdown 阅读器可能将扩展显示为 JSON 代码框；
当前预览器遇到无效扩展数据时也保留代码形式，不执行其内容。

### 原文保护与加载方式

普通 PDF 正文中的 `*`、`$`、`#` 等会转义，方括号保存为 `&#91;`／`&#93;`，避免把原文
误解析为强调、公式或链接；代码区域保留代码文本。其他来源的 MD 若包含有效富文本语法，
则按本节规则渲染。只特许无属性 `<br>` 和纯文本 `<small>`；任意原始 HTML 不开放执行。

MD 页面使用固定内部地址 `https://huiwen-md.invalid/preview.html`，由 CEF 回调返回 Qt qrc
中的完整 HTML；不将富文本页面内容编码成超长 `data:` URL。子进程只为确切的该页面地址
及原有兼容页面注册预览就绪／滚动回调。页面与图片均由本地资源回调返回，不启动 HTTP 服务。
这套加载方式的目标平台语法检查与设备运行验收应分别记录。
