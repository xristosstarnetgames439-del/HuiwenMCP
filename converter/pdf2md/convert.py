"""PDF 固定命令入口；鸿蒙作为 pyz 的 __main__，桌面可直接执行，业务 API 保持不变。"""

import errno
import json
import logging
from pathlib import Path
import sys


def failure_result(error: Exception) -> tuple[int, str]:
    pending = [error]
    seen = set()
    while pending:
        current = pending.pop()
        if id(current) in seen:
            continue
        seen.add(id(current))
        name = type(current).__name__
        if isinstance(current, (ImportError, ModuleNotFoundError)) or name == "MissingDependencyException":
            return 1008, "转化失败：转换组件缺失，请更新或重新安装应用后重试。"
        if isinstance(current, FileNotFoundError):
            return 1002, "转化失败：所选文件已不存在，请重新选择 PDF。"
        if isinstance(current, PermissionError):
            return 1006, "转化失败：无法读取文件或写入目录，请重新选择文件后重试。"
        if isinstance(current, OSError) and current.errno in (errno.ENOSPC, errno.EDQUOT):
            return 1007, "转化失败：存储空间不足，请清理空间后重新转化。"
        if name in ("PDFPasswordIncorrect", "PDFEncryptionError", "PDFTextExtractionNotAllowed"):
            return 1003, "转化失败：PDF 已加密或限制文字提取，请先解除限制再转化。"
        if name in ("PDFSyntaxError", "PSEOF", "PDFNoValidXRef", "UnsupportedFormatException"):
            return 1004, "转化失败：PDF 内容损坏或格式不受支持，请确认原文件可正常打开。"
        if isinstance(current, ValueError):
            if "PDF 未提取到文字" in str(current):
                return 1005, "转化失败：未提取到文字，扫描件暂不支持，请选择含可复制文字的 PDF。"
            return 1001, "转化失败：文件路径或格式不正确，请重新选择 PDF。"
        cause = current.__cause__ or current.__context__
        if cause is not None:
            pending.append(cause)
        for attempt in getattr(current, "attempts", None) or []:
            if attempt.exc_info is not None:
                pending.append(attempt.exc_info[1])
    return 1099, "转化失败：文件处理异常，请重新转化；若仍失败，请更换 PDF。"


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="[HUIWEN_PY][%(levelname)s] %(name)s %(message)s",
        stream=sys.stderr,
    )
    logger = logging.getLogger("huiwen.convert")
    try:
        if len(sys.argv) != 3:
            raise ValueError("需要输入 PDF 路径和输出 MD 路径")
        source, target = Path(sys.argv[1]), Path(sys.argv[2])
        if source.suffix.lower() != ".pdf" or target.suffix.lower() != ".md":
            raise ValueError("输入必须是 PDF，输出必须是 MD")
        if not source.is_file():
            raise FileNotFoundError(source)
        import markitdown
        from markitdown.converters import _pdf_converter
        from huiwen_converter import pdf_to_md

        logger.info(
            "RUNTIME python=%s markitdown=%s module=%s pdf_module=%s algorithm=%s",
            sys.version.split()[0],
            getattr(markitdown, "__version__", "unknown"),
            markitdown.__file__,
            _pdf_converter.__file__,
            getattr(_pdf_converter, "PDF_STRUCTURE_ALGORITHM", "upstream-no-structure"),
        )
        output = pdf_to_md(source, target)
        result = {"code": 0, "message": "转化成功，Markdown 文件已生成。", "outputPath": str(output)}
        logger.info("CONVERT_DONE")
        print("HUIWEN_CONVERT_OK", flush=True)
    except Exception as error:
        logger.exception("CONVERT_FAILED")
        code, message = failure_result(error)
        result = {"code": code, "message": message, "outputPath": ""}
    print("HUIWEN_CONVERT_RESULT " + json.dumps(result, ensure_ascii=False), flush=True)
    return 0 if result["code"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
