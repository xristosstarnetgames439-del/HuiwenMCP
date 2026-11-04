# Python HNP 的离线 PDF OCR 配套

业务源码仍由 `build_python_bundle.py` 收集为独立 pyz。`pack_python_hnp.sh` 收集稳定运行时：

| 输入 | 随 HNP 的位置 | 职责 |
| --- | --- | --- |
| Tesseract 5.5.1 + Leptonica 1.85.0 | `bin/tesseract` | 只启用 LSTM／PNM；关闭训练、调试界面、HTTP、压缩包、OpenMP、额外图片编码器 |
| tessdata_best 简中模型、tessdata_fast 英文模型 | `share/tessdata/*.traineddata` | 完全离线识别；源码修订和 SHA-256 锁在 `build_pdf_ocr.py` |
| 现有项目 OHOS PDFium | `lib/libpdfium.so` | 复用 `entry/libs/arm64-v8a/libpdfium.so`，不修改 PDFium 源码或 Qt／CEF |
| Pillow 12.3.0 + libjpeg-turbo 3.1.2 | `lib/python3.14/site-packages/PIL/` | 使用目标 Python 头文件构建 `_imaging.so`；JPEG 静态链接，复用 HNP zlib；覆盖 Linux wheel 中的图片扩展 |
| 全部许可及来源清单 | `share/huiwen-ocr/` | Tesseract、Leptonica、模型、PDFium、Pillow 和 JPEG 许可 |

`build_pdf_ocr.py` 用现有 OHOS NDK 交叉编译，静态链接 C++ 标准库和 Leptonica；通过 readelf
确认 AArch64 且仅依赖系统 `libc.so` 后发布缓存。NDK 可用 `OHOS_NDK_HOME` 指定 native 目录或
其 openharmony 父目录。首次需要联网下载校验固定源码与模型；之后输入及成品校验一致则缓存命中。
下载／编译失败停止构建，不替换有效 HNP。

Pillow 只构建转换所需的基础图片处理与 JPEG／zlib 功能，不构建字体、GUI、TIFF、WebP 等可选扩展。
构建时检查其 AArch64 架构、动态依赖，并逐个核对未定义强符号是否由目标 Python、libc、zlib 提供。
HNP 打包先安装锁定的 Python 依赖，再用这套 OHOS Pillow 覆盖 Linux wheel，避免混入不可加载的扩展。
这仍是交叉编译和静态链接检查，不能代替设备端 `import PIL` 与真实转换验收。

This software is based in part on the work of the Independent JPEG Group.

现有 Run 顺序无需调整：先准备 PDFium，再在签名前的 HNP 准备中生成／复用 OCR 运行时。
环境 HNP 指纹包含 OCR 成品、模型、PDFium 以及许可；业务 Python 改动仍只重打包 pyz。
没有新增 Ability、NAPI、CEF 或 WebAssembly 转换链路。

本地组件验证（不连接设备）：

```sh
python3 packaging/ohos/build_pdf_ocr.py /path/to/ohos/python-runtime
HOST_PYTHON=/path/to/host/python bash packaging/ohos/pack_python_hnp.sh /path/to/ohos/python-runtime
python3 packaging/ohos/build_python_bundle.py
```

宿主运行 pyz 使用宿主 Tesseract/PDFium，不能执行 ARM64 OHOS 的程序。宿主转换、ELF 检查、
HNP 打包分别代表不同验证阶段；均不等于 HAP 部署或鸿蒙运行验收。
