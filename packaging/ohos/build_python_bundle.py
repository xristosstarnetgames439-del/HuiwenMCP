"""将汇文转换 API 与 MarkItDown 源码打成独立 pyz，不复制 Python 环境或原生依赖。"""

import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "entry/src/main/resources/rawfile/python/huiwen_converter.pyz"


def build():
    source = ROOT / "third_party/markitdown/packages/markitdown/src/markitdown"
    if not (source / "__init__.py").is_file():
        raise SystemExit("缺少 MarkItDown 源码，请先初始化 third_party/markitdown 子模块")
    with tempfile.TemporaryDirectory(prefix="huiwen-python-") as directory:
        stage = Path(directory)
        # 只收集源码与包内资源；本机字节码和原生扩展不进入 zip 导入路径。
        ignored = shutil.ignore_patterns("__pycache__", "*.pyc", "*.pyo", "*.so", "*.dylib", "*.pyd")
        shutil.copytree(source, stage / "markitdown", ignore=ignored)
        shutil.copytree(ROOT / "converter/huiwen_converter", stage / "huiwen_converter", ignore=ignored)
        subprocess.run(["patch", "--batch", "-s", "-d", str(stage), "-p1", "-i",
                        str(ROOT / "packaging/ohos/markitdown-no-magika.patch")], check=True)
        shutil.copyfile(ROOT / "converter/pdf2md/convert.py", stage / "__main__.py")
        licenses = stage / "licenses/markitdown"
        licenses.mkdir(parents=True)
        shutil.copyfile(ROOT / "third_party/markitdown/LICENSE", licenses / "LICENSE")
        shutil.copyfile(ROOT / "third_party/markitdown/packages/markitdown/ThirdPartyNotices.md",
                        licenses / "ThirdPartyNotices.md")
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    # 固定归档时间及权限，源文件仅时间戳改变时不会重写成品。
                    entry = zipfile.ZipInfo(path.relative_to(stage).as_posix(), (1980, 1, 1, 0, 0, 0))
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    entry.external_attr = 0o100644 << 16
                    package.writestr(entry, path.read_bytes())
        contents = archive.getvalue()
    if OUTPUT.is_file() and OUTPUT.read_bytes() == contents:
        print(f"[HUIWEN_PY_BUNDLE] CACHED file={OUTPUT}", flush=True)
        return
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=OUTPUT.parent, delete=False) as output:
        temporary = Path(output.name)
    try:
        temporary.write_bytes(contents)
        temporary.replace(OUTPUT)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"[HUIWEN_PY_BUNDLE] GENERATED file={OUTPUT} bytes={len(contents)}", flush=True)


if __name__ == "__main__":
    build()
