"""稳定的 Python 业务接口，不向调用方暴露第三方转换库。"""

from pathlib import Path
import os
import tempfile

from ._markitdown_adapter import pdf_to_markdown


def pdf_to_md(input_path: str | Path, output_path: str | Path) -> Path:
    """将 PDF 写成 UTF-8 Markdown，并返回生成文件路径。

    输入、输出均为应用可访问的本地路径。无法提取文字或转换失败时抛出异常，
    不新建空的 MD 文件；扫描页由转换核心调用 HNP 内的离线 OCR。
    """
    source = Path(input_path)
    target = Path(output_path)
    if source.suffix.lower() != ".pdf" or target.suffix.lower() != ".md":
        raise ValueError("输入必须是 PDF，输出必须是 MD")

    target.parent.mkdir(parents=True, exist_ok=True)
    # Stage on the same filesystem, then publish images before replacing the MD.
    # Content-addressed images are immutable; existing documents remain usable on failure.
    with tempfile.TemporaryDirectory(prefix=".pdf2md-", dir=target.parent) as temporary:
        stage = Path(temporary)
        images = stage / "images"
        images.mkdir()
        markdown = pdf_to_markdown(source, images, target.stem)
        if not markdown or not markdown.strip():
            raise ValueError("PDF 未提取到文字；OCR 未生成有效内容")
        staged_markdown = stage / "document.md"
        staged_markdown.write_text(markdown, encoding="utf-8")
        created = []
        try:
            for image in images.iterdir():
                destination = target.parent / image.name
                try:
                    os.link(image, destination)
                    created.append(destination)
                except FileExistsError:
                    if (
                        destination.is_symlink()
                        or destination.read_bytes() != image.read_bytes()
                    ):
                        raise ValueError(f"图片文件冲突：{destination.name}")
            staged_markdown.replace(target)
        except Exception:
            for image in created:
                image.unlink(missing_ok=True)
            raise
    return target


def doc_to_docx(input_path: str | Path, output_path: str | Path) -> Path:
    """使用内置 DOC 核心静默转换；预览及输出清理由调用方负责。"""
    from ._word_adapter import doc_to_docx as convert_word

    return convert_word(input_path, output_path)
