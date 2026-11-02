"""从已有 OpenQtCef 工程迁入 CEF 环境，不启用浏览器或修改 Qt 运行库。"""

import argparse
import hashlib
import json
from pathlib import Path
import sys
import re
import subprocess

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.pdfium.build import native_sdk
from third_party.qt.prepare import ROOT, copy_changed

CEF = ROOT / "third_party/cef"
SDK = CEF / "sdk"
LIBS = ROOT / "entry/libs/arm64-v8a"
INCLUDE_DIRS = ("base", "capi", "internal", "test", "views", "wrapper")
IGNORE_PARTS = {".git", "build", ".hvigor", "oh_modules", "node_modules"}
IGNORE_RESOURCES = {"cefclient_files", "ceftests_files"}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare(source):
    source = source.resolve()
    cpp = source / "entry/src/main/cpp"
    sdk = native_sdk()
    readelf = sdk / "llvm/bin/llvm-readelf"
    paths = {}

    def add_tree(directory, destination):
        if not directory.is_dir():
            raise SystemExit(f"缺少 CEF 环境目录：{directory}")
        for path in sorted(directory.rglob("*")):
            relative = path.relative_to(directory)
            if (path.is_file() and not IGNORE_PARTS.intersection(relative.parts)
                    and not IGNORE_RESOURCES.intersection(relative.parts)):
                paths[destination / relative] = path

    for directory in INCLUDE_DIRS:
        add_tree(cpp / "include" / directory, SDK / "include" / directory)
    paths.update({SDK / "include" / path.name: path
                  for path in (cpp / "include").glob("cef_*.h")})
    for directory in ("cmake", "libcef_dll", "ohos", "log"):
        add_tree(cpp / directory, SDK / directory)

    web_engine = source / "web_engine"
    add_tree(web_engine / "src", SDK / "web_engine/src")
    for name in ("Index.ets", "childProcess.ets", "BuildProfile.ets", "build-profile.json5",
                 "oh-package.json5", "hvigorfile.ts", "obfuscation-rules.txt", "consumer-rules.txt"):
        paths[SDK / "web_engine" / name] = web_engine / name

    for name in ("libcef.so", "libadapter.so", "libc++_shared.so"):
        paths[LIBS / name] = source / "entry/libs/arm64-v8a" / name
    adapter = source / "entry/libs/arm64-v8a/libadapter_c.so"
    if not adapter.is_file():
        adapter = (source / "entry/build/default/intermediates/stripped_native_libs"
                   / "default/arm64-v8a/libadapter_c.so")
    paths[LIBS / adapter.name] = adapter

    required = (SDK / "include/cef_version.h", SDK / "cmake/FindCEF.cmake",
                SDK / "libcef_dll/CMakeLists.txt", SDK / "ohos/adapter_c/adapter_c.cpp",
                SDK / "web_engine/src/main/ets/ability/WebQtAbility.ets",
                SDK / "web_engine/src/main/resources/resfile/icudtl.dat")
    if any(path not in paths for path in required):
        raise SystemExit("来源工程缺少 CEF 头文件、包装源码、适配层或核心资源")
    for path in paths.values():
        if not path.is_file():
            raise SystemExit(f"缺少 CEF 环境文件：{path}")

    libraries = {}
    system_libs = sdk / "sysroot/usr/lib/aarch64-linux-ohos"
    for target, path in paths.items():
        if target.parent != LIBS:
            continue
        with path.open("rb") as file:
            header = file.read(20)
        if (header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[16:18], "little") != 3
                or int.from_bytes(header[18:20], "little") != 183):
            raise SystemExit(f"不是 ARM64 ELF 动态库：{path}")
        dynamic = subprocess.check_output([readelf, "--dynamic", path], text=True)
        needed = re.findall(r"Shared library: \[(.*?)\]", dynamic)
        missing = [name for name in needed
                   if LIBS / name not in paths and not (LIBS / name).is_file()
                   and not (system_libs / name).is_file()]
        if missing:
            raise SystemExit(f"{path.name} 缺少动态依赖：{', '.join(missing)}")
        digest = sha256(path)
        if target.is_file() and sha256(target) != digest:
            raise SystemExit(f"不覆盖已有不同版本的运行库：{target}")
        libraries[path.name] = {"sha256": digest, "bytes": path.stat().st_size,
                                "needed": needed}

    files = {}
    for target, path in sorted(paths.items()):
        copy_changed(path, target)
        digest = sha256(target)
        if digest != sha256(path):
            raise SystemExit(f"迁入文件校验失败：{target}")
        files[target.relative_to(ROOT).as_posix()] = {
            "source": path.relative_to(source).as_posix(),
            "bytes": target.stat().st_size, "sha256": digest}
    release = SDK / "Release"
    release.mkdir(parents=True, exist_ok=True)
    for name in libraries:
        link = release / name
        relative = Path("../../../../entry/libs/arm64-v8a") / name
        if link.exists() and not link.is_symlink():
            raise SystemExit(f"不覆盖已有非符号链接：{link}")
        if link.is_symlink():
            link.unlink()
        link.symlink_to(relative)

    # 来源没有单独许可证文件，保留同套 CEF 头文件中的完整 BSD 声明。
    header = (cpp / "include/cef_version.h").read_text()
    license_text = header.split("// ---------------------------------------------------------------------------", 1)[0]
    license_file = SDK / "licenses/CEF-BSD.txt"
    license_file.parent.mkdir(parents=True, exist_ok=True)
    license_file.write_text("\n".join(line.removeprefix("// ").removeprefix("//")
                                     for line in license_text.splitlines()).strip() + "\n")
    version = re.search(r'^#define CEF_VERSION "([^"]+)"', header, re.MULTILINE).group(1)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    metadata = {"origin": "Tongda OpenQtCef working-tree snapshot", "source_revision": revision,
                "cef_version_header": version, "target": "OpenHarmony arm64-v8a",
                "activated": False, "libraries": libraries, "copied_files": len(files),
                "copied_bytes": sum(file["bytes"] for file in files.values()),
                "license_materials": "CEF header BSD notice; Chromium/vendor notices incomplete",
                "adapter_c": "Copied stripped prebuilt library; not rebuilt from migrated sources"}
    (SDK / "files.json").write_text(json.dumps(files, ensure_ascii=False, indent=2) + "\n")
    (CEF / "environment.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    from third_party.cef.prepare_preview import prepare as prepare_preview
    prepare_preview()
    print(f"[HUIWEN_CEF] 迁入 {len(files)} 个文件；SDK：{SDK}；运行库：{LIBS}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="已有 OpenQtCef 工程目录")
    args = parser.parse_args()
    prepare(args.source)
