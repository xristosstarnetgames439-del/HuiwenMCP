"""Run 前增量构建鸿蒙 ARM64 PDFium，并部署已验证的动态库。"""

import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

if sys.version_info < (3, 9) or not hasattr(tarfile, "data_filter"):
    raise SystemExit(
        f"[HUIWEN_PDFIUM] 宿主 Python 不兼容：{sys.executable}（{sys.version.split()[0]}）。"
        "需要 Python >= 3.9 且支持 tarfile 安全解压过滤，建议使用 Python 3.12+。"
        "请通过 HOST_PYTHON 环境变量或 local.properties 中的 python.host.path "
        "指定解释器的绝对路径后重试。")

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from third_party.qt.prepare import ROOT, copy_changed

ADAPTER = ROOT / "third_party/pdfium"
SOURCE = ADAPTER / "source"
TOOLS = ROOT / ".native-tools/depot_tools"
OUTPUT = SOURCE / "out/ohos-arm64"
LIBRARY = ROOT / "entry/libs/arm64-v8a/libpdfium.so"
APIS = ("FPDF_InitLibrary", "FPDF_LoadDocument", "FPDF_RenderPageBitmap",
        "FPDFText_SetText", "FPDFPage_GenerateContent", "FPDF_SaveAsCopy")


def run(args, cwd=SOURCE, **kwargs):
    kwargs.setdefault("check", True)
    return subprocess.run([str(arg) for arg in args], cwd=cwd, **kwargs)


def native_sdk():
    roots = [os.environ.get("OHOS_NDK_HOME"), os.environ.get("OHOS_SDK_HOME"),
             os.environ.get("DEVECO_SDK_HOME"),
             "/Applications/DevEco-Studio.app/Contents/sdk"]
    for root in filter(None, roots):
        for candidate in (Path(root), Path(root) / "native",
                          Path(root) / "default/openharmony/native"):
            if (candidate / "llvm/bin/clang++").is_file() and (candidate / "sysroot").is_dir():
                return candidate.resolve()
    raise SystemExit("找不到鸿蒙 native SDK，请配置 OHOS_NDK_HOME")


def apply_overlays(reverse=False):
    for directory in ("build", "buildtools"):
        patch = ADAPTER / "patches" / f"chromium-{directory}.patch"
        if not (SOURCE / directory / ".git").exists():
            continue
        for section in patch.read_text().split("diff --git ")[1:]:
            data = ("diff --git " + section).encode()
            applied = run(["git", "apply", "--reverse", "--check", "-"], cwd=SOURCE / directory,
                          input=data, capture_output=True, check=False).returncode == 0
            # 迁移前的补丁使用旧工具链名称；只撤回能精确匹配的旧补丁段。
            legacy = data.replace(b"//huiwen_adapter:arm64", b"//huiwen:arm64")
            if not applied and legacy != data and run(
                    ["git", "apply", "--reverse", "--check", "-"], cwd=SOURCE / directory,
                    input=legacy, capture_output=True, check=False).returncode == 0:
                run(["git", "apply", "--reverse", "-"], cwd=SOURCE / directory, input=legacy)
            if reverse and applied:
                run(["git", "apply", "--reverse", "-"], cwd=SOURCE / directory, input=data)
            elif not reverse and not applied:
                run(["git", "apply", "-"], cwd=SOURCE / directory, input=data)


def exclude_generated(name):
    git_directory = run(["git", "rev-parse", "--absolute-git-dir"], capture_output=True, text=True).stdout.strip()
    exclude = Path(git_directory) / "info/exclude"
    exclude.parent.mkdir(exist_ok=True)
    entry = f"/{name}"
    if not exclude.exists() or entry not in exclude.read_text().splitlines():
        with exclude.open("a") as file:
            file.write(f"\n{entry}\n")


