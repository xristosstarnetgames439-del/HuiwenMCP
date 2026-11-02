"""把同一套鸿蒙 Qt SDK 的开发文件和运行库部署到项目内。"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
QT = ROOT / "third_party/qt"
LIBS = ROOT / "entry/libs/arm64-v8a"
MODULES = ("Core", "Gui", "Widgets", "Concurrent")
HEADER_MODULES = (*MODULES, "Napi")  # 鸿蒙 QtCore 公开头文件依赖 QtNapi 的内联封装。


def properties():
    path = ROOT / "local.properties"
    return dict(line.split("=", 1) for line in path.read_text().splitlines()
                if "=" in line and not line.lstrip().startswith("#")) if path.exists() else {}


def copy_changed(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or source.read_bytes() != target.read_bytes():
        shutil.copy2(source, target)


def prepare(source, license_source):
    source = source.resolve()
    required = [source / "include" / f"Qt{module}" for module in HEADER_MODULES]
    required += [source / "lib" / f"libQt5{module}.so" for module in MODULES]
    required += [source / "bin" / tool for tool in ("moc", "rcc", "qmake", "uic")]
    required += [source / "plugins/platforms/libplugins_platforms_qopenharmony.so"]
    for path in required:
        if not path.exists():
            raise SystemExit(f"Qt SDK 缺少：{path}")
    version = subprocess.check_output([source / "bin/moc", "-v"], text=True).strip()
    if version != "moc 5.15.12":
        raise SystemExit(f"要求当前项目使用的 Qt 5.15.12，实际为：{version}")
    for module in HEADER_MODULES:
        shutil.copytree(source / "include" / f"Qt{module}", QT / "include" / f"Qt{module}",
                        dirs_exist_ok=True)
    for module in ("", *MODULES):
        directory = source / "lib/cmake" / f"Qt5{module}"
        for path in directory.glob("*.cmake"):
            # QtGui 自动扫描插件；当前应用只分发 QPA，不引入其他模块。
            if "Plugin" not in path.name or "QOpenHarmonyPlatformIntegration" in path.name:
                copy_changed(path, QT / "lib/cmake" / directory.name / path.name)
    shutil.copytree(source / "mkspecs", QT / "mkspecs", dirs_exist_ok=True)
    for tool in ("moc", "rcc", "qmake", "uic"):
        copy_changed(source / "bin" / tool, QT / "bin" / tool)
    (QT / "bin/qt.conf").write_text("[Paths]\nPrefix=..\n")
    libraries = {f"libQt5{module}.so": source / "lib" / f"libQt5{module}.so"
                 for module in MODULES}
    libraries["libplugins_platforms_qopenharmony.so"] = required[-1]
    for name, path in libraries.items():
        header = path.read_bytes()[:20]
        if (header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[16:18], "little") != 3
                or int.from_bytes(header[18:20], "little") != 183):
            raise SystemExit(f"不是 ARM64 ELF 动态库：{path}")
        copy_changed(path, LIBS / name)
        link = (QT / "plugins/platforms" / name if name.startswith("libplugins_")
                else QT / "lib" / name)
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.is_symlink():
            link.unlink()
        elif link.exists():
            raise SystemExit(f"不覆盖已有非符号链接：{link}")
        link.symlink_to(os.path.relpath(LIBS / name, link.parent))
    notices = ROOT / "entry/src/main/resources/rawfile/third_party_licenses/qt"
    if not license_source or not (license_source / "LICENSE.LGPL3").is_file():
        raise SystemExit("请用 --license-source 指定对应 Qt 源码目录（包含 LICENSE.LGPL3）")
    for path in license_source.rglob("*"):
        if path.is_file() and (path.name.startswith(("LICENSE", "COPYING", "NOTICE"))
                               or path.name == "qt_attribution.json"):
            relative = path.relative_to(license_source)
            if not any(part.startswith(".") for part in relative.parts):
                copy_changed(path, notices / relative)
    metadata = {
        "version": "5.15.12", "target": "OpenHarmony arm64-v8a",
        "host": f"{platform.system()}-{platform.machine()}",
        "libraries": {name: hashlib.sha256((LIBS / name).read_bytes()).hexdigest()
                      for name in libraries},
    }
    (QT / "sdk.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"[HUIWEN_QT] 已准备项目内 SDK：{QT}；运行库：{LIBS}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--license-source", type=Path)
    args = parser.parse_args()
    settings = properties()
    sdk = args.source or os.environ.get("QT_PREFIX") or settings.get("qt.sdk.dir")
    if not sdk:
        parser.error("请提供 --source 或配置 qt.sdk.dir")
    prepare(Path(sdk), args.license_source)
