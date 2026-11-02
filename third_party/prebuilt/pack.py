"""从维护者已校验的 Qt/CEF 环境生成固定依赖包和可提交清单，不编译应用。"""
import argparse
import gzip
import json
import os
from pathlib import Path
import sys
import stat
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.prebuilt.prepare import ROOT, MANIFEST, sha256


def pack(version, url):
    qt = json.loads((ROOT / "third_party/qt/sdk.json").read_text())
    cef = json.loads((ROOT / "third_party/cef/environment.json").read_text())
    paths = set()
    for directory in ("third_party/qt/bin", "third_party/qt/include", "third_party/qt/lib",
                      "third_party/qt/mkspecs", "third_party/qt/plugins", "third_party/cef/sdk",
                      "entry/src/main/resources/rawfile/third_party_licenses/qt"):
        for path in (ROOT / directory).rglob("*"):
            relative = path.relative_to(ROOT)
            if not {"build", ".hvigor", "oh_modules", "node_modules"}.intersection(relative.parts):
                if path.is_file() or path.is_symlink():
                    paths.add(relative.as_posix())
    paths.add("third_party/qt/sdk.json")
    libraries = {**qt["libraries"], **{n: r["sha256"] for n, r in cef["libraries"].items()}}
    for name, digest in libraries.items():
        path = ROOT / "entry/libs/arm64-v8a" / name
        if sha256(path) != digest:
            raise RuntimeError(f"预编译库与来源清单不一致：{name}")
        paths.add(path.relative_to(ROOT).as_posix())
    output = ROOT / ".native-tools/prebuilt"
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"huiwen-qt-cef-{version}.tar.gz"
    files = {}
    # 固定时间戳和排序，确保同一依赖内容可重复生成完全相同的包。
    with archive.open("wb") as raw, gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
        with tarfile.open(fileobj=gz, mode="w|") as tar:
            for name in sorted(paths):
                path = ROOT / name
                info = tar.gettarinfo(str(path), arcname=name)
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ""
                if path.is_symlink():
                    if not path.resolve().is_relative_to(ROOT):
                        raise RuntimeError(f"不打包指向项目外的链接：{name}")
                    files[name] = {"type": "symlink", "target": os.readlink(path)}
                    tar.addfile(info)
                else:
                    files[name] = {"type": "file", "mode": stat.S_IMODE(path.stat().st_mode),
                                   "bytes": path.stat().st_size, "sha256": sha256(path)}
                    with path.open("rb") as stream:
                        tar.addfile(info, stream)
    manifest = {"version": version, "host": qt["host"], "target": qt["target"],
                "qt": qt["version"], "cef": cef["cef_version_header"], "archive": archive.name,
                "url": url, "bytes": archive.stat().st_size, "sha256": sha256(archive), "files": files}
    destination = ROOT / MANIFEST
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(f"[HUIWEN_PREBUILT] packed {archive} bytes={manifest['bytes']} sha256={manifest['sha256']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--url", required=True)
    args = parser.parse_args()
    pack(args.version, args.url)
