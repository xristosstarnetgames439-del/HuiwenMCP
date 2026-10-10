/* Qt Widgets 工作区入口：默认最大化并保留系统边框，窗口实际曝光后通知鸿蒙关闭原首页。 */
#include "workspace/ConversionWindow.h"
#include <QApplication>
#include <QTimer>
#include <QWindow>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("HuiWenMCP");
    ConversionWindow window;
    window.showMaximized();
#ifdef Q_OS_OPENHARMONY
    QTimer readyTimer;
    readyTimer.setInterval(100);
    QObject::connect(&readyTimer, &QTimer::timeout, &window,
                     [&]
                     {
                         if (window.windowHandle() && window.windowHandle()->isExposed() &&
                             QtBridge::instance().request("ready"))
                             readyTimer.stop();
                     });
    readyTimer.start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { QtBridge::instance().request("close"); });
#endif
    return app.exec();
}
