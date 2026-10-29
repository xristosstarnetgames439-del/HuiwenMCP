"""检查工作区 HAP 的原生库、Ability、Python 环境及业务包，避免漏打运行依赖。"""

import io
import json
import sys
import zipfile
from pathlib import Path


def check_hap(path: Path) -> None:
    libraries = [
        "libhuiwen_native.so", "libhuiwen_qt.so", "libQt5Core.so", "libQt5Gui.so",
        "libQt5Widgets.so", "libQt5Concurrent.so", "libplugins_platforms_qopenharmony.so",
        "libpdfium.so",
    ]
    with zipfile.ZipFile(path) as hap:
        for library in libraries:
            entry = f"libs/arm64-v8a/{library}"
            assert hap.namelist().count(entry) == 1, f"缺失或重复原生库：{entry}"
            header = hap.read(entry)[:20]
            assert header[:5] == b"\x7fELF\x02", f"不是 ELF64：{library}"
            assert header[5] == 1 and int.from_bytes(header[18:20], "little") == 183, library
        module = json.loads(hap.read("module.json"))["module"]
        assert module["srcEntry"] == "./ets/abilitystage/QtAbilityStage.ets"
        abilities = {item["name"] for item in module["abilities"]}
        assert {"EntryAbility", "QtWorkspaceAbility"} <= abilities
        assert {"package": "huiwen_python.hnp", "type": "private"} in module["hnpPackages"]
        assert len(hap.read("hnp/arm64-v8a/huiwen_python.hnp")) > 0
        with zipfile.ZipFile(io.BytesIO(hap.read("resources/rawfile/python/huiwen_converter.pyz"))) as bundle:
            assert bundle.testzip() is None, "Python 业务包校验失败"
            for name in ("__main__.py", "huiwen_converter/__init__.py", "markitdown/__init__.py",
                         "licenses/markitdown/LICENSE", "licenses/markitdown/ThirdPartyNotices.md"):
                assert name in bundle.namelist(), f"Python 业务包缺失入口或许可：{name}"
        for name in ("qt", "pdfium"):
            prefix = f"resources/rawfile/third_party_licenses/{name}/"
            assert any(entry.startswith(prefix) and "LICENSE" in entry for entry in hap.namelist()), name
    print("HAP_CHECK_OK: 8 个 ARM64 原生库、两个 Ability、Python 环境 HNP、业务 pyz 与许可材料")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("用法：python3 packaging/ohos/check_workspace_hap.py <signed.hap>")
    check_hap(Path(sys.argv[1]))
