/* 鸿蒙截图桥：起窗前缓存原生像素图，Qt 取用后自动释放，不落盘。 */
#pragma once
#include <cstdint>
#include <memory>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <vector>

struct PreparedScreenshot
{
    uint32_t displayId;
    std::shared_ptr<OH_PixelmapNative> pixels;
};

// 仅供 Qt GUI 线程取走本次全部屏幕底图；读取后缓存即为空。
std::vector<PreparedScreenshot> TakePreparedScreenshots();
void InitHarmonyScreenshot(napi_env env, napi_value exports);
