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
if [[ ! -x "$python_bin" || ! -d "$runtime_dir/lib" ]]; then
  echo "运行时目录必须包含可执行的 bin/python3 和 lib/" >&2
  exit 2
fi
if [[ $(file -L -b "$python_bin") != *"ARM aarch64"* ]]; then
  echo "bin/python3 不是鸿蒙 arm64 ELF，请勿使用本机 macOS Python" >&2
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
cp "$project_dir/packaging/ohos/python_smoke.py" "$stage/share/huiwen/python_smoke.py"
cp "$project_dir/packaging/ohos/hnp.json" "$stage/hnp.json"
"$hnpcli" pack -i "$stage" -o "$project_dir/hnp/arm64-v8a"
echo "已生成 $project_dir/hnp/arm64-v8a/huiwen_python.hnp"
