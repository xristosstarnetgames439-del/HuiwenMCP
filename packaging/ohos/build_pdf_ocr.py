"""Build the offline OCR payload for the existing Python HNP; no device access.

Only PNM input and LSTM OCR are enabled. Leptonica/Tesseract/libc++ are linked
statically, leaving the executable dependent solely on OHOS libc.so.
"""

import argparse
import ast
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / ".native-tools/pdf-ocr"
SDK = Path(
    os.environ.get(
        "OHOS_NDK_HOME",
        "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native",
    )
)
if (SDK / "native/build/cmake/ohos.toolchain.cmake").is_file():
    SDK = SDK / "native"
MODEL_REVISION = "87416418657359cb625c412a48b6e1d6d41c29bd"
SOURCES = {
    "jpeg.tar.gz": (
        "https://codeload.github.com/libjpeg-turbo/libjpeg-turbo/tar.gz/refs/tags/3.1.2",
        "560f6338b547544c4f9721b18d8b87685d433ec78b3c644c70d77adad22c55e6",
    ),
    "pillow.tar.gz": (
        "https://files.pythonhosted.org/packages/1c/3d/bb7fca845737cf9d7dbde16ed1843984665ff2e0a518f5db43e77ec540b9/pillow-12.3.0.tar.gz",
        "3b8182a766685eaa002637e28b4ec8d6b18819a0c71f579bf0dbaa5830297cce",
    ),
    "TESSDATA-LICENSE": (
        "https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/e12c65a915945e4c28e237a9b52bc4a8f39a0cec/LICENSE",
        "a6cba85bc92e0cff7a450b1d873c0eaa2e9fc96bf472df0247a26bec77bf3ff9",
    ),
    "tesseract.tar.gz": (
        "https://codeload.github.com/tesseract-ocr/tesseract/tar.gz/refs/tags/5.5.1",
        "a7a3f2a7420cb6a6a94d80c24163e183cf1d2f1bed2df3bbc397c81808a57237",
    ),
    "leptonica.tar.gz": (
        "https://codeload.github.com/DanBloomberg/leptonica/tar.gz/refs/tags/1.85.0",
        "c01376bce0379d4ea4bc2ec5d5cbddaa49e2f06f88242619ab8c059e21adf233",
    ),
    "chi_sim.traineddata": (
        "https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/e12c65a915945e4c28e237a9b52bc4a8f39a0cec/chi_sim.traineddata",
        "4fef2d1306c8e87616d4d3e4c6c67faf5d44be3342290cf8f2f0f6e3aa7e735b",
    ),
    "eng.traineddata": (
        f"https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/{MODEL_REVISION}/eng.traineddata",
        "7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2",
    ),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def download(name):
    url, checksum = SOURCES[name]
    target = CACHE / name
    if target.is_file() and sha(target) == checksum:
        return
    temporary = target.with_suffix(target.suffix + ".part")
    try:
        with urllib.request.urlopen(url, timeout=120) as response, temporary.open(
            "wb"
        ) as output:
            shutil.copyfileobj(response, output)
        if sha(temporary) != checksum:
            raise RuntimeError(f"OCR source checksum mismatch: {name}")
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)


