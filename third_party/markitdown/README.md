# MarkItDown 源码与项目兼容补丁

`source/` 是我们的 MarkItDown fork 子模块，核心 PDF 转换算法在该仓库维护。
`patches/no-magika.patch` 属于主项目，用于鸿蒙 Python 环境中按已知 PDF 扩展名转换，
不强制导入 Magika／ONNX Runtime。

项目根目录执行 `git submodule sync --recursive && git submodule update --init --recursive`。
`packaging/ohos/build_python_bundle.py` 从 `source/` 读取源码，在临时目录应用补丁，
与主项目转换 API 打成业务 pyz；不改动子模块源码。应用级打包仍由主仓库维护。

## 独立 PDF 转 Markdown Skill

Skill 源码位于 [skill/huiwen-pdf-to-md/SKILL.md](skill/huiwen-pdf-to-md/SKILL.md)，
属于主项目，`source/` 子模块保持独立。对外给 Agent 的入口链接：

https://github.com/xristosstarnetgames439-del/HuiwenMCP/blob/main/third_party/markitdown/skill/huiwen-pdf-to-md/SKILL.md

Agent 按入口下载最新正式 `pdf-skill-v*` Release，并在对方 Windows／macOS 创建 `.venv`、
安装本机依赖，配置 Tesseract 和语言模型，再通过固定脚本调用转换。
不用克隆鸿蒙工程，不下载 Qt／CEF／HNP，也不替换成上游未经汇文修改的 MarkItDown。

```text
third_party/markitdown/
├── source/                         定制转换核心子模块
├── patches/no-magika.patch         打包时应用，不修改子模块
├── build_skill.py                  独立发布归档构建
└── skill/huiwen-pdf-to-md/
    ├── SKILL.md                    Agent 的安装和调用入口
    ├── agents/openai.yaml          Codex 显示元数据
    ├── scripts/
    │   ├── download.py             查找最新正式 Skill，下载并校验 SHA-256
    │   ├── setup.py                独立环境安装、转换核心与 OCR 自检
    │   └── convert.py              固定 PDF→MD 命令接口
    └── references/environment.md   Windows／macOS 环境配置说明

发布包额外包含：
huiwen-pdf-to-md/
├── assets/
│   ├── huiwen_converter.pyz        与鸿蒙使用同一个构建函数生成的业务源码
│   ├── requirements.txt            与鸿蒙同源的 PDF Python 依赖锁定文件
│   └── manifest.json               版本、主项目和转换核心提交、工作区状态
└── licenses/HuiWenMCP-LICENSE      主项目封装许可；pyz 内保留 MarkItDown 许可
```

本机打包不安装运行依赖、不构建 HAP：

```sh
python3 third_party/markitdown/build_skill.py --version 0.1.0
```

生成 `.native-tools/skill-release/huiwen-pdf-to-md.zip` 与 `SHA256SUMS`，均不进入 Git。
包内不含解释器、原生库和 OCR 模型：由接收方 Agent 按说明配置本机版本。
`packaging/ohos/build_python_bundle.py --output <路径>` 可指定业务包输出位置；
省略参数时仍按鸿蒙现有流程输出到 rawfile，不改变现有应用调用。

审核并提交后，推送 `pdf-skill-v0.1.0` 形式的版本标签即可触发
`.github/workflows/release-pdf-skill.yml`，只初始化 MarkItDown 子模块并发布独立 Skill。
下载器从带完整附件的正式 Skill Release 中选择最新版本，不追随上游版本或普通应用 Release。
源码链接、可下载发布包、本机安装成功、各平台转换验收是不同阶段，发布前分别核对。
