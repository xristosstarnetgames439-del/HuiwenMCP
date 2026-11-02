"""验证两次 Run 构建会把修改后的 PDFium 源码更新到 libs 和 HAP；不安装设备。"""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import zipfile

from check_workspace_hap import check_hap
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.qt.prepare import ROOT

SOURCE = ROOT / "third_party/pdfium/source/fpdfsdk/fpdf_view.cpp"
LIBRARY = ROOT / "entry/libs/arm64-v8a/libpdfium.so"
HAP = ROOT / "entry/build/default/outputs/default/entry-default-signed.hap"


def check_update(hvigor):
    original = SOURCE.read_text()
    if "HUIWEN_RUN_UPDATE_PROBE_BEGIN" in original:
        raise SystemExit("源码已有构建探针，请先检查并清理之前中断的验证")
    command = [str(hvigor), "assembleHap", "--mode", "module", "-p", "product=default",
               "-p", "module=entry@default", "--no-daemon"]
    env = dict(os.environ)
    for name, location in {
        "NODE_HOME": "/Applications/DevEco-Studio.app/Contents/tools/node",
        "JAVA_HOME": "/Applications/DevEco-Studio.app/Contents/jbr/Contents/Home",
        "DEVECO_SDK_HOME": "/Applications/DevEco-Studio.app/Contents/sdk",
    }.items():
        if Path(location).is_dir():
            env.setdefault(name, location)
    previous = None
    probe = ""
    try:
        for version in (1, 2):
            marker = f"HUIWEN_RUN_PROBE_v{version}"
            probe = ('\n// HUIWEN_RUN_UPDATE_PROBE_BEGIN\n'
                     'extern "C" FPDF_EXPORT const char* FPDF_CALLCONV Huiwen_RunBuildProbe() {\n'
                     f'  return "{marker}";\n'
                     '}\n// HUIWEN_RUN_UPDATE_PROBE_END\n')
            SOURCE.write_text(original + probe)
            subprocess.run(command, cwd=ROOT, env=env, check=True)
            check_hap(HAP)
            assert marker.encode() in LIBRARY.read_bytes(), "libs 中仍是旧 PDFium"
            with zipfile.ZipFile(HAP) as hap:
                packed = hap.read("libs/arm64-v8a/libpdfium.so")
            assert marker.encode() in packed, "HAP 中仍是旧 PDFium"
            digest = hashlib.sha256(packed).hexdigest()
            assert digest != previous, "两次源码修改没有更新 HAP 中的 PDFium"
            previous = digest
            print(f"RUN_UPDATE_PROBE_OK v{version} hap_pdfium_sha256={digest}", flush=True)
    finally:
        # 仅移除本测试加入的内容，保留验证期间可能发生的其他源码编辑。
        current = SOURCE.read_text()
        if probe and current.endswith(probe):
            SOURCE.write_text(current[:-len(probe)])
        elif current != original:
            raise RuntimeError("探针源码被其他操作改动，请检查 fpdf_view.cpp 后重新构建")
        subprocess.run(command, cwd=ROOT, env=env, check=True)
    check_hap(HAP)
    with zipfile.ZipFile(HAP) as hap:
        assert b"HUIWEN_RUN_PROBE_" not in hap.read("libs/arm64-v8a/libpdfium.so")
    assert b"HUIWEN_RUN_PROBE_" not in LIBRARY.read_bytes()
    print("RUN_UPDATE_OK: 源码 → 新 so → libs → HAP；探针已移除，最终包复验通过")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hvigor", type=Path,
                        default=Path("/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw"))
    args = parser.parse_args()
    check_update(args.hvigor.resolve())
