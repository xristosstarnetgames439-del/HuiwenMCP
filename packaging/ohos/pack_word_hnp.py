"""只收集 DOC 转换运行模块，打成独立私有 HNP；解释器复用 huiwen_python。"""

import ast
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "third_party/doc2docx/source"
OUTPUT = ROOT / "hnp/arm64-v8a/huiwen_doc2docx.hnp"


def build() -> None:
    core = SOURCE / "src/doc2docx"
    if not (core / "converter.py").is_file():
        raise RuntimeError("缺少 DOC 转换核心，请执行 git submodule update --init --recursive")
    inputs = {"__main__.py": (ROOT / "converter/word2docx/convert.py").read_bytes(),
              "huiwen_converter/__init__.py": b"from ._word_adapter import doc_to_docx\n",
              "huiwen_converter/_word_adapter.py":
                  (ROOT / "converter/huiwen_converter/_word_adapter.py").read_bytes(),
              "licenses/doc2docx/LICENSE": (SOURCE / "LICENSE").read_bytes()}
    for path in sorted(core.rglob("*.py")):
        if path.name in ("cli.py", "__main__.py") or "__pycache__" in path.parts:
            continue
        for node in ast.walk(ast.parse(path.read_text(), filename=str(path))):
            names = []
            if isinstance(node, ast.Import):
                names = [item.name.split(".")[0] for item in node.names]
            elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
                names = [node.module.split(".")[0]]
            if set(names) - sys.stdlib_module_names - {"doc2docx"}:
                raise RuntimeError(f"转换核心新增非标准库依赖：{path.name}: {names}")
        inputs["doc2docx/" + path.relative_to(core).as_posix()] = path.read_bytes()
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(inputs.items())}
    package = io.BytesIO()
    with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(inputs.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    contents = package.getvalue()
    metadata = json.dumps({"schema": 1, "upstream": "https://github.com/HuiTurn/doc2docx",
                           "sources": manifest}, sort_keys=True).encode()
    configuration = (ROOT / "packaging/ohos/hnp-word.json").read_bytes()
    tool = os.environ.get("HNPCLI", "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hnpcli")
    if not Path(tool).is_file():
        raise RuntimeError("找不到 hnpcli，请配置 HNPCLI 为鸿蒙 SDK 打包工具")
    digest = hashlib.sha256(contents + metadata + configuration + Path(__file__).read_bytes() +
                            Path(tool).read_bytes()).hexdigest()
    stamp = ROOT / ".hnp-cache/word-build.sha256"
    if OUTPUT.is_file() and stamp.is_file():
        archive_hash = hashlib.sha256(OUTPUT.read_bytes()).hexdigest()
        if stamp.read_text().strip() == f"{digest} {archive_hash}":
            print(f"[HUIWEN_WORD_HNP] CACHED {OUTPUT}")
            return
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".word-", dir=OUTPUT.parent) as directory:
        stage = Path(directory) / "stage"
        shared = stage / "share/huiwen"
        shared.mkdir(parents=True)
        (stage / "hnp.json").write_bytes(configuration)
        (shared / "convert_word.pyz").write_bytes(contents)
        (shared / "build.json").write_bytes(metadata)
        shutil.copyfile(SOURCE / "LICENSE", shared / "LICENSE.doc2docx")
        subprocess.run([tool, "pack", "-i", str(stage), "-o", directory], check=True)
        artifact = Path(directory) / OUTPUT.name
        if not artifact.is_file() or not artifact.stat().st_size:
            raise RuntimeError("DOC HNP 未生成，停止打包")
        with zipfile.ZipFile(artifact) as archive:
            if archive.testzip() is not None:
                raise RuntimeError("DOC HNP 归档校验失败")
        signature = hashlib.sha256(artifact.read_bytes()).hexdigest()
        stamp_file = Path(directory) / "build.sha256"
        stamp_file.write_text(f"{digest} {signature}\n")
        artifact.replace(OUTPUT)
        stamp_file.replace(stamp)
    print(f"[HUIWEN_WORD_HNP] GENERATED {OUTPUT} bytes={OUTPUT.stat().st_size}")


if __name__ == "__main__":
    try:
        build()
    except Exception as error:
        raise SystemExit(f"[HUIWEN_WORD_HNP] FAILED {error}")
