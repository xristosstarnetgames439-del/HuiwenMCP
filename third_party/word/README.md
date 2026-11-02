# PortOne Word 核心与项目构建适配

`source/` 是 <https://github.com/xristosstarnetgames439-del/docx-editor> 的源码子模块，
固定主项目记录的提交。PortOne 核心修改在子模块提交、推送到我们的 fork。
外层 `library.ts`、`build.mjs`、`check_build.mjs`、依赖锁文件及本说明属于主仓库。

本阶段仍只准备库，不创建预览／编辑页面，不改变 PDF／MD 页面行为。
上游 Apache-2.0 许可、第三方声明和实际打入包的依赖许可随资源分发。

## 克隆和 Run

在项目根目录执行：

```sh
git submodule sync --recursive && git submodule update --init --recursive
```

缺少源码时 Run 停止并提示该命令，不自动执行 Git 克隆或更新。
`PrepareMarkdownResources` 在 MD 构建后调用本目录 `build.mjs`，
构建失败阻断 HAP，签名前检查四个 Word 资源是否入包。

构建直接使用 `source/src/` 的 TypeScript 和 CSS，复用 DevEco Node／npm，
无需安装 pnpm、Node 22 或上游测试／站点工具。首次按锁文件准备独立的运行时依赖和 esbuild；
React 固定为 18.3.1，依赖完整时可离线构建。入口只导出库，不挂载编辑器。

输出到 `entry/src/main/resources/rawfile/word/`：

- `huiwen-word.js`：编辑器、DOCX 接口、React 和 `createRoot`。
- `huiwen-word.css`：编辑器及 ProseMirror 样式。
- `THIRD_PARTY_LICENSES.txt`：完整许可证／声明。
- `build.json`：源码、版本锁和产物的 SHA-256。

这些是 Web 资源，不生成额外 `.so`；每次 Run 构建，内容相同保留时间戳。

```sh
node third_party/word/build.mjs
node third_party/word/check_build.mjs
```

自检使用临时副本，不修改真实 fork。本次目录迁移仅做语法与格式检查，
未重新构建资源、编译 C++、打包 HAP 或运行设备；编辑器页面尚未接入。