def sync_dependencies():
    # 只撤回我们自己的适配补丁，避免 gclient 拒绝更新脏依赖；不重置其他修改。
    apply_overlays(reverse=True)
    if not (TOOLS / "gclient").is_file():
        run(["git", "-c", "http.version=HTTP/1.1", "clone", "--depth", "1",
             "https://chromium.googlesource.com/chromium/tools/depot_tools.git", TOOLS], cwd=ROOT)
    # gclient 不管理主源码，避免 Run 覆盖用户在 fork 中的修改。
    skip = ("buildtools/reclient", "third_party/cpython3/host", "third_party/cpython3/linux-amd64",
            "third_party/depot_tools", "third_party/instrumented_libs", "third_party/nasm",
            "third_party/siso/cipd", "tools/code_coverage",
            "tools/memory", "tools/resultdb", "tools/skia_goldctl/mac_amd64",
            "tools/skia_goldctl/mac_arm64", "third_party/llvm-build/Release+Asserts",
            "third_party/ninja", "buildtools/mac", "buildtools/linux64", "buildtools/win",
            "third_party/rust-toolchain")
    # 上游 gclient 的 custom_deps 不过滤 GCS/CIPD；生成仅含 Git 源码的 DEPS，
    # 原始 DEPS 保持不变，GN/Clang 另按其中的固定版本获取。
    tree = ast.parse((SOURCE / "DEPS").read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id in ("recursedeps", "hooks")
                                               for target in node.targets):
            node.value = ast.List(elts=[], ctx=ast.Load())
        if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == "deps"
                                               for target in node.targets):
            pairs = []
            for key, value in zip(node.value.keys, node.value.values):
                kind = next((item.value for name, item in zip(value.keys, value.values)
                             if isinstance(name, ast.Constant) and name.value == "dep_type"), "git") \
                    if isinstance(value, ast.Dict) else "git"
                if key.value not in skip and kind == "git":
                    pairs.append((key, value))
            node.value = ast.Dict(keys=[key for key, _ in pairs], values=[value for _, value in pairs])
    generated = ".huiwen_DEPS"
    (SOURCE / generated).write_text(ast.unparse(tree) + "\n")
    exclude_generated(generated)
    solution = dict(name="pdfium/source", url="https://github.com/xristosstarnetgames439-del/pdfium.git",
                    managed=False, deps_file=generated,
                    custom_vars=dict(checkout_configuration="minimal", checkout_clang_tidy=False,
                                     checkout_clangd=False, checkout_clang_coverage_tools=False))
    (ROOT / "third_party/.gclient").write_text(f"solutions = {[solution]!r}\n")
    # gclient/vpython3 会从 PATH 再次查找 python3，沿用宿主解释器，
    # 避免 DevEco 的 PATH 回退到 Xcode 自带的旧版 Python。
    env = {**os.environ,
           "PATH": str(Path(sys.executable).parent) + os.pathsep + os.environ.get("PATH", ""),
           "DEPOT_TOOLS_UPDATE": "0",
           "VPYTHON_BYPASS": "manually managed python not supported by chrome operations",
           "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "http.version",
           "GIT_CONFIG_VALUE_0": "HTTP/1.1"}
    run([TOOLS / "gclient", "sync", "--nohooks", "--no-history", "-j", "6"],
        cwd=ROOT / "third_party", env=env)
    prepare_tools()


def prepare_tools():
    # 读取与当前源码一致的官方 DEPS；避免 gsutil 引导锁及隐式工具升级。
    scope = {"Str": str}
    scope["Var"] = lambda name: scope["vars"][name]
    exec(compile((SOURCE / "DEPS").read_text(), str(SOURCE / "DEPS"), "exec"), scope)
    host = "mac" if sys.platform == "darwin" else "linux"
    arch = "arm64" if platform.machine() in ("arm64", "aarch64") else "amd64"
    prefix = "Mac_arm64/" if host == "mac" and arch == "arm64" else "Mac/" if host == "mac" else "Linux_x64/"
    if host == "linux" and arch != "amd64":
        raise SystemExit("当前上游编译器包仅支持 Linux x64 或 macOS 构建主机")
    package = scope["deps"]["third_party/llvm-build/Release+Asserts"]
    obj = next(item for item in package["objects"]
               if item["object_name"].startswith(prefix + "clang-llvmorg"))
    cache = ROOT / ".native-tools"
    cache.mkdir(exist_ok=True)
    archive = cache / "clang.tar.xz"
    url = f"https://storage.googleapis.com/{package['bucket']}/{obj['object_name']}"
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != obj["sha256sum"]:
        print(f"[HUIWEN_PDFIUM] DOWNLOAD {obj['object_name']}", flush=True)
        with urllib.request.urlopen(url, timeout=120) as response, archive.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != obj["sha256sum"]:
        raise RuntimeError("官方 Clang 下载校验失败")
    target = SOURCE / "third_party/llvm-build/Release+Asserts"
    target.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as tar:
        tar.extractall(target, filter="data")
    gn_package = scope["deps"]["buildtools/mac" if host == "mac" else "buildtools/linux64"]["packages"][0]
    gn_name = gn_package["package"].replace("${{arch}}", arch)
    url = f"https://chrome-infra-packages.appspot.com/dl/{gn_name}/+/{gn_package['version']}"
    archive = cache / "gn.zip"
    with urllib.request.urlopen(url, timeout=120) as response:
        archive.write_bytes(response.read())
    target = SOURCE / "buildtools" / ("mac" if host == "mac" else "linux64")
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as package_zip:
        package_zip.extractall(target)
    (target / "gn").chmod(0o755)


def check_library(path, sdk):
    header = path.read_bytes()[:20]
    if (header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[16:18], "little") != 3
            or int.from_bytes(header[18:20], "little") != 183):
        raise RuntimeError(f"不是 ARM64 ELF 动态库：{path}")
    readelf = sdk / "llvm/bin/llvm-readelf"
    symbols = run([readelf, "--dyn-syms", "--wide", path], capture_output=True, text=True).stdout
    for api in APIS:
        if not any(line.split()[-1:] == [api] and " UND " not in line for line in symbols.splitlines()):
            raise RuntimeError(f"动态库没有导出必要接口：{api}")
    dynamic = run([readelf, "--dynamic", path], capture_output=True, text=True).stdout
    dependencies = [line.split("[")[1].split("]")[0] for line in dynamic.splitlines()
                    if "(NEEDED)" in line]
    # 仅依赖鸿蒙系统 C 库；其他库若出现，先解决部署，再允许生成 HAP。
    if any(name != "libc.so" for name in dependencies):
        raise RuntimeError(f"PDFium 存在未部署动态依赖：{dependencies}")
    return dependencies


