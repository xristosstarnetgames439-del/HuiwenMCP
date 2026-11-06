---
name: huiwen-pdf-to-md
description: Convert local PDFs to Markdown with the Huiwen MarkItDown fork, preserving detected headings, emphasis, tables and extracted images, with offline OCR for scanned pages. Use for PDF-to-Markdown requests or when asked to install this skill from its GitHub link; not for creating PDFs or converting Word files.
---

# 汇文 PDF → Markdown

使用汇文现有转换核心，不依赖鸿蒙、Qt、NAPI、云端接口或 API Key。目标环境是
Windows／macOS，Python 3.10–3.14。转换在本机进行；安装时需要下载依赖。

## 从 GitHub 链接安装

如果你正通过 GitHub 页面读取本文、尚未有本机 Skill：

1. 使用可用的下载工具取得仓库中的
   [download.py](https://raw.githubusercontent.com/xristosstarnetgames439-del/HuiwenMCP/main/third_party/markitdown/skill/huiwen-pdf-to-md/scripts/download.py)。
   将脚本保存到本机后运行，`--dest` 选择当前 Agent 通常使用的 Skill 目录。
   目录必须不存在；更新已有安装时先保留旧目录，勿覆盖其中的环境或用户文件。

   ```sh
   python download.py --dest "/absolute/path/to/skills/huiwen-pdf-to-md"
   ```

2. 脚本优先跟随 GitHub 公开的最新 Release 链接；若最新发布属于应用，使用 Releases API
   查找最新的、非预发布的 `pdf-skill-v*` 版本。下载
   `huiwen-pdf-to-md.zip` 和同一版本的 `SHA256SUMS`，校验后解压。普通应用发布不影响
   Skill 版本选择。无正式发布包时明确报告，不能改装成上游 Microsoft 的转换器。
   如果 API 限流且公开最新发布不含 Skill，稍后重试或让用户提供确切的正式 Skill Release 链接。
3. 重新读取安装目录内的 `SKILL.md`。后续所有相对路径均以该目录为基准。

若机器没有受支持的 Python，先按 [环境配置](references/environment.md) 配齐，
再执行下载脚本。仅阅读链接不会自动获得执行权限；遵循宿主 Agent 自身的安装规则。

## 配置与自检

先执行 `python scripts/setup.py --check`。首次使用或依赖不符时执行：

```sh
python scripts/setup.py
```

脚本在 Skill 内创建 `.venv`，安装发布包锁定的 Python 依赖，不修改系统 Python。
需要 OCR 时，按 [环境配置](references/environment.md) 安装本机 Tesseract 及简中／英文模型。
再次运行 `--check`，读取 JSON 中的 `python_ready`、`ocr_ready` 和 `errors`。
完整 PDF 能力要求两者均为 true；不能把缺失 OCR 的机器描述成扫描件可用。

## 调用

```sh
python scripts/convert.py "/absolute/path/input.pdf" "/absolute/path/output/document.md"
```

`convert.py` 只接收本地 PDF 和 MD 输出路径，调用随包携带的 `huiwen_converter.pdf_to_md()`。
输出文件已存在时默认拒绝；用户要求覆盖时才加 `--overwrite`。
无需激活虚拟环境；脚本会选择 `.venv/Scripts/python.exe` 或 `.venv/bin/python`。
路径可包含中文及空格，不拼接 Shell 命令。

完成后：

- 检查退出码和 stdout 最后一条 `HUIWEN_CONVERT_RESULT {JSON}`；`code=0` 才表示成功。
- 向用户提供生成的 `.md` 和同目录图片；转移文件时一并保留这些相对引用。
- 失败时读取 stderr 的 `[HUIWEN_PY]` 日志，不以改后缀、空文件或伪造结果替代转换。
- 保留输入 PDF。标题／加粗／表格属于解析结果，扫描件依赖 OCR；不承诺复杂排版完全还原。

只有 PDF→MD 属于本 Skill 已实现接口；需要其他格式时说明当前边界，不调用未打包的 DOC 接口。
主项目封装许可随包保留，MarkItDown 及其他组件许可分别见 `licenses/` 和 pyz 内 `licenses/`。
