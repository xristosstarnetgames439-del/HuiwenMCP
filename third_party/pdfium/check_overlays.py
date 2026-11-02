"""在临时 Git 目录检查兼容补丁的首次应用、升级、幂等和撤回，不编译或改动实际依赖。"""

from pathlib import Path
import subprocess
import tempfile

import build as pdfium_build


def check():
    source = pdfium_build.SOURCE
    patch = (pdfium_build.ADAPTER / "patches/chromium-build.patch").read_bytes()
    legacy = patch.replace(b"//huiwen_adapter:arm64", b"//huiwen:arm64")
    files = ("config/clang/BUILD.gn", "gn_logs.gni")
    originals = {name: subprocess.check_output(
        ["git", "-C", str(source / "build"), "show", f"HEAD:{name}"]) for name in files}
    originals["gn_logs.gni"] += b"\n# Unrelated local change must survive.\n"
    try:
        with tempfile.TemporaryDirectory(prefix="huiwen-overlays-") as temporary:
            pdfium_build.SOURCE = Path(temporary)
            dependency = Path(temporary) / "build"
            dependency.mkdir()
            subprocess.run(["git", "init", "--quiet", str(dependency)], check=True)
            for name, data in originals.items():
                file = dependency / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(data)

            def contents():
                return {name: (dependency / name).read_bytes() for name in files}

            pdfium_build.apply_overlays()
            updated = contents()
            assert all(b"//huiwen_adapter:arm64" in data for data in updated.values())
            assert updated["gn_logs.gni"].endswith(originals["gn_logs.gni"].splitlines(keepends=True)[-1])
            pdfium_build.apply_overlays()
            assert contents() == updated, "重复应用改变了文件"
            pdfium_build.apply_overlays(reverse=True)
            assert contents() == originals, "新版补丁撤回未保留原有修改"

            for reverse in (False, True):
                subprocess.run(["git", "apply", "-"], cwd=dependency, input=legacy, check=True)
                pdfium_build.apply_overlays(reverse=reverse)
                assert contents() == (originals if reverse else updated), "旧补丁升级或撤回失败"
                if not reverse:
                    pdfium_build.apply_overlays(reverse=True)
            print("OVERLAYS_OK: 首次应用、旧补丁升级、重复应用及新旧补丁撤回通过；保留其他修改")
    finally:
        pdfium_build.SOURCE = source


if __name__ == "__main__":
    check()
