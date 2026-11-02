# PDFium 源码与 OpenHarmony 构建适配

- `source/` 是独立子模块，来自我们的 PDFium fork，固定主项目记录的提交。
- `build.py`、`ohos/BUILD.gn`、`patches/` 属于 HuiwenMCP，修改推送到主仓库。
- 核心引擎修改在 `source/` 提交、推送到 PDFium fork 的维护分支，再更新主项目指针。

项目根目录初始化全部源码：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

准备／构建命令：`python3 third_party/pdfium/build.py --sync`。
每次 Run 增量构建当前源码并同步 `libpdfium.so` 到 `entry/libs/arm64-v8a/`。
构建不执行 Git pull，不覆盖核心工作树修改，失败不替换有效旧库。

GN 要求工具链标签位于源码根内，因此构建时将主项目模板复制到忽略目录
`source/huiwen_adapter/`，并使用 `//huiwen_adapter:arm64`。源码快照内原有的
`source/huiwen/` 历史适配文件保持原样，当前主项目不再读取它们。
Chromium 构建依赖的补丁从外层 `patches/` 读取；构建记录包含适配文件哈希。
