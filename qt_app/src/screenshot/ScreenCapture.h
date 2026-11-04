/* 汇文截图入口与全屏覆盖层：框选、标注、保存 PNG 和系统剪贴板。 */
#ifndef HUIWEN_SCREEN_CAPTURE_H
#define HUIWEN_SCREEN_CAPTURE_H

#include <QString>
#include <functional>

class ScreenCapture
{
  public:
    using FinishCallback = std::function<void(bool, const QString &, const QString &, bool)>;

    static ScreenCapture *getInstance();

    // 回调依次返回成功、路径、错误、是否为应用缓存图；用户另存文件只入库、不删除。
    // 必须在 Qt GUI 线程调用。
    void startCapture(bool ignoreActiveWindow, FinishCallback callback,
                      const QString &cacheDirectory = {});

  private:
    ScreenCapture() = default;
};

#endif // HUIWEN_SCREEN_CAPTURE_H
