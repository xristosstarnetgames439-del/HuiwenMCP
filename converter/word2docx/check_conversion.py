"""转换边界自检；不运行 Office 引擎、预览器或设备。"""

from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "third_party/doc2docx/source/src"))
from huiwen_converter import doc_to_docx


class ConversionBoundaryCheck(unittest.TestCase):
    def test_preserve_input_and_refuse_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "misnamed.doc", root / "preview.docx"
            with zipfile.ZipFile(source, "w") as package:
                package.writestr("[Content_Types].xml", (
                    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                    '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.'
                    'wordprocessingml.document.main+xml"/></Types>'
                ))
                package.writestr("_rels/.rels", (
                    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                    '<Relationship Id="rId1" Target="word/document.xml" '
                    'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"/>'
                    '</Relationships>'
                ))
                package.writestr("word/document.xml", (
                    '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
                    '<w:body><w:p><w:r><w:t>原文保留</w:t></w:r></w:p></w:body></w:document>'
                ))
            original = source.read_bytes()
            self.assertEqual(doc_to_docx(source, output), output)
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual(output.read_bytes(), original)
            with self.assertRaises(ValueError):
                doc_to_docx(source, output)
            self.assertEqual(output.read_bytes(), original)
            self.assertEqual(list(root.glob(".docx-*")), [])

    def test_invalid_binary_doc_does_not_publish_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source.doc", root / "preview.docx"
            original = bytes.fromhex("d0cf11e0a1b11ae1") + b"native engine required"
            source.write_bytes(original)
            with self.assertRaisesRegex(ValueError, "DOC 内容无法转换"):
                doc_to_docx(source, output)
            self.assertEqual(source.read_bytes(), original)
            self.assertFalse(output.exists())
            self.assertEqual(list(root.glob(".docx-*")), [])


if __name__ == "__main__":
    unittest.main()
