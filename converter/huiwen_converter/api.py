"""稳定的 Python 业务接口，不向调用方暴露第三方转换库。"""

from pathlib import Path

from ._markitdown_adapter import pdf_to_markdown


def pdf_to_md(input_path: str | Path, output_path: str | Path) -> Path:
    """将文字型 PDF 写成 UTF-8 Markdown，并返回生成文件路径。

    输入、输出均为应用可访问的本地路径。无法提取文字或转换失败时抛出异常，
    不新建空的 MD 文件；当前接口不处理扫描件 OCR。
    """
    source = Path(input_path)
    target = Path(output_path)
    if source.suffix.lower() != ".pdf" or target.suffix.lower() != ".md":
        raise ValueError("输入必须是 PDF，输出必须是 MD")

    markdown = pdf_to_markdown(source)
    if not markdown or not markdown.strip():
        raise ValueError("PDF 未提取到文字；扫描件需要 OCR")

    target.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = target.with_suffix(".md.part")
    try:
        temporary_path.write_text(markdown, encoding="utf-8")
        temporary_path.replace(target)
    finally:
        temporary_path.unlink(missing_ok=True)
    return target
