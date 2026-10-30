/* Qt Widgets 工作区入口：默认最大化并保留系统边框，窗口实际曝光后通知鸿蒙关闭原首页。 */
#include "workspace/ConversionWindow.h"
#include <QApplication>
#include <QIcon>
#include <QTimer>
#include <QWindow>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#include "documentView/renderers/markdown/MarkdownPreviewWidget.h"
#include <QPointer>
#include <memory>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_OPENHARMONY
    // PDF 容器、分隔条和悬浮按钮按需创建原生节点，避免连带提升兄弟控件。
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif
    QApplication app(argc, argv);
#ifdef Q_OS_OPENHARMONY
    app.setQuitOnLastWindowClosed(false);
#endif
    app.setApplicationName("HuiwenMCP");
    app.setApplicationDisplayName(QStringLiteral("汇文MCP"));
    app.setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
#ifdef Q_OS_OPENHARMONY
    // 待迁移：窗口基类可接管展示/关闭钩子；Ability 身份与 NAPI 交接仍留在平台侧。
    std::unique_ptr<ConversionWindow> window;
    quint64 workspaceId = 0;
    auto &bridge = QtBridge::instance();
    QTimer readyTimer;
    readyTimer.setInterval(100);
    const auto synchronizeWindow = [&]
    {
        const auto id = QtBridge::workspaceId();
        if (id == workspaceId)
            return;
        readyTimer.stop();
        window.reset();
        workspaceId = id;
        if (id)
        {
            window = std::make_unique<ConversionWindow>();
            window->showMaximized();
            readyTimer.start();
        }
    };
    QObject::connect(&bridge, &QtBridge::workspaceChanged, &app, synchronizeWindow);
    QObject::connect(&readyTimer, &QTimer::timeout, &app,
                     [&]
                     {
                         if (window && workspaceId == QtBridge::workspaceId() &&
                             window->windowHandle() && window->windowHandle()->isExposed() &&
                             bridge.request("ready"))
                             readyTimer.stop();
                     });
    QObject::connect(&bridge, &QtBridge::closeRequested, &app,
                     [&]
                     {
                         if (!window)
                             return;
                         window->hide(); // 隐藏会停止预览定时器，关闭期间不再创建浏览器。
                         QPointer<ConversionWindow> closingWindow(window.get());
                         const auto id = workspaceId;
                         MarkdownPreviewWidget::closeWorkspaceBrowsers(
                             [closingWindow, id]
                             {
                                 if (closingWindow && id == QtBridge::workspaceId())
                                     QtBridge::instance().request("closed");
                             });
                     });
    synchronizeWindow();
#else
    ConversionWindow window;
    window.showMaximized();
#endif
    return app.exec();
}
