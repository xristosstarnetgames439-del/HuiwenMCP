"""Host checks for the existing PDF API's image publication and rollback."""

from pathlib import Path
from unittest.mock import patch
import re
from urllib.parse import unquote

from PIL import Image
import pytest
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader

from huiwen_converter.api import pdf_to_md


def test_pdf_exports_small_embedded_images_beside_markdown(tmp_path):
    source = tmp_path / "中文 样本.pdf"
    pdf = canvas.Canvas(str(source), pagesize=(300, 400))
    pdf.drawString(20, 350, "Readable text")
    pdf.drawImage(ImageReader(Image.new("RGB", (12, 12), "red")), 20, 100, 12, 12)
    pdf.save()
    target = tmp_path / "output" / "中文 文档.md"
    pdf_to_md(source, target)
    text = target.read_text()
    paths = re.findall(r"!\[[^\]]*\]\(([^)]+)\)", text)
    assert paths and "base64" not in text and "data:image" not in text
    images = [target.parent / unquote(name) for name in paths]
    assert all(p.parent == target.parent and p.is_file() for p in images)
    assert all(Image.open(p).size == (12, 12) for p in images)
    before = {p.name: p.read_bytes() for p in target.parent.iterdir()}
    pdf_to_md(source, target)
    assert {p.name: p.read_bytes() for p in target.parent.iterdir()} == before


def test_failed_publication_preserves_old_markdown_and_images(tmp_path):
    target = tmp_path / "document.md"
    target.write_text("old content")
    old = tmp_path / "old.png"
    old.write_bytes(b"old image")

    def convert(source, directory, name):
        (directory / "old.png").write_bytes(b"old image")
        (directory / "new.png").write_bytes(b"new image")
        return "![new](new.png)"

    def fail_replace(self, destination):
        raise OSError("simulated write failure")

    with patch(
        "huiwen_converter.api.pdf_to_markdown", side_effect=convert
    ), patch.object(Path, "replace", fail_replace):
        with pytest.raises(OSError, match="simulated"):
            pdf_to_md(tmp_path / "input.pdf", target)
    assert target.read_text() == "old content"
    assert old.read_bytes() == b"old image"
    assert not (tmp_path / "new.png").exists()
    assert not list(tmp_path.glob(".pdf2md-*"))


def test_image_conflict_never_overwrites_existing_file(tmp_path):
    target = tmp_path / "document.md"
    target.write_text("old")
    existing = tmp_path / "picture.png"
    existing.write_bytes(b"user image")

    def convert(source, directory, name):
        (directory / existing.name).write_bytes(b"different bytes")
        return "![picture](picture.png)"

    with patch("huiwen_converter.api.pdf_to_markdown", side_effect=convert):
        with pytest.raises(ValueError, match="图片文件冲突"):
            pdf_to_md(tmp_path / "input.pdf", target)
    assert existing.read_bytes() == b"user image" and target.read_text() == "old"
