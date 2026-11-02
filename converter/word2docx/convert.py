"""独立 HNP 的固定入口；只做 DOC→DOCX，不启动界面。"""

import argparse
import json
from pathlib import Path
import sys

directory = Path(__file__).resolve().parents[1]
if (directory / "huiwen_converter").is_dir():
    sys.path.insert(0, str(directory))
    sys.path.insert(0, str(directory.parent / "third_party/doc2docx/source/src"))
from huiwen_converter import doc_to_docx


def main() -> int:
    parser = argparse.ArgumentParser(description="静默 DOC → DOCX，不覆盖输入或已有输出")
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--json", action="store_true", help="供原生后台接口读取固定结果协议")
    arguments = parser.parse_args()
    try:
        output = doc_to_docx(arguments.input, arguments.output)
        result = {"code": 0, "message": "DOC 转 DOCX 完成", "outputPath": str(output)}
    except Exception as error:
        result = {"code": 2001, "message": str(error)[:512], "outputPath": ""}
    if arguments.json:
        print("HUIWEN_CONVERT_RESULT " + json.dumps(result, ensure_ascii=False), flush=True)
    elif result["code"]:
        print(result["message"], file=sys.stderr)
    else:
        print(result["outputPath"])
    return 0 if result["code"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
