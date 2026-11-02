# PortOne Word 预览／编辑库构建准备

本阶段只准备源码、依赖和 Run 资源构建链，不创建页面、不挂载编辑器、不改变 PDF／MD 功能。

## 源码和许可

- 上游：<https://github.com/portone-io/docx-editor>。
- 本项目 fork：<https://github.com/xristosstarnetgames439-del/docx-editor>。
- 源码：`../portone-docx/`，Git 子模块固定提交；初始版本 `0.6.7`，
  提交 `7271f4b5ce80b0acbad84f28ae04b24d2807e57d`。
- 许可：Apache-2.0；上游 `LICENSE` 和 `THIRD_PARTY_NOTICES.md` 保留，
  构建时与实际打入浏览器包的运行时依赖许可一起收集。
- 后续改库时直接修改子模块源码，先提交／推送 fork，再在主项目更新子模块指针。
  Run 不执行 `git pull`，不覆盖本地修改。

## 每次 Run

现有 `PrepareMarkdownResources` 任务在 MD 构建后调用 `packaging/ohos/build_word.mjs`。
`ConfigureCmake`、`CompileResource`、`PackageHap` 继续等待该任务完成，
Word 构建失败会阻止后续 HAP。现有签名前 HAP 检查也会检查 Word 四个资源是否入包。

构建直接读取 fork 的 `src/index.ts`、`src/core.ts` 和 CSS，包含 React 及 ProseMirror，
生成面向 Chromium 114 的自包含 IIFE，全局对象为 `HuiwenWord`。入口只导出库接口，
不创建业务窗口、React 根节点或编辑器实例。

输出目录：`entry/src/main/resources/rawfile/word/`，随 HAP 分发：

- `huiwen-word.js`：编辑器、`core` 导入导出接口、React 和 `createRoot`。
- `huiwen-word.css`：编辑器和 ProseMirror 样式。
- `THIRD_PARTY_LICENSES.txt`：源码和打入包的依赖许可证／声明。
- `build.json`：版本、锁文件、实际输入文件及产物的 SHA-256。

每次 Run 都执行源码打包；内容相同则保留输出文件和时间戳。只在构建、依赖引用检查、
许可证收集均成功后替换产物。该库打包为 Web 资源，不生成 `.so`，不重编 Qt／CEF。

## 克隆和环境

正常使用 `git clone --recurse-submodules`；若仅缺少本子模块，首次 Run 会按主项目固定提交
自动执行 `git submodule update --init -- third_party/portone-docx`，需要 Git 和网络。
其他现有子模块、Qt／CEF、SDK 和签名配置仍遵循主项目的准备流程。

复用 Hvigor／DevEco 随附 Node 和 npm，无需安装 pnpm、Node 22 或上游站点／测试工具链。
本目录使用与 MD 相同的 esbuild 直接编译 TypeScript，React 固定为 18.3.1；
`package-lock.json` 固定完整构建依赖树和完整性校验值。
首次 Run 自动 `npm ci --ignore-scripts`，安装匹配宿主平台的 esbuild 二进制；
依赖完整且锁文件／宿主平台未变时复用，后续可离线构建。设备不需要 Node 或 npm。

仅构建 Word 资源（在项目根目录）：

```sh
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node packaging/ohos/build_word.mjs
```

构建链自检（先执行一次上面的命令准备依赖）：

```sh
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node packaging/ohos/check_word_build.mjs
```

自检在临时目录修改 fork 副本，检查缓存复用、源码更新、失败保留原产物和 Hvigor 任务顺序。
不修改真实 fork，不编译 C++、打 HAP 或运行应用。浏览器行为、中文输入、剪贴板和 DOCX
保存后的排版效果，留到页面接入时验证；目标设为 Chromium 114 不等于已通过鸿蒙运行验证。
