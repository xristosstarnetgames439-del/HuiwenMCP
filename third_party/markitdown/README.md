# MarkItDown 源码与项目兼容补丁

`source/` 是我们的 MarkItDown fork 子模块，核心 PDF 转换算法在该仓库维护。
`patches/no-magika.patch` 属于主项目，用于鸿蒙 Python 环境中按已知 PDF 扩展名转换，
不强制导入 Magika／ONNX Runtime。

项目根目录执行 `git submodule sync --recursive && git submodule update --init --recursive`。
`packaging/ohos/build_python_bundle.py` 从 `source/` 读取源码，在临时目录应用补丁，
与主项目转换 API 打成业务 pyz；不改动子模块源码。应用级打包仍由主仓库维护。
