"""MarkItDown 的内部适配层；其他项目代码只调用 api.py。"""

from pathlib import Path
import hashlib
import re


def pdf_to_markdown(input_path: Path, image_directory: Path, document_name: str) -> str:
    """使用第三方库提取 PDF 内容，返回尚未写入文件的 Markdown。"""
    from markitdown import MarkItDown

    # The API owns publication/rollback; the core only hands us encoded image bytes.
    prefix = (
        re.sub(r"[^\w.-]+", "_", document_name)[:32]
        + "-"
        + hashlib.sha256(document_name.encode()).hexdigest()[:8]
    )

    def write_image(data: bytes, suffix: str, page: int, role: str) -> str:
        if suffix not in {"png", "jpg"} or role not in {
            "image",
            "logo",
            "watermark",
            "page",
            "figure",
        }:
            raise ValueError("无效的 PDF 图片类型")
        digest = hashlib.sha256(data).hexdigest()[:16]
        name = f"{prefix}-p{page:03d}-{role}-{digest}.{suffix}"
        path = image_directory / name
        if not path.exists():
            path.write_bytes(data)
        return name

    return (
        MarkItDown(enable_plugins=False)
        .convert_local(input_path, pdf_image_writer=write_image)
        .markdown
    )