def build(sync=False):
    if not (SOURCE / "public/fpdfview.h").is_file():
        raise SystemExit("缺少 PDFium 源码，请执行 git submodule update --init --recursive")
    sdk = native_sdk()
    deps_hash = hashlib.sha256((SOURCE / "DEPS").read_bytes()).hexdigest()
    stamp = ROOT / ".native-tools/pdfium-deps.sha256"
    # 即使仅构建动态库，GN 仍会解析 testing 对 test_fonts 的引用。
    # 旧版脚本曾跳过此依赖，已有同步标记也需要补齐它。
    if (sync or not stamp.exists() or stamp.read_text().strip() != deps_hash
            or not (SOURCE / "third_party/test_fonts/BUILD.gn").is_file()):
        print("[HUIWEN_PDFIUM] SYNC dependencies", flush=True)
        sync_dependencies()
        stamp.write_text(deps_hash + "\n")
    gn = SOURCE / "buildtools" / ("mac" if sys.platform == "darwin" else "linux64") / "gn"
    compiler = SOURCE / "third_party/llvm-build/Release+Asserts"
    if not gn.is_file() or not (compiler / "bin/clang++").is_file():
        raise SystemExit("PDFium 构建工具缺失，请加 --sync 重新准备依赖")
    apply_overlays()
    # GN 要求工具链位于源码根目录内；只生成忽略文件，适配模板由主项目维护。
    copy_changed(ADAPTER / "ohos/BUILD.gn", SOURCE / "huiwen_adapter/BUILD.gn")
    exclude_generated("huiwen_adapter/")
    args = dict(target_os="linux", target_cpu="arm64", custom_toolchain="//huiwen_adapter:arm64",
                ohos_ndk=str(sdk), clang_base_path=str(compiler),
                target_sysroot=str(sdk / "sysroot"), use_sysroot=False,
                is_debug=False, is_component_build=False, symbol_level=0,
                pdf_is_shared_lib=True, pdf_enable_v8=False, pdf_enable_xfa=False,
                pdf_use_skia=False, pdf_use_partition_alloc=False, pdf_is_standalone=False,
                clang_use_chrome_plugins=False, treat_warnings_as_errors=False,
                use_custom_libcxx=True, use_siso=False, use_remoteexec=False,
                icu_use_data_file=False, use_system_xcode=True,
                libyuv_use_sme=False, libyuv_use_sve=False)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    args_file = OUTPUT / "args.gn"
    content = "\n".join(f"{name} = {json.dumps(value)}" for name, value in args.items()) + "\n"
    if not args_file.exists() or args_file.read_text() != content:
        args_file.write_text(content)
    print("[HUIWEN_PDFIUM] BUILD current sources", flush=True)
    run([gn, "gen", OUTPUT, f"--script-executable={sys.executable}"])
    run([sdk / "build-tools/cmake/bin/ninja", "-C", OUTPUT, "pdfium", "-j", "4"])
    binary = OUTPUT / "libpdfium.so"
    dependencies = check_library(binary, sdk)
    # 原子替换；构建或验证失败时，不覆盖之前的有效库。
    if not LIBRARY.exists() or binary.read_bytes() != LIBRARY.read_bytes():
        temporary = LIBRARY.with_suffix(".so.tmp")
        temporary.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(binary, temporary)
        temporary.replace(LIBRARY)
    notices = ROOT / "entry/src/main/resources/rawfile/third_party_licenses/pdfium"
    for directory in (SOURCE / "third_party", SOURCE):
        paths = directory.rglob("*") if directory.name == "third_party" else directory.glob("*")
        for path in paths:
            if path.is_file() and path.name.startswith(("LICENSE", "COPYING", "NOTICE")):
                relative = path.relative_to(SOURCE)
                if not any(part.startswith(".") for part in relative.parts):
                    copy_changed(path, notices / relative)
    revision = run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    patch = run(["git", "diff", "HEAD", "--", "."], capture_output=True).stdout
    metadata = dict(source_commit=revision, working_diff_sha256=hashlib.sha256(patch).hexdigest(),
                    adapter_sha256={str(path.relative_to(ADAPTER)): hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in (ADAPTER / "ohos/BUILD.gn", *sorted((ADAPTER / "patches").glob("*.patch")))},
                    target="aarch64-linux-ohos", args=args, dependencies=dependencies,
                    sha256=hashlib.sha256(LIBRARY.read_bytes()).hexdigest())
    (LIBRARY.parent / "pdfium-build.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"[HUIWEN_PDFIUM] DEPLOYED {LIBRARY}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sync", action="store_true", help="重新同步锁定的构建依赖")
    options = parser.parse_args()
    try:
        build(options.sync)
    except (subprocess.CalledProcessError, RuntimeError) as error:
        raise SystemExit(f"[HUIWEN_PDFIUM] FAILED: {error}") from error
