/* 汇文截图入口与全屏覆盖层：框选、标注、保存 PNG 和系统剪贴板。 */
#ifndef HUIWEN_SCREEN_CAPTURE_H
#define HUIWEN_SCREEN_CAPTURE_H

#include <QString>
#include <functional>

class ScreenCapture
{
  public:
    using FinishCallback = std::function<void(bool, const QString &, const QString &)>;

    static ScreenCapture *getInstance();

    // 启动框选截图。回调参数依次为：成功、图片本地路径、错误信息。
    // 必须在 Qt GUI 线程调用。
    void startCapture(bool ignoreActiveWindow, FinishCallback callback);

  private:
    ScreenCapture() = default;
};

#endif // HUIWEN_SCREEN_CAPTURE_H
