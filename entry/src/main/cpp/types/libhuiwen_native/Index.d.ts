// ArkTS 调用 libhuiwen_native.so 时使用的类型声明。
interface HuiwenNative {
  ping(): string;
}

declare const nativeBridge: HuiwenNative;
export default nativeBridge;
