# 本机环境配置

支持 Python 3.10–3.14，优先使用已有的受支持版本；新安装可选择 Python 3.12。
Windows 可用 `py -3.12` 替代下列 `python`；macOS 通常使用 `python3`。
解释器、Pillow、cryptography、PDFium 必须与本机操作系统和架构匹配。
不要从鸿蒙 HNP 复制解释器或 `.so`，也不要执行 `pip install markitdown` 替换包内的定制核心。

## Python 与依赖

没有 Python 时从 https://www.python.org/downloads/ 取得对应系统的安装包。
安装后检查版本，再执行 `scripts/setup.py`。所有 pip 操作由脚本指向 Skill 自己的 `.venv`；
公司代理或镜像使用现有 pip 配置，不需要系统管理员权限，也不改系统解释器。

`assets/requirements.txt` 来自主项目同一份 PDF 依赖锁定文件。
二进制扩展由 pip 选择本机版本；Tesseract 是独立程序，不能靠 pip 安装替代。

## macOS OCR

已经使用 Homebrew 的机器可执行：

```sh
brew install tesseract tesseract-lang
tesseract --list-langs
```

没有 Homebrew 时按其官方安装方式或已有包管理器准备，不擅自改动用户的其他软件环境。

## Windows OCR

按 [Tesseract 官方 Windows 安装指引](https://tesseract-ocr.github.io/tessdoc/Installation.html)
选择其列出的 Windows 安装器，安装简体中文和英文模型。
若未加入 PATH，可在当前 Agent 会话配置实际安装路径，例如：

```powershell
$env:HUIWEN_TESSERACT = 'C:\Program Files\Tesseract-OCR\tesseract.exe'
$env:HUIWEN_TESSDATA = 'C:\Program Files\Tesseract-OCR\tessdata'
```

需要时，模型可从 https://github.com/tesseract-ocr/tessdata_fast 下载
`chi_sim.traineddata` 和 `eng.traineddata` 到用户可写的模型目录，并设置
`HUIWEN_TESSDATA`。模型目录不必放在 Program Files 内。

## 检查结果与配置项

```sh
python scripts/setup.py --check
```

- `python_ready=true`：锁定依赖、随包转换 API 和本机 PDFium 已可加载。
- `ocr_ready=true`：Tesseract 可执行，存在配置的识别语言及英文模型。
- 退出码为 0：两者均就绪。退出码非 0：按 `errors` 修复后复查，不重装整个应用。
- `HUIWEN_TESSERACT`：Tesseract 完整路径，含 Windows 的 `.exe`。
- `HUIWEN_TESSDATA`：实际模型目录，内含 `.traineddata` 文件。
- `HUIWEN_OCR_LANGUAGE`：默认 `chi_sim`，可按文档设为 `eng` 或 `chi_sim+eng`。
- `HUIWEN_PDFIUM`：可选的本机 PDFium 动态库路径；通常由 pypdfium2 自动提供。

扫描件、图片和水印处理可能需要 OCR，即使 PDF 还包含可提取文字。
当前图片发布使用硬链接，输出应位于支持硬链接的本地文件系统（如 NTFS／APFS）；
不支持硬链接的移动盘可先输出到本机目录，再复制整个输出目录。

完整发布包含 `assets/huiwen_converter.pyz`、依赖清单和来源清单；若直接克隆的是
Skill 源码目录，先下载正式发布包。维护者也可在主项目运行 `third_party/markitdown/build_skill.py`。
