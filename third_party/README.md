# 第三方源码与项目适配边界

在项目根目录执行以下一条命令，下载主项目固定版本的全部源码子模块：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

| 目录 | 主项目维护 | 独立仓库维护 |
| --- | --- | --- |
| `pdfium/` | `build.py`、`ohos/` 工具链模板、`patches/` 兼容补丁 | `source/`：PDFium fork |
| `markitdown/` | `patches/` Python 兼容补丁及使用说明 | `source/`：MarkItDown fork |
| `markdown/` | 页面入口、样式、滚动同步、`build.mjs`、检查脚本及工具锁文件 | `source/`：汇总维护的 MD 核心模块 |
| `word/` | 库入口、`build.mjs`、检查脚本及依赖锁文件 | `source/`：PortOne fork |
| `doc2docx/` | 使用说明；转换适配在 `converter/`，HNP 打包在 `packaging/ohos/` | `source/`：DOC→DOCX 纯 Python 核心 fork |
| `qt/` | SDK 准备脚本与使用说明 | 固定预编译 SDK，不新增内核源码仓库 |
| `cef/` | 当前 HAR／桥接模块、准备脚本与说明 | 当前固定 `.so`／SDK，不迁移 Chromium 内核 |
| `prebuilt/` | 固定 Qt／CEF 包的清单、校验、恢复和制包脚本 | 原有 Release 固定包 |

普通 `git clone` 取得适配层，`source/` 子模块目录为空；必须显式初始化后再 Run。
Run 缺少任何源码会在准备依赖前停止并提示初始化命令，不自行拉取或更新 Git 版本。
Node／PDFium 构建工具和固定 SDK 仍按原有锁定流程准备，不进入主项目 Git。

修改 `source/` 内的核心代码，在对应子模块提交、推送到我们的仓库；
修改外层适配文件，在 HuiwenMCP 提交。核心提交推送成功后，主项目再记录新的子模块指针。
`git push` 主项目不会自动提交／推送子模块内部改动。初始化默认取得主项目固定提交，
子模块可能处于 detached HEAD；维护核心时先切换或创建相应维护分支，再提交。

`packaging/ohos/` 继续维护 HAP／Python HNP／业务 pyz 打包及应用级检查，
第三方构建适配集中在本目录。所有上游文件保留各自许可证。
