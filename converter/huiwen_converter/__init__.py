"""独立文件转换接口；应用现有业务仍只调用 PDF 转 MD。"""

from .api import doc_to_docx, pdf_to_md

__all__ = ["doc_to_docx", "pdf_to_md"]
