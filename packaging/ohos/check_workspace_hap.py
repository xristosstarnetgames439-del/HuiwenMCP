"""检查工作区 HAP 的原生库、Ability 与 Python HNP，避免仅编译成功却漏打运行依赖。"""

import json
import sys
import zipfile
from pathlib import Path


def check_hap(path: Path) -> None:
    libraries = [
        "libhuiwen_native.so", "libhuiwen_qt.so", "libQt5Core.so", "libQt5Gui.so",
        "libQt5Widgets.so", "libQt5Concurrent.so", "libplugins_platforms_qopenharmony.so",
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
    print("HAP_CHECK_OK: 7 个 ARM64 原生库、两个 Ability 与私有 Python HNP")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("用法：python3 packaging/ohos/check_workspace_hap.py <signed.hap>")
    check_hap(Path(sys.argv[1]))
