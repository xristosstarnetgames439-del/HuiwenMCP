"""PDF→MD 的 Skill 调用入口；仅启动独立环境中的固定业务包，不依赖鸿蒙界面。"""

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--overwrite", action="store_true")
    arguments = parser.parse_args()
    source = arguments.input.expanduser().resolve()
    target = arguments.output.expanduser().resolve()
    if target.exists() and not arguments.overwrite:
        parser.error("输出已存在；确认覆盖时才使用 --overwrite")
    python = (
        ROOT / ".venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    )
    bundle = ROOT / "assets/huiwen_converter.pyz"
    if not python.is_file() or not bundle.is_file():
        parser.error("缺少转换环境或业务包，请先下载完整 Skill 并执行 scripts/setup.py")
    return subprocess.run(
        [str(python), "-X", "utf8", "-B", "-u", str(bundle), str(source), str(target)]
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
