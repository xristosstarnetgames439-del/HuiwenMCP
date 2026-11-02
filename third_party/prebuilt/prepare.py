"""恢复锁定的 Qt/CEF 开发依赖；不编译 Qt、CEF 或应用。"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import stat
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = "third_party/prebuilt/manifest.json"
PREFIXES = ("third_party/qt/", "third_party/cef/sdk/", "entry/libs/arm64-v8a/",
            "entry/src/main/resources/rawfile/third_party_licenses/qt/")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_path(root, name):
    relative = PurePosixPath(name)
    if (not name.startswith(PREFIXES) or relative.is_absolute() or ".." in relative.parts
            or "\\" in name or relative.as_posix() != name):
        raise RuntimeError(f"依赖包路径不合法：{name}")
    path = root / name
    if not path.parent.resolve().is_relative_to(root.resolve()):
        raise RuntimeError(f"依赖路径穿越符号链接：{name}")
    return path


def matches(path, record):
    if record["type"] == "symlink":
        return path.is_symlink() and os.readlink(path) == record["target"]
    return (path.is_file() and not path.is_symlink() and path.stat().st_size == record["bytes"]
            and stat.S_IMODE(path.stat().st_mode) == record["mode"] and sha256(path) == record["sha256"])


def prepare(root=ROOT, archive=None):
    root = root.resolve()
    manifest = json.loads((root / MANIFEST).read_text())
    host = f"{platform.system()}-{platform.machine()}"
    if host != manifest["host"]:
        raise RuntimeError(f"预编译开发工具支持 {manifest['host']}，当前为 {host}；需要对应宿主工具包")
    files = manifest["files"]
    paths = {name: safe_path(root, name) for name in files}
    missing = []
    for name, path in paths.items():
        if path.exists() or path.is_symlink():
            if not matches(path, files[name]):
                raise RuntimeError(f"本地依赖与锁定版本不同，保留原文件并停止：{name}")
        else:
            missing.append(name)
    if not missing:
        print(f"[HUIWEN_PREBUILT] cached version={manifest['version']}", flush=True)
        return
    cache = root / ".native-tools/prebuilt"
    cache.mkdir(parents=True, exist_ok=True)
    package = Path(archive).resolve() if archive else cache / manifest["archive"]
    if not archive and (not package.is_file() or sha256(package) != manifest["sha256"]):
        print(f"[HUIWEN_PREBUILT] download {manifest['url']}", flush=True)
        if not manifest["url"].startswith("https://"):
            raise RuntimeError("依赖下载地址必须使用 HTTPS")
        fd, temporary = tempfile.mkstemp(prefix="download-", dir=cache)
        try:
            with os.fdopen(fd, "wb") as output:
                received = 0
                for attempt in range(3):
                    headers = {"User-Agent": "HuiwenMCP-bootstrap"}
                    if received:
                        headers["Range"] = f"bytes={received}-"
                    request = urllib.request.Request(manifest["url"], headers=headers)
                    with urllib.request.urlopen(request, timeout=60) as response:
                        if received and (response.status != 206 or not response.headers.get(
                                "Content-Range", "").startswith(f"bytes {received}-")):
                            raise RuntimeError("服务器未正确响应 Qt/CEF 断点续传；请重试")
                        for block in iter(lambda: response.read(1024 * 1024), b""):
                            received += len(block)
                            if received > manifest["bytes"]:
                                raise RuntimeError("下载的依赖包超过锁定大小")
                            output.write(block)
                    if received == manifest["bytes"]:
                        break
                    print(f"[HUIWEN_PREBUILT] incomplete bytes={received}/{manifest['bytes']} attempt={attempt + 1}/3", flush=True)
                if received != manifest["bytes"]:
                    raise RuntimeError(f"Qt/CEF 依赖包下载不完整：{received}/{manifest['bytes']} bytes；请重试")
            if sha256(Path(temporary)) != manifest["sha256"]:
                raise RuntimeError("下载的 Qt/CEF 依赖包 SHA-256 不匹配")
            os.replace(temporary, package)
        finally:
            Path(temporary).unlink(missing_ok=True)
    if (not package.is_file() or package.stat().st_size != manifest["bytes"]
            or sha256(package) != manifest["sha256"]):
        raise RuntimeError(f"Qt/CEF 依赖包不存在或 SHA-256 不匹配：{package}")
    # 整包校验成功后在独立目录逐项解包；不使用 extractall，不覆盖本机已有开发依赖。
    with tempfile.TemporaryDirectory(prefix="extract-", dir=cache) as directory:
        staging = Path(directory)
        with tarfile.open(package, "r:gz") as tar:
            members = tar.getmembers()
            if len(members) != len(files) or {m.name for m in members} != set(files):
                raise RuntimeError("依赖包文件清单不匹配")
            for member in members:
                record = files[member.name]
                target = safe_path(staging, member.name)
                target.parent.mkdir(parents=True, exist_ok=True)
                if record["type"] == "symlink":
                    link = record["target"]
                    if (not member.issym() or member.linkname != link or Path(link).is_absolute()
                            or not (target.parent / link).resolve().is_relative_to(staging)):
                        raise RuntimeError(f"依赖符号链接不合法：{member.name}")
                    target.symlink_to(link)
                else:
                    if not member.isfile() or member.size != record["bytes"]:
                        raise RuntimeError(f"依赖文件类型或大小不匹配：{member.name}")
                    with tar.extractfile(member) as source, target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                    target.chmod(record["mode"])
                if not matches(target, record):
                    raise RuntimeError(f"依赖文件校验失败：{member.name}")
        for name in missing:
            path = paths[name]
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.parent.resolve().is_relative_to(root):
                raise RuntimeError(f"安装路径越界：{name}")
            if path.exists() or path.is_symlink():
                if not matches(path, files[name]):
                    raise RuntimeError(f"安装期间文件已被修改，停止：{name}")
                continue
            # 相邻临时文件 + replace，避免中断留下半个 .so 或工具。
            source = staging / name
            fd, temporary = tempfile.mkstemp(prefix=".huiwen-", dir=path.parent)
            os.close(fd)
            try:
                Path(temporary).unlink()
                if files[name]["type"] == "symlink":
                    Path(temporary).symlink_to(files[name]["target"])
                else:
                    shutil.copy2(source, temporary)
                os.replace(temporary, path)
            finally:
                Path(temporary).unlink(missing_ok=True)
    print(f"[HUIWEN_PREBUILT] installed version={manifest['version']} files={len(missing)}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--archive", type=Path, default=os.environ.get("HUIWEN_PREBUILT_ARCHIVE"))
    args = parser.parse_args()
    try:
        prepare(args.root, args.archive)
    except (OSError, RuntimeError, ValueError, tarfile.TarError) as error:
        raise SystemExit(f"[HUIWEN_PREBUILT] FAILED {error}") from error
