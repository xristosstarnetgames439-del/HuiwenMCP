"""配置 Skill 独立 Python 环境，并检测定制转换核心、PDFium 与本机 OCR。"""

import argparse
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / "assets/huiwen_converter.pyz"
REQUIREMENTS = ROOT / "assets/requirements.txt"
PYTHON = ROOT / ".venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def probe():
    errors = []
    for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        name, expected = line.split("==")
        try:
            installed = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            installed = "missing"
        if installed != expected:
            errors.append(f"Python 依赖 {name}: 需要 {expected}，实际 {installed}")
    sys.path.insert(0, str(BUNDLE))
    try:
        from huiwen_converter import pdf_to_md
        from markitdown.converters._pdf_converter import PDF_STRUCTURE_ALGORITHM
        from markitdown.converters._pdf_raster import _library

        _library()
        converter = {"api": pdf_to_md.__module__, "algorithm": PDF_STRUCTURE_ALGORITHM}
    except Exception as error:
        errors.append(f"转换核心／PDFium 不可用：{error}")
        converter = {}
    python_ready = not errors
    binary = os.environ.get("HUIWEN_TESSERACT") or shutil.which("tesseract")
    required = set(os.environ.get("HUIWEN_OCR_LANGUAGE", "chi_sim").split("+")) | {
        "eng"
    }
    languages = set()
    try:
        if not binary:
            raise FileNotFoundError(
                "找不到 Tesseract，请安装本机 OCR 或设置 HUIWEN_TESSERACT"
            )
        command = [binary, "--list-langs"]
        if os.environ.get("HUIWEN_TESSDATA"):
            command += ["--tessdata-dir", os.environ["HUIWEN_TESSDATA"]]
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
        )
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        languages = set(result.stdout.splitlines())
        if required - languages:
            raise RuntimeError(
                "缺少 OCR 模型：" + ", ".join(sorted(required - languages))
            )
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        errors.append(str(error))
    ocr_ready = bool(binary) and required <= languages
    print(
        json.dumps(
            {
                "python": sys.version.split()[0],
                "python_ready": python_ready,
                "ocr_ready": ocr_ready,
                "converter": converter,
                "errors": errors,
            },
            ensure_ascii=False,
        )
    )
    return 0 if python_ready and ocr_ready else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="仅检测已经安装的环境，不安装依赖"
    )
    parser.add_argument("--probe", action="store_true", help=argparse.SUPPRESS)
    arguments = parser.parse_args()
    if not (3, 10) <= sys.version_info[:2] < (3, 15):
        raise RuntimeError("需要 Python 3.10–3.14，请用受支持的解释器重新执行")
    if not BUNDLE.is_file() or not REQUIREMENTS.is_file():
        raise FileNotFoundError(
            "缺少发布资源，请下载完整 Skill 发布包，而不是仅复制源码目录"
        )
    if arguments.probe:
        return probe()
    if not arguments.check:
        if not PYTHON.is_file():
            venv.EnvBuilder(with_pip=True).create(ROOT / ".venv")
        subprocess.run(
            [
                str(PYTHON),
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                "--no-input",
                "-r",
                str(REQUIREMENTS),
            ],
            stdout=sys.stderr,
            check=True,
        )
    if not PYTHON.is_file():
        print(
            json.dumps(
                {
                    "python_ready": False,
                    "ocr_ready": False,
                    "errors": ["尚未配置独立环境，请先运行 scripts/setup.py"],
                },
                ensure_ascii=False,
            )
        )
        return 1
    return subprocess.run(
        [str(PYTHON), "-X", "utf8", str(Path(__file__).resolve()), "--probe"]
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
