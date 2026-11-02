"""隔离检查固定依赖的下载、校验、恢复和失败保护；不构建应用。"""
import hashlib
import io
import json
from pathlib import Path
import sys
import platform
import tarfile
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.prebuilt.prepare import prepare, MANIFEST


def fixture(root, link="../../../entry/libs/arm64-v8a/libcef.so"):
    files = {
        "third_party/qt/bin/moc": (b"host-tool", 0o755),
        "entry/libs/arm64-v8a/libcef.so": (b"fixed-library", 0o644),
        "third_party/cef/sdk/web_engine/src/main/resources/resfile/icudtl.dat": (b"core-resource", 0o644),
    }
    records = {}
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode="w:gz") as tar:
        for name, (content, mode) in files.items():
            record = {"type": "file", "mode": mode, "bytes": len(content),
                      "sha256": hashlib.sha256(content).hexdigest()}
            records[name] = record
            info = tarfile.TarInfo(name)
            info.size, info.mode = len(content), mode
            tar.addfile(info, io.BytesIO(content))
        name = "third_party/qt/lib/libcef.so"
        records[name] = {"type": "symlink", "target": link}
        info = tarfile.TarInfo(name)
        info.type, info.linkname = tarfile.SYMTYPE, link
        tar.addfile(info)
    payload = data.getvalue()
    manifest = {"version": "check", "host": f"{platform.system()}-{platform.machine()}",
                "archive": "check.tar.gz", "url": "https://example.invalid/check.tar.gz",
                "sha256": hashlib.sha256(payload).hexdigest(), "bytes": len(payload), "files": records}
    path = root / MANIFEST
    path.parent.mkdir(parents=True)
    path.write_text(json.dumps(manifest))
    return payload, manifest


def must_fail(action, message):
    try:
        action()
    except RuntimeError as error:
        assert message in str(error), str(error)
    else:
        raise AssertionError("expected failure: " + message)


def check():
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-check-") as directory:
        root = Path(directory)
        payload, manifest = fixture(root)
        with patch("urllib.request.urlopen", return_value=io.BytesIO(payload)) as download:
            prepare(root)
            assert download.call_count == 1
            assert download.call_args.args[0].full_url == manifest["url"]
        tool = root / "third_party/qt/bin/moc"
        library = root / "entry/libs/arm64-v8a/libcef.so"
        link = root / "third_party/qt/lib/libcef.so"
        assert tool.stat().st_mode & 0o111
        assert link.is_symlink() and link.read_bytes() == library.read_bytes()
        with patch("urllib.request.urlopen", side_effect=AssertionError("unexpected network request")):
            prepare(root)
            tool.unlink()
            prepare(root)  # 缓存恢复缺失文件。
        library.write_bytes(b"local-custom-library")
        must_fail(lambda: prepare(root), "保留原文件")
        assert library.read_bytes() == b"local-custom-library"
        print("PASS download, offline cache, executable bits, symlinks, missing-file recovery, local edits preserved")
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-corrupt-") as directory:
        root = Path(directory)
        payload, _ = fixture(root)
        def partial(request, **kwargs):
            offset = int(request.headers.get("Range", "bytes=0-")[6:-1])
            response = io.BytesIO(payload[offset:offset + 7])
            response.status = 206 if offset else 200
            response.headers = {"Content-Range": f"bytes {offset}-{offset + 6}/{len(payload)}"}
            return response
        with patch("urllib.request.urlopen", side_effect=partial) as download:
            must_fail(lambda: prepare(root), "下载不完整")
            assert download.call_count == 3
        corrupted = bytes([payload[0] ^ 1]) + payload[1:]
        with patch("urllib.request.urlopen", return_value=io.BytesIO(corrupted)):
            must_fail(lambda: prepare(root), "SHA-256")
        assert not (root / "entry/libs").exists()
        assert not list((root / ".native-tools/prebuilt").glob("download-*"))
        print("PASS truncated download and checksum failure leave no installed files or partial download")
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-resume-") as directory:
        root = Path(directory)
        payload, _ = fixture(root)
        first, remaining = io.BytesIO(payload[:17]), io.BytesIO(payload[17:])
        remaining.status = 206
        remaining.headers = {"Content-Range": f"bytes 17-{len(payload) - 1}/{len(payload)}"}
        with patch("urllib.request.urlopen", side_effect=[first, remaining]) as download:
            prepare(root)
            assert download.call_args.args[0].headers["Range"] == "bytes=17-"
        assert (root / "entry/libs/arm64-v8a/libcef.so").read_bytes() == b"fixed-library"
        print("PASS truncated download resumes at the exact byte offset")
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-range-") as directory:
        root = Path(directory)
        payload, _ = fixture(root)
        ignored_range = io.BytesIO(payload)
        ignored_range.status, ignored_range.headers = 200, {}
        with patch("urllib.request.urlopen", side_effect=[io.BytesIO(payload[:17]), ignored_range]):
            must_fail(lambda: prepare(root), "断点续传")
        assert not (root / "entry/libs").exists()
        print("PASS ignored Range response rejected before installing files")
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-links-") as directory:
        root = Path(directory)
        payload, _ = fixture(root, "/tmp/outside-huiwen-prebuilt")
        archive = root / "test.tar.gz"
        archive.write_bytes(payload)
        must_fail(lambda: prepare(root, archive), "符号链接不合法")
        assert not (root / "entry/libs").exists()
        print("PASS unsafe archive link rejected before installing files")
    with tempfile.TemporaryDirectory(prefix="huiwen-prebuilt-paths-") as directory:
        root = Path(directory)
        _, manifest = fixture(root)
        manifest["files"]["../outside"] = {"type": "file"}
        (root / MANIFEST).write_text(json.dumps(manifest))
        must_fail(lambda: prepare(root), "路径不合法")
        manifest["host"] = "unsupported-host"
        (root / MANIFEST).write_text(json.dumps(manifest))
        must_fail(lambda: prepare(root), "对应宿主工具包")
        print("PASS traversal and unsupported host rejected")


if __name__ == "__main__":
    check()
