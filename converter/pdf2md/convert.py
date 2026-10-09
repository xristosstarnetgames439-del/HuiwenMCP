"""HNP 固定的 PDF 命令入口；业务接口由 huiwen_converter.pdf_to_md 提供。"""

import sys

from huiwen_converter import pdf_to_md


def main() -> int:
    if len(sys.argv) != 3:
        raise ValueError("需要输入 PDF 路径和输出 MD 路径")
    pdf_to_md(sys.argv[1], sys.argv[2])
    print("HUIWEN_CONVERT_OK", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"PDF 转换失败：{error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
