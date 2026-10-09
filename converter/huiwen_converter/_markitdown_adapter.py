"""MarkItDown 的内部适配层；其他项目代码只调用 api.py。"""

from pathlib import Path


def pdf_to_markdown(input_path: Path) -> str:
    """使用第三方库提取 PDF 内容，返回尚未写入文件的 Markdown。"""
    from markitdown import MarkItDown

    return MarkItDown(enable_plugins=False).convert_local(input_path).markdown
