# 固定 Qt/CEF 开发依赖

`manifest.json` 固定 Qt 5.15.12、CEF 迁入快照、9 个 ARM64 运行库及逐文件 SHA-256。
开发主机为 macOS ARM64，应用目标为 OpenHarmony ARM64；不支持直接复用这些宿主工具到 Windows/Linux。

## 使用

克隆子模块、安装 DevEco SDK 并复制根目录构建模板后，首次 Run 的 `PrepareNativeDependencies`
调用 `third_party/prebuilt/prepare.py`，恢复 Qt/CEF，再执行 PDFium。Markdown 任务依赖该任务，
随后同步 CEF 核心数据和构建 Markdown 资源。Qt/CEF 本体不重编。
源码模块 `third_party/cef/web_engine/` 已在仓库中，因此 OHPM 先于 Run 解析依赖也能成功。
本机 `build-profile.json5` 若沿用旧生成目录，需将 web_engine.srcPath 改为
`./third_party/cef/web_engine`；首次克隆使用模板时已采用该路径。

下载包缓存位于 `.native-tools/prebuilt/`。完整恢复后可离线复用；本地库、头文件或工具
与锁定版本不一致时停止并保留原文件，避免静默覆盖自定义内核或混用版本。
只删除缺失文件时可从缓存恢复。升级自定义内核应同时更新配套文件及固定清单。

离线导入同一包：

```sh
python3 third_party/prebuilt/prepare.py --archive /绝对路径/huiwen-qt-cef-darwin-arm64-20261022-v1.tar.gz
```

也可通过 `HUIWEN_PREBUILT_ARCHIVE` 指定离线包，仍强制校验锁定哈希。
Python HNP 运行时、DevEco SDK、设备签名仍使用原项目配置，不包含在此依赖包中。

## 维护与发布

先通过 `third_party/qt/prepare.py`、`third_party/cef/prepare.py` 准备维护者环境，然后执行：

```sh
python3 third_party/prebuilt/pack.py \
  --version darwin-arm64-20261022-v1 \
  --url https://github.com/xristosstarnetgames439-del/HuiwenMCP/releases/download/native-deps-20261022-v1/huiwen-qt-cef-darwin-arm64-20261022-v1.tar.gz
```

脚本核对来源库哈希后，生成忽略目录中的 tar.gz 和可提交的 manifest.json。
包只包含固定开发依赖及许可文件，不包含签名、local.properties、PDFium 缓存、
用户文档或本项目构建出来的 huiwen_qt/huiwen_native/cefsimple 动态库。
固定包已发布至 [GitHub 开发快照](https://github.com/xristosstarnetgames439-del/HuiwenMCP/releases/tag/native-deps-20261022-v1)，首次 Run 使用清单中的下载地址。
本次按开发快照预发布；Chromium/厂商完整许可材料仍待补齐，未声明生产分发就绪。

验证依赖恢复逻辑，不编译或打包应用：

```sh
python3 third_party/prebuilt/check.py
```

打包排序与时间戳固定；恢复时检查包哈希、文件哈希、执行权限和符号链接，
拒绝路径越界、外部链接、不匹配的已有文件，下载失败不覆盖有效本地依赖。
连接提前结束时按已接收的字节续传，最多三次；服务器必须正确响应 Range，整包仍需通过哈希校验。
