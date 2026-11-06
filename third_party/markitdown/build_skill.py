"""复用鸿蒙业务包构建，生成可独立下载的 PDF Skill 和 SHA256SUMS，不打包 Qt／HNP。"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
NAME = "huiwen-pdf-to-md"


def build(version, output):
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("版本格式应为主版本.次版本.修订号")
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="huiwen-skill-") as temporary:
        stage = Path(temporary) / NAME
        shutil.copytree(
            Path(__file__).parent / "skill" / NAME,
            stage,
            ignore=shutil.ignore_patterns(".venv", "__pycache__", "*.pyc"),
        )
        assets = stage / "assets"
        assets.mkdir()
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "packaging/ohos/build_python_bundle.py"),
                "--output",
                str(assets / "huiwen_converter.pyz"),
            ],
            check=True,
        )
        shutil.copyfile(
            ROOT / "packaging/ohos/requirements-pdf.txt", assets / "requirements.txt"
        )
        (stage / "licenses").mkdir()
        shutil.copyfile(ROOT / "LICENSE", stage / "licenses/HuiWenMCP-LICENSE")
        revision = lambda path: subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"], text=True
        ).strip()
        manifest = {
            "version": version,
            "project_commit": revision(ROOT),
            "markitdown_commit": revision(ROOT / "third_party/markitdown/source"),
            "source_dirty": bool(
                subprocess.check_output(
                    ["git", "-C", str(ROOT), "status", "--porcelain"], text=True
                ).strip()
            ),
            "python": ">=3.10,<3.15",
        }
        (assets / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
        archive = output / (NAME + ".zip")
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    entry = zipfile.ZipInfo(
                        path.relative_to(stage.parent).as_posix(), (1980, 1, 1, 0, 0, 0)
                    )
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    entry.external_attr = 0o100644 << 16
                    package.writestr(entry, path.read_bytes())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / "SHA256SUMS").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    print(
        json.dumps(
            {"package": str(archive), "bytes": archive.stat().st_size, "sha256": digest}
        )
    )
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument(
        "--output", type=Path, default=ROOT / ".native-tools/skill-release"
    )
    arguments = parser.parse_args()
    build(arguments.version, arguments.output)
