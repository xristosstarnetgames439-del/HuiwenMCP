"""从汇文 GitHub 正式发布中下载最新 PDF Skill，校验归档后安装到新目录。"""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import quote, unquote
from urllib.request import Request, urlopen
import zipfile

REPOSITORY = "xristosstarnetgames439-del/HuiwenMCP"
NAME = "huiwen-pdf-to-md"
ARCHIVE = NAME + ".zip"


def fetch(url):
    request = Request(
        url, headers={"User-Agent": NAME, "Accept": "application/vnd.github+json"}
    )
    with urlopen(request, timeout=60) as response:
        return response.read()


def public_latest():
    # 优先跟随公开 Release 跳转，不消耗 GitHub 匿名 API 的有限额度。
    request = Request(f"https://github.com/{REPOSITORY}/releases/latest", method="HEAD")
    try:
        with urlopen(request, timeout=30) as response:
            url = response.geturl()
    except (HTTPError, URLError):
        return None
    prefix = f"https://github.com/{REPOSITORY}/releases/tag/"
    if url.lower().startswith(prefix.lower()):
        tag = unquote(url[len(prefix) :])
        if tag.startswith("pdf-skill-v"):
            base = f"https://github.com/{REPOSITORY}/releases/download/{quote(tag, safe='')}"
            return tag, {name: f"{base}/{name}" for name in (ARCHIVE, "SHA256SUMS")}
    return None


def latest_release():
    release = public_latest()
    if release:
        return release
    page = 1
    while True:
        releases = json.loads(
            fetch(
                f"https://api.github.com/repos/{REPOSITORY}/releases?per_page=100&page={page}"
            )
        )
        for release in releases:
            assets = {
                asset["name"]: asset["browser_download_url"]
                for asset in release["assets"]
            }
            if (
                not release["draft"]
                and not release["prerelease"]
                and release["tag_name"].startswith("pdf-skill-v")
                and {ARCHIVE, "SHA256SUMS"} <= assets.keys()
            ):
                return release["tag_name"], assets
        if len(releases) < 100:
            raise RuntimeError(
                "尚未找到正式发布的汇文 PDF Skill；请联系维护者发布，不能回退到上游包"
            )
        page += 1


def install(destination):
    destination = Path(destination).expanduser().absolute()
    if destination.exists():
        raise FileExistsError(
            f"安装目录已存在，请先保留旧版本或指定新目录：{destination}"
        )
    tag, assets = latest_release()
    checksums = fetch(assets["SHA256SUMS"]).decode("utf-8")
    expected = next(
        (
            line.split()[0]
            for line in checksums.splitlines()
            if len(line.split()) == 2 and line.split()[1].lstrip("*") == ARCHIVE
        ),
        None,
    )
    if not expected:
        raise ValueError("发布校验清单缺少 Skill 归档")
    content = fetch(assets[ARCHIVE])
    if hashlib.sha256(content).hexdigest() != expected:
        raise ValueError("发布包 SHA-256 校验失败，停止安装")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".huiwen-skill-", dir=destination.parent
    ) as temporary:
        stage = Path(temporary)
        with zipfile.ZipFile(io.BytesIO(content)) as archive:
            for entry in archive.infolist():
                path = PurePosixPath(entry.filename)
                if (
                    path.is_absolute()
                    or ".." in path.parts
                    or "\\" in entry.filename
                    or not path.parts
                    or path.parts[0] != NAME
                    or (entry.external_attr >> 16) & 0o170000 == 0o120000
                ):
                    raise ValueError("发布包包含无效路径或符号链接")
            archive.extractall(stage)
        root = stage / NAME
        if (
            not (root / "SKILL.md").is_file()
            or not (root / "assets/huiwen_converter.pyz").is_file()
        ):
            raise ValueError("发布包缺少 Skill 入口或转换业务包")
        root.rename(destination)
    print(
        json.dumps({"version": tag, "installed": str(destination)}, ensure_ascii=False)
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", type=Path, required=True)
    install(parser.parse_args().dest)
