// ArkTS 调用 libhuiwen_native.so 时使用的类型声明。
interface HuiwenNative {
  ping(): string;
  checkPython(): Promise<string>;
  // 异步把沙箱 PDF 写成 MD；成功返回输出路径，失败拒绝 Promise。
  convertPdf(inputPath: string, outputPath: string): Promise<string>;
}

declare const nativeBridge: HuiwenNative;
export default nativeBridge;
