# Markdown 核心与主项目页面适配

`source/` 子模块来自 <https://github.com/xristosstarnetgames439-del/huiwen-markdown>，
统一维护 CodeMirror、Lezer Markdown、markdown-it 及共 28 个运行时包。
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
本次目录迁移仅做语法与格式检查，未重新构建资源、编译 C++、打包 HAP 或运行设备。
