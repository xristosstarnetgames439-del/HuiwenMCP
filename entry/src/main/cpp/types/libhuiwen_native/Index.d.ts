// ArkTS 调用 libhuiwen_native.so 时使用的类型声明。
interface HuiwenNative {
  ping(): string;
  checkPython(): Promise<string>;
  // 异步把沙箱 PDF 写成 MD；成功返回输出路径，失败拒绝 Promise。
  convertPdf(inputPath: string, outputPath: string): Promise<string>;
  // 新 Qt Ability 注册事件；回调在 ArkTS 线程处理选择、打开位置和窗口就绪。
  registerWorkspace(callback: (event: string) => void): void;
  clearWorkspace(): void;
  workspaceReply(result: string): void;
}

declare const nativeBridge: HuiwenNative;
export default nativeBridge;