def build(python_runtime):
    python_runtime = Path(python_runtime).resolve(strict=True)
    if not (python_runtime / "include/python3.14/Python.h").is_file():
        raise RuntimeError("需要鸿蒙 Python 3.14 的开发头文件")
    CACHE.mkdir(parents=True, exist_ok=True)
    cmake = SDK / "build-tools/cmake/bin/cmake"
    ninja = SDK / "build-tools/cmake/bin/ninja"
    toolchain = SDK / "build/cmake/ohos.toolchain.cmake"
    readelf = SDK / "llvm/bin/llvm-readelf"
    for tool in (cmake, ninja, toolchain, readelf):
        if not tool.is_file():
            raise RuntimeError(f"找不到鸿蒙构建工具：{tool}")
    runtime = CACHE / "runtime"
    manifest = runtime / "share/huiwen-ocr/build.json"
    fingerprint = hashlib.sha256(
        (
            sha(Path(__file__))
            + sha(Path(__file__).parent / "pillow/CMakeLists.txt")
            + str(python_runtime)
            + sha(python_runtime / "bin/python3")
            + sha(python_runtime / "lib/libz.so")
            + "".join(
                sha(p)
                for p in sorted((python_runtime / "include/python3.14").rglob("*.h"))
            )
            + sha(toolchain)
            + str(SDK.resolve())
            + sha(SDK / "llvm/bin/clang")
        ).encode()
    ).hexdigest()
    if manifest.is_file():
        stored = json.loads(manifest.read_text())
        if stored.get("fingerprint") == fingerprint and all(
            (runtime / name).is_file() and sha(runtime / name) == digest
            for name, digest in stored["files"].items()
        ):
            print(f"[HUIWEN_OCR] CACHED {runtime}")
            return runtime
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(download, SOURCES))
    for archive, directory in [
        ("jpeg.tar.gz", "libjpeg-turbo-3.1.2"),
        ("pillow.tar.gz", "pillow-12.3.0"),
        ("leptonica.tar.gz", "leptonica-1.85.0"),
        ("tesseract.tar.gz", "tesseract-5.5.1"),
    ]:
        source = CACHE / directory
        if source.exists():
            shutil.rmtree(source)
        with tarfile.open(CACHE / archive) as package:
            package.extractall(CACHE, filter="data")
    prefix = CACHE / "install"
    common = [
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DOHOS_ARCH=arm64-v8a",
        "-DOHOS_PLATFORM=OHOS",
        "-DOHOS_STL=c++_static",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={prefix}",
        "-DBUILD_SHARED_LIBS=OFF",
    ]
    jobs = str(min(os.cpu_count() or 2, 6))

    def run(*args):
        subprocess.run([str(cmake), *args], check=True)

    run(
        "-S",
        str(CACHE / "leptonica-1.85.0"),
        "-B",
        str(CACHE / "lept-build"),
        *common,
        "-DBUILD_PROG=OFF",
        *[
            f"-DENABLE_{codec}=OFF"
            for codec in ["ZLIB", "PNG", "GIF", "JPEG", "TIFF", "WEBP", "OPENJPEG"]
        ],
    )
    run("--build", str(CACHE / "lept-build"), "--target", "install", "-j", jobs)
    run(
        "-S",
        str(CACHE / "tesseract-5.5.1"),
        "-B",
        str(CACHE / "tess-build"),
        *common,
        f"-DLeptonica_DIR={prefix}/lib/cmake/leptonica",
        "-DBUILD_TRAINING_TOOLS=OFF",
        "-DGRAPHICS_DISABLED=ON",
        "-DDISABLED_LEGACY_ENGINE=ON",
        "-DOPENMP_BUILD=OFF",
        "-DENABLE_NATIVE=OFF",
        "-DDISABLE_ARCHIVE=ON",
        "-DDISABLE_CURL=ON",
        "-DDISABLE_TIFF=ON",
        "-DLEPT_TIFF_RESULT=1",
    )
    run("--build", str(CACHE / "tess-build"), "--target", "tesseract", "-j", jobs)
    run(
        "-S",
        str(CACHE / "libjpeg-turbo-3.1.2"),
        "-B",
        str(CACHE / "jpeg-build"),
        *common,
        "-DENABLE_SHARED=OFF",
        "-DWITH_SIMD=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    )
    run("--build", str(CACHE / "jpeg-build"), "--target", "jpeg-static", "-j", jobs)
    pillow = CACHE / "pillow-12.3.0"
    # Read only the upstream literal source lists, never execute setup.py on host.
    lists = {}
    for node in ast.parse((pillow / "setup.py").read_text()).body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name):
            name = node.targets[0].id
            if name in {"_IMAGING", "_LIB_IMAGING"}:
                lists[name] = ast.literal_eval(node.value)
    source_files = [pillow / "src/_imaging.c", pillow / "src/libImaging/Mode.c"]
    source_files += [pillow / f"src/{name}.c" for name in lists["_IMAGING"]]
    source_files += [
        pillow / f"src/libImaging/{name}.c" for name in lists["_LIB_IMAGING"]
    ]
    source_list = CACHE / "pillow-sources.cmake"
    source_list.write_text(
        "set(PILLOW_SOURCES\n" + "\n".join(f'  "{p}"' for p in source_files) + "\n)\n"
    )
    run(
        "-S",
        str(Path(__file__).parent / "pillow"),
        "-B",
        str(CACHE / "pillow-build"),
        *common,
        f"-DPILLOW_SOURCE_LIST={source_list}",
        f"-DPILLOW_SOURCE={pillow}",
        f"-DPYTHON_RUNTIME={python_runtime}",
        f"-DJPEG_SOURCE={CACHE / 'libjpeg-turbo-3.1.2'}",
        f"-DJPEG_BUILD={CACHE / 'jpeg-build'}",
    )
    run("--build", str(CACHE / "pillow-build"), "-j", jobs)
    # Stage first; an interrupted build must not replace the previous valid runtime.
    stage = CACHE / "staging"
    if stage.exists():
        shutil.rmtree(stage)
    (stage / "bin").mkdir(parents=True)
    (stage / "share/tessdata").mkdir(parents=True)
    notices = stage / "share/huiwen-ocr"
    notices.mkdir()
    binary = stage / "bin/tesseract"
    shutil.copy2(CACHE / "tess-build/bin/tesseract", binary)
    subprocess.run([str(SDK / "llvm/bin/llvm-strip"), str(binary)], check=True)
    report = subprocess.check_output([str(readelf), "-h", "-d", str(binary)], text=True)
    import re

    if "AArch64" not in report or set(re.findall(r"\(NEEDED\).*\[(.*?)\]", report)) != {
        "libc.so"
    }:
        raise RuntimeError("OCR binary is not self-contained OHOS ARM64")
    pil_destination = stage / "lib/python3.14/site-packages/PIL"
    shutil.copytree(
        pillow / "src/PIL",
        pil_destination,
        ignore=shutil.ignore_patterns("*.pyi", "__pycache__"),
    )
    imaging = pil_destination / "_imaging.so"
    shutil.copy2(CACHE / "pillow-build/_imaging.so", imaging)
    subprocess.run([str(SDK / "llvm/bin/llvm-strip"), str(imaging)], check=True)
    report = subprocess.check_output(
        [str(readelf), "-h", "-d", str(imaging)], text=True
    )
    needed = set(re.findall(r"\(NEEDED\).*\[(.*?)\]", report))
    if "$ORIGIN/../../.." not in report or str(python_runtime) in report:
        raise RuntimeError("Pillow must locate HNP zlib relative to itself")
    if "AArch64" not in report or not needed <= {"libc.so", "libz.so.1", "libz.so"}:
        raise RuntimeError(f"Pillow is not OHOS ARM64: {needed}")
    nm = SDK / "llvm/bin/llvm-nm"

    def symbols(path, option):
        output = subprocess.check_output([str(nm), "-D", option, str(path)], text=True)
        return {
            line.split()[-1].split("@")[0]
            for line in output.splitlines()
            if line.split() and line.split()[-2] not in {"w", "v"}
        }

    exported = set()
    for library in (
        python_runtime / "bin/python3",
        python_runtime / "lib/libz.so",
        SDK / "sysroot/usr/lib/aarch64-linux-ohos/libc.so",
    ):
        exported.update(symbols(library, "--defined-only"))
    undefined = symbols(imaging, "--undefined-only")
    optional = {
        "__gmon_start__",
        "_ITM_deregisterTMCloneTable",
        "_ITM_registerTMCloneTable",
    }
    missing = undefined - exported - optional
    if missing:
        raise RuntimeError(f"Pillow target symbols unresolved: {sorted(missing)}")
    shutil.copy2(pillow / "LICENSE", notices / "PILLOW-LICENSE")
    shutil.copy2(
        CACHE / "libjpeg-turbo-3.1.2/LICENSE.md", notices / "LIBJPEG-TURBO-LICENSE"
    )
    shutil.copy2(CACHE / "libjpeg-turbo-3.1.2/README.ijg", notices / "README.ijg")
    for name in ("chi_sim.traineddata", "eng.traineddata"):
        shutil.copy2(CACHE / name, stage / "share/tessdata" / name)
    shutil.copy2(CACHE / "tesseract-5.5.1/LICENSE", notices / "TESSERACT-LICENSE")
    shutil.copy2(
        CACHE / "leptonica-1.85.0/leptonica-license.txt", notices / "LEPTONICA-LICENSE"
    )
    shutil.copy2(CACHE / "TESSDATA-LICENSE", notices / "TESSDATA-LICENSE")
    (notices / "SOURCES.json").write_text(json.dumps(SOURCES, indent=2) + "\n")
    files = {
        str(p.relative_to(stage)): sha(p)
        for p in sorted(stage.rglob("*"))
        if p.is_file()
    }
    (notices / "build.json").write_text(
        json.dumps({"fingerprint": fingerprint, "files": files}, indent=2) + "\n"
    )
    backup = CACHE / "previous-runtime"
    if backup.exists():
        shutil.rmtree(backup)
    if runtime.exists():
        runtime.rename(backup)
    try:
        stage.rename(runtime)
    except Exception:
        if backup.exists():
            backup.rename(runtime)
        raise
    if backup.exists():
        shutil.rmtree(backup)
    print(f"[HUIWEN_OCR] GENERATED {runtime}")
    return runtime


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("python_runtime", help="鸿蒙 Python 3.14 安装目录（含头文件）")
    build(parser.parse_args().python_runtime)
