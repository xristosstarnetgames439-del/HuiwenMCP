// 汇文页面调用原生转换能力的入口；页面不直接依赖 NAPI 模块。
import nativeBridge from 'libhuiwen_native.so';

// 返回原生桥接状态，供首页显示模块是否就绪。
export function getNativeStatus(): string {
  return nativeBridge.ping();
}

// 异步把沙箱中的 PDF 转为 MD；成功返回输出路径，失败拒绝 Promise。
export function pdfToMd(inputPath: string, outputPath: string): Promise<string> {
  return nativeBridge.convertPdf(inputPath, outputPath);
}
