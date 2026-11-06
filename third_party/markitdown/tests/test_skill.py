"""独立 Skill 下载边界检查；使用内存发布包，不访问 GitHub 或修改用户环境。"""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import MagicMock, patch
import zipfile

SOURCE = (
    Path(__file__).resolve().parents[1] / "skill/huiwen-pdf-to-md/scripts/download.py"
)
SPEC = importlib.util.spec_from_file_location("skill_download", SOURCE)
DOWNLOAD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DOWNLOAD)


def archive(extra=None):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w") as package:
        package.writestr(DOWNLOAD.NAME + "/SKILL.md", "skill")
        package.writestr(DOWNLOAD.NAME + "/assets/huiwen_converter.pyz", "converter")
        if extra:
            package.writestr(extra, "outside")
    return stream.getvalue()


class DownloadChecks(unittest.TestCase):
    def test_public_redirect_pins_both_assets_to_same_release(self):
        response = MagicMock()
        response.__enter__.return_value.geturl.return_value = (
            f"https://github.com/{DOWNLOAD.REPOSITORY}/releases/tag/pdf-skill-v1.2.3"
        )
        with patch.object(DOWNLOAD, "urlopen", return_value=response):
            tag, assets = DOWNLOAD.public_latest()
        self.assertEqual(tag, "pdf-skill-v1.2.3")
        self.assertTrue(
            all(
                "/releases/download/pdf-skill-v1.2.3/" in url for url in assets.values()
            )
        )

    def test_release_selection_skips_app_and_prerelease(self):
        assets = [
            {"name": name, "browser_download_url": name}
            for name in (DOWNLOAD.ARCHIVE, "SHA256SUMS")
        ]
        releases = [
            {
                "tag_name": "app-v9",
                "draft": False,
                "prerelease": False,
                "assets": assets,
            },
            {
                "tag_name": "pdf-skill-v2",
                "draft": False,
                "prerelease": True,
                "assets": assets,
            },
            {
                "tag_name": "pdf-skill-v1",
                "draft": False,
                "prerelease": False,
                "assets": assets,
            },
        ]
        with patch.object(DOWNLOAD, "public_latest", return_value=None), patch.object(
            DOWNLOAD, "fetch", return_value=json.dumps(releases).encode()
        ):
            self.assertEqual(DOWNLOAD.latest_release()[0], "pdf-skill-v1")

    def test_no_release_is_explicit_failure(self):
        with patch.object(DOWNLOAD, "public_latest", return_value=None), patch.object(
            DOWNLOAD, "fetch", return_value=b"[]"
        ):
            with self.assertRaisesRegex(RuntimeError, "尚未找到"):
                DOWNLOAD.latest_release()

    def test_public_latest_does_not_require_api(self):
        release = (
            "pdf-skill-v1",
            {DOWNLOAD.ARCHIVE: "archive", "SHA256SUMS": "checksums"},
        )
        with patch.object(
            DOWNLOAD, "public_latest", return_value=release
        ), patch.object(DOWNLOAD, "fetch") as fetch:
            self.assertEqual(DOWNLOAD.latest_release(), release)
            fetch.assert_not_called()

    def install(self, destination, content, checksum=None):
        digest = checksum or hashlib.sha256(content).hexdigest()
        assets = {DOWNLOAD.ARCHIVE: "archive", "SHA256SUMS": "checksums"}
        responses = {
            "archive": content,
            "checksums": f"{digest}  {DOWNLOAD.ARCHIVE}\n".encode(),
        }
        with patch.object(
            DOWNLOAD, "latest_release", return_value=("pdf-skill-v1", assets)
        ), patch.object(DOWNLOAD, "fetch", side_effect=responses.__getitem__):
            DOWNLOAD.install(destination)

    def test_install_is_independent_and_preserves_existing_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "中文 Skill"
            self.install(destination, archive())
            self.assertTrue((destination / "assets/huiwen_converter.pyz").is_file())
            (destination / "user.txt").write_text("keep")
            with self.assertRaises(FileExistsError):
                self.install(destination, archive())
            self.assertEqual((destination / "user.txt").read_text(), "keep")

    def test_bad_checksum_does_not_install(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "skill"
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                self.install(destination, archive(), "0" * 64)
            self.assertFalse(destination.exists())

    def test_missing_checksum_is_explicit_failure(self):
        assets = {DOWNLOAD.ARCHIVE: "archive", "SHA256SUMS": "checksums"}
        with tempfile.TemporaryDirectory() as temporary, patch.object(
            DOWNLOAD, "latest_release", return_value=("pdf-skill-v1", assets)
        ), patch.object(DOWNLOAD, "fetch", return_value=b"unrelated\n"):
            with self.assertRaisesRegex(ValueError, "校验清单缺少"):
                DOWNLOAD.install(Path(temporary) / "skill")

    def test_archive_cannot_escape_install_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "skill"
            with self.assertRaisesRegex(ValueError, "无效路径"):
                self.install(destination, archive(DOWNLOAD.NAME + "/../outside.txt"))
            self.assertFalse(destination.exists())
            self.assertFalse((Path(temporary) / "outside.txt").exists())


if __name__ == "__main__":
    unittest.main()
