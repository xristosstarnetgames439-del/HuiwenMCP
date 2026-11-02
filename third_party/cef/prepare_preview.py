"""从项目内 CEF SDK 准备汇文预览使用的 HAR 和资源，不重编 CEF 或 Qt。"""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.qt.prepare import ROOT, copy_changed


def prepare():
    sdk = ROOT / "third_party/cef/sdk"
    source = sdk / "web_engine"
    target = ROOT / "third_party/cef/web_engine"
    required = [sdk / "include/cef_app.h", source / "Index.ets",
                source / "src/main/resources/resfile/icudtl.dat"]
    required += [ROOT / "entry/libs/arm64-v8a" / name
                 for name in ("libcef.so", "libadapter.so", "libadapter_c.so", "libc++_shared.so")]
    for path in required:
        if not path.is_file():
            raise SystemExit(f"缺少 CEF 预览依赖，请先准备项目内 CEF SDK：{path}")
    # HAR 源码随 Git 固定，OHPM 同步先于 Run，不能依赖 Run 才生成模块。
    if not (target / "oh-package.json5").is_file():
        raise SystemExit(f"缺少项目内 CEF ArkTS 模块：{target}")
    # CEF 原生读取 resfile；Markdown 页面则由 Qt qrc 提供，不假定 rawfile 可直接 fopen。
    resources = source / "src/main/resources/resfile"
    for path in resources.rglob("*"):
        if path.is_file():
            copy_changed(path, ROOT / "entry/src/main/resources/resfile" / path.relative_to(resources))
    for path in (sdk / "licenses").glob("*"):
        if path.is_file():
            copy_changed(path, ROOT / "entry/src/main/resources/rawfile/third_party_licenses/cef" / path.name)
    copy_changed(ROOT / "third_party/cef/licenses/HAR-DEPENDENCIES.txt",
                 ROOT / "entry/src/main/resources/rawfile/third_party_licenses/cef/HAR-DEPENDENCIES.txt")
    print(f"[HUIWEN_CEF] PREVIEW_RESOURCES ready module={target}")


if __name__ == "__main__":
    prepare()
