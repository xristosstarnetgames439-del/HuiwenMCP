"""校验独立 DOC HNP，并可在宿主机执行包内转换核心；不连接设备。"""

import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", nargs="*", type=Path)
    parser.add_argument("--hnp", type=Path, default=ROOT / "hnp/arm64-v8a/huiwen_doc2docx.hnp")
    arguments = parser.parse_args()
    with zipfile.ZipFile(arguments.hnp) as package:
        assert package.testzip() is None
        name = next(name for name in package.namelist() if name.endswith("/share/huiwen/convert_word.pyz"))
        payload = package.read(name)
        metadata = json.loads(package.read(name.replace("convert_word.pyz", "build.json")))
    with zipfile.ZipFile(io.BytesIO(payload)) as code:
        assert code.testzip() is None
        assert set(code.namelist()) == set(metadata["sources"])
        for name, expected in metadata["sources"].items():
            assert hashlib.sha256(code.read(name)).hexdigest() == expected, name
        assert "doc2docx/converter.py" in code.namelist()
        assert "doc2docx/cli.py" not in code.namelist()
        assert "licenses/doc2docx/LICENSE" in code.namelist()
    with tempfile.TemporaryDirectory(prefix="huiwen-word-check-") as directory:
        root = Path(directory)
        app = root / "convert_word.pyz"
        app.write_bytes(payload)
        for index, sample in enumerate(arguments.samples):
            source = sample.resolve(strict=True)
            original = hashlib.sha256(source.read_bytes()).digest()
            target = root / f"converted-{index}.docx"
            process = subprocess.run([sys.executable, "-I", str(app), str(source), str(target), "--json"],
                                     capture_output=True, text=True, timeout=60)
            assert process.returncode == 0, process.stdout + process.stderr
            marker = "HUIWEN_CONVERT_RESULT "
            result = json.loads(next(line[len(marker):] for line in process.stdout.splitlines()
                                     if line.startswith(marker)))
            assert result["code"] == 0 and result["outputPath"] == str(target)
            assert hashlib.sha256(source.read_bytes()).digest() == original, "转换不得修改原文件"
            with zipfile.ZipFile(target) as docx:
                assert docx.testzip() is None
                document = ET.fromstring(docx.read("word/document.xml"))
                namespace = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
                tables = len(document.findall(".//w:tbl", namespace))
                characters = sum(len(node.text or "") for node in document.findall(".//w:t", namespace))
            print(f"WORD_HNP_CONVERT_OK {source.name} chars={characters} tables={tables} bytes={target.stat().st_size}")
    print(f"WORD_HNP_CHECK_OK bytes={arguments.hnp.stat().st_size} samples={len(arguments.samples)}")
    if not arguments.samples:
        print("仅完成包结构与源码摘要校验，未执行 DOC 样本转换")


if __name__ == "__main__":
    main()
