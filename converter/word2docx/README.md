# 独立静默 DOC → DOCX

转换核心使用我们 fork 的 HuiTurn/doc2docx（MIT、Python 标准库实现），不调用外部 Office 程序或浏览器。
核心固定在 `third_party/doc2docx/source` 子模块；主项目保留适配、打包及后台调用代码。

## Run 与 HNP

每次 Run 按源码内容准备 `hnp/arm64-v8a/huiwen_doc2docx.hnp`，内容不变复用成品。
包内只含核心运行模块、项目入口、校验／保存适配及许可证，排除上游 CLI、测试、开发脚本。
该 HNP 复用同一 HAP 安装的 `huiwen_python.hnp` 解释器，不再复制第二套 Python 或 PDF 依赖。
源码缺失时构建停止，执行主项目的完整子模块初始化命令即可恢复。

```sh
git submodule sync --recursive && git submodule update --init --recursive
python3 packaging/ohos/pack_word_hnp.py
```

## 调用

Qt 工作区通过后台转换服务调用固定原生入口：

```text
ConversionService::convertDocToDocx(inputDoc)
  → RunDocConversion(inputDoc, temporaryDocx)
  → HNP Python 执行 convert_word.pyz
  → doc_to_docx() → doc2docx.convert()
```

选择或恢复 DOC 时显示加载提示，后台转换成功后把临时 DOCX 交给原有预览器。
完整业务流程及每一步的源码位置见 [DOC 库项目说明](../../third_party/doc2docx/README.md)。
临时文件位于原沙箱任务的 `.word-preview-*` 子目录，原始 DOC 保留；更换文件或关闭时释放临时目录。
关闭期间仍在执行的转换由后台任务持有目录，完成后再清理。鸿蒙端无需配置 CEF 参数或外部程序路径。
桌面调试使用 `HUIWEN_DOC_CONVERT_SCRIPT` 指向本目录 `convert.py`，解释器沿用 `HUIWEN_PYTHON`／`python3`。

开发机直接验证：

```sh
python3 converter/word2docx/convert.py /绝对路径/原文件.doc /绝对路径/临时文件.docx --json
python3 converter/word2docx/check_conversion.py
python3 packaging/ohos/check_word_hnp.py /绝对路径/原文件.doc
```

最后一条执行 HNP 内实际 pyz，并启用 Python 隔离模式，不依赖本机安装的转换库。

## 边界

- 输入和输出限制 16 MiB，原文件只读，已有输出拒绝覆盖；工作目录成功／失败均清理。
- 误标的 DOCX 校验后复制；真实 DOC 使用包内解析器写出 DOCX，校验完成后原子发布。
- 核心为 Word 97–2003 格式实现；加密、损坏或不支持的文件返回明确错误。
- 上游仍处于 Beta，复杂绘图、旧版排版等存在限制；保真警告写入后台日志，不能声称完全等同 Word。
- 用户已确认当前 DOC 选取、转换与预览基本链路可用；复杂文档保真、异常与生命周期场景仍需按样本验收。
