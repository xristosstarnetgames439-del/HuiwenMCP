// ArkTS 调用 libhuiwen_native.so 时使用的类型声明。
import { resourceManager } from '@kit.LocalizationKit';
export interface HarmonySandboxDocument {
  name: string;
  kind: string;
  mode: string;
  taskPath: string;
  inputPath: string;
  outputPath: string;
  openPath: string;
  lastUsedAt: number;
}

// 文件管理记录：只返回真实存在且可读的输入／输出文件。
export interface HarmonySandboxFile {
  name: string;
  path: string;
  role: string;
  kind: string;
  taskPath: string;
  workspaceName: string;
  mode: string;
  sourceKind: string;
  lastUsedAt: number;
  sizeBytes: number;
}

interface HuiwenNative {
  ping(): string;
  // 启动期从应用资源部署内置转换 pyz，不接收外部脚本。
  configurePython(resources: resourceManager.ResourceManager, cacheDir: string): void;
  checkPython(): Promise<string>;
  // 异步把沙箱 PDF 写成 MD；成功返回输出路径，失败拒绝 Promise。
  convertPdf(inputPath: string, outputPath: string): Promise<string>;
  harmonyImportDocument(filesDir: string, name: string, fd: number, kind: string,
    mode: string): Promise<HarmonySandboxDocument>;
  harmonyValidateOutput(filesDir: string, path: string): string;
  // 校验预览任务目录或 input/output 目录；转换前也能打开空输出目录。
  harmonyValidateTaskDirectory(filesDir: string, path: string): string;
  // 后台按最近使用时间降序分页；mode 为 all/preview/edit/convert，offset 默认 0。
  harmonyRecentDocuments(filesDir: string, mode: string, limit: number,
    offset?: number): Promise<HarmonySandboxDocument[]>;
  // 读取并校验 PDF/MD 预览或 PDF→MD 任务，Qt 确认载入后才更新使用时间。
  harmonyLoadTask(filesDir: string, taskPath: string): Promise<HarmonySandboxDocument>;
  // 异步读取文件快照，按工作区最近使用时间降序；查询不更新时间。
  harmonyListFiles(filesDir: string): Promise<HarmonySandboxFile[]>;
  // 将模式、文档类型与回调一起交给 Qt；截图使用 screenshot 模式和空 kind。
  // 回调仍在 ArkTS 线程处理系统操作。
  registerWorkspace(callback: (event: string) => void, mode: string, kind: string): void;
  clearWorkspace(): void;
  workspaceReply(result: string): void;
}

declare const nativeBridge: HuiwenNative;
export default nativeBridge;
