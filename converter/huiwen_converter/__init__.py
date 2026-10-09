"""汇文对外提供的文件转换接口；当前只开放 PDF 转 MD。"""

from .api import pdf_to_md

__all__ = ["pdf_to_md"]
