"""独立的本地 DOC 转 DOCX；不导入浏览器、Node 或前端模块。"""

import os
from pathlib import Path
import tempfile
import warnings
import xml.etree.ElementTree as ET
import zipfile

MAX_FILE_BYTES = 16 * 1024 * 1024
DOC_SIGNATURE = bytes.fromhex("d0cf11e0a1b11ae1")


class _XmlWithoutDtd(ET.TreeBuilder):
    def doctype(self, name, public_id, system_id):
        raise ValueError("DOCX XML 不允许 DTD 或实体声明")


def _validate_docx(path: Path) -> None:
    if not 0 < path.stat().st_size <= MAX_FILE_BYTES:
        raise ValueError("DOCX 为空或超过 16 MiB")
    try:
        with zipfile.ZipFile(path) as package:
            parts = package.infolist()
            if len(parts) > 10000 or sum(part.file_size for part in parts) > 64 * 1024 * 1024:
                raise ValueError("DOCX 解压大小或部件数量超限")
            if len(parts) != len({part.filename for part in parts}):
                raise ValueError("DOCX 含重复部件")
            for name in ("[Content_Types].xml", "_rels/.rels", "word/document.xml"):
                if package.getinfo(name).file_size > 32 * 1024 * 1024:
                    raise ValueError("DOCX XML 部件过大")
            if package.testzip() is not None:
                raise ValueError("DOCX 包校验失败")

            def read_xml(name):
                return ET.fromstring(package.read(name), parser=ET.XMLParser(target=_XmlWithoutDtd()))

            content_types = read_xml("[Content_Types].xml")
            body = read_xml("word/document.xml")
            if content_types.tag != "{http://schemas.openxmlformats.org/package/2006/content-types}Types":
                raise ValueError("DOCX 内容类型无效")
            types_namespace = "{http://schemas.openxmlformats.org/package/2006/content-types}"
            main_type = "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"
            if not any(part.get("PartName") == "/word/document.xml" and part.get("ContentType") == main_type
                       for part in content_types.findall(types_namespace + "Override")):
                raise ValueError("DOCX 缺少正文内容类型")
            relationships = read_xml("_rels/.rels")
            rel_namespace = "{http://schemas.openxmlformats.org/package/2006/relationships}"
            office_type = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"
            if relationships.tag != rel_namespace + "Relationships" or not any(
                relation.get("Type") == office_type and relation.get("Target", "").lstrip("/") == "word/document.xml"
                and relation.get("TargetMode", "Internal") == "Internal"
                for relation in relationships.findall(rel_namespace + "Relationship")
            ):
                raise ValueError("DOCX 缺少内部 Word 正文关联")
            namespace = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
            if body.tag != namespace + "document" or body.find(namespace + "body") is None:
                raise ValueError("DOCX 缺少 Word 正文")
    except (zipfile.BadZipFile, KeyError, ET.ParseError) as error:
        raise ValueError("转换结果不是可读取的 DOCX 文档") from error


def doc_to_docx(input_path: str | Path, output_path: str | Path) -> Path:
    """使用内置纯 Python 核心静默转换，保留输入且拒绝覆盖；不调用外部程序。"""
    source = Path(input_path).resolve(strict=True)
    target = Path(output_path).absolute()
    if source.suffix.lower() != ".doc" or target.suffix.lower() != ".docx":
        raise ValueError("输入必须为 DOC，输出必须为 DOCX")
    if not source.is_file() or target.exists() or target.is_symlink():
        raise ValueError("输入不是文件，或输出已存在；拒绝覆盖")
    with source.open("rb") as stream:
        data = stream.read(MAX_FILE_BYTES + 1)
    if not 0 < len(data) <= MAX_FILE_BYTES:
        raise ValueError("DOC 为空或超过 16 MiB")
    package = data.startswith(b"PK\x03\x04")
    if not package and not data.startswith(DOC_SIGNATURE):
        raise ValueError("输入不是二进制 DOC，也不是后缀误标的 DOCX")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".docx-", dir=target.parent) as directory:
        work = Path(directory).resolve()
        staged = work / "source.doc"
        staged.write_bytes(data)
        result = staged
        if not package:
            try:
                from doc2docx import convert
                from doc2docx.cfb import CompoundFileLimits
                from doc2docx.errors import Doc2DocxError
            except ImportError as error:
                raise RuntimeError("缺少内置 DOC 转换核心，请初始化子模块并重新准备 DOC HNP") from error
            result = work / "source.docx"
            try:
                converted = convert(staged, result, limits=CompoundFileLimits(
                    max_input_bytes=MAX_FILE_BYTES, max_stream_bytes=MAX_FILE_BYTES,
                    max_directory_entries=65536))
            except Doc2DocxError as error:
                raise ValueError(f"DOC 内容无法转换：{error}") from error
            if converted.report.warnings:
                codes = ", ".join(item.code for item in converted.report.warnings[:8])
                warnings.warn(f"DOC 转换有 {len(converted.report.warnings)} 条保真提示：{codes}", stacklevel=2)
        _validate_docx(result)
        os.link(result, target)  # 同一文件系统原子发布，不覆盖竞争创建的输出。
    return target
