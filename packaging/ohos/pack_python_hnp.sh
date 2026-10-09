#!/usr/bin/env bash
# 将已解压的鸿蒙 arm64 Python 运行时打成应用私有 HNP。
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "用法: $0 <鸿蒙 Python 安装目录>" >&2
  exit 2
fi

runtime_dir=$(cd "$1" && pwd)
project_dir=$(cd "$(dirname "$0")/../.." && pwd)
python_bin="$runtime_dir/bin/python3"
markitdown_src="$project_dir/third_party/markitdown/packages/markitdown/src/markitdown"
if [[ ! -x "$python_bin" || ! -d "$runtime_dir/lib" ]]; then
  echo "运行时目录必须包含可执行的 bin/python3 和 lib/" >&2
  exit 2
fi
if [[ $(file -L -b "$python_bin") != *"ARM aarch64"* ]]; then
  echo "bin/python3 不是鸿蒙 arm64 ELF，请勿使用本机 macOS Python" >&2
  exit 2
fi
if [[ ! -f "$markitdown_src/__init__.py" ]]; then
  echo "缺少 MarkItDown 源码；请先运行 git submodule update --init" >&2
  exit 2
fi

hnpcli=${HNPCLI:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hnpcli}
if [[ ! -x "$hnpcli" ]]; then
  echo "找不到 hnpcli，可通过 HNPCLI 环境变量指定工具路径" >&2
  exit 2
fi

stage=$(mktemp -d)
trap 'rm -r "$stage"' EXIT
cp -pR "$runtime_dir/." "$stage/"
mkdir -p "$stage/share/huiwen" "$project_dir/hnp/arm64-v8a"
site_packages="$stage/lib/python3.14/site-packages"
if [[ ! -d "$site_packages" ]]; then
  echo "当前打包脚本要求鸿蒙 Python 3.14 运行时" >&2
  exit 2
fi

# 仅安装 PDF 路线所需的目标架构 wheel；MarkItDown 源码由固定版本子模块提供。
"${HOST_PYTHON:-python3}" -m pip install --disable-pip-version-check --no-input --no-compile \
  --platform musllinux_1_2_aarch64 --implementation cp --python-version 3.14 --abi cp314 \
  --only-binary=:all: --target "$site_packages" \
  -r "$project_dir/packaging/ohos/requirements-pdf.txt"
cp -R "$markitdown_src" "$site_packages/markitdown"
patch -s -d "$site_packages" -p1 < "$project_dir/packaging/ohos/markitdown-no-magika.patch"
# 汇文公开 API 与第三方库同处 HNP；页面只通过 NAPI 调用固定命令。
cp -R "$project_dir/converter/huiwen_converter" "$site_packages/huiwen_converter"
mkdir -p "$stage/share/licenses/markitdown"
cp "$project_dir/third_party/markitdown/LICENSE" "$stage/share/licenses/markitdown/LICENSE"
cp "$project_dir/third_party/markitdown/packages/markitdown/ThirdPartyNotices.md" \
  "$stage/share/licenses/markitdown/ThirdPartyNotices.md"
cp "$project_dir/packaging/ohos/python_smoke.py" "$stage/share/huiwen/python_smoke.py"
cp "$project_dir/converter/pdf2md/convert.py" "$stage/share/huiwen/convert_pdf.py"
cp "$project_dir/packaging/ohos/hnp.json" "$stage/hnp.json"
"$hnpcli" pack -i "$stage" -o "$project_dir/hnp/arm64-v8a"
echo "已生成 $project_dir/hnp/arm64-v8a/huiwen_python.hnp"
