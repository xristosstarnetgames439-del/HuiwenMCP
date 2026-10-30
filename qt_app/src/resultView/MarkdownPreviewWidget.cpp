/* 直接读取 MD 原文，交给内嵌 CEF 的离线页面渲染。 */
#include "MarkdownPreviewWidget.h"
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMoveEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTextCodec>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

#ifdef Q_OS_OPENHARMONY
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_load_handler.h"
#include "include/cef_menu_model.h"
#include "include/cef_process_message.h"
#include "include/cef_request_handler.h"
#include "include/cef_task.h"
#include <unordered_map>
#include "ohos/adapter_c/adapter_c.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <hilog/log.h>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
class UiTask final : public CefTask
{
  public:
    explicit UiTask(std::function<void()> action) : action_(std::move(action)) {}
    void Execute() override { action_(); }

  private:
    std::function<void()> action_;
    IMPLEMENT_REFCOUNTING(UiTask);
};

void postCef(std::function<void()> action) { CefPostTask(TID_UI, new UiTask(std::move(action))); }

class CefRuntime
{
  public:
    enum State
    {
        Idle,
        Starting,
        Ready,
        Failed,
        Stopped
    };
    std::atomic<State> state{Idle};
    std::atomic<bool> stopping{false};
    // 仅在 CEF UI 线程访问。
    std::vector<CefRefPtr<CefBrowser>> browsers;
    int creating = 0;
    std::function<void()> closeCompleted;
    static CefRuntime &instance()
    {
        static CefRuntime runtime;
        return runtime;
    }
    void start();
    void stop()
    {
        stopping = true;
        if (state == Ready)
            postCef(
                [this]
                {
                    if (browsers.empty() && creating == 0)
                        CefQuitMessageLoop();
                    else
                    {
                        const auto pending = browsers;
                        for (const auto &browser : pending)
                            browser->GetHost()->CloseBrowser(true);
                    }
                });
        if (thread_.joinable())
            thread_.join();
    }
    void closed(CefRefPtr<CefBrowser> browser)
    {
        browsers.erase(std::remove_if(browsers.begin(), browsers.end(),
                                      [browser](const auto &item) { return item->IsSame(browser); }),
                       browsers.end());
        finishClose();
    }
    void finishClose()
    {
        if (!browsers.empty() || creating != 0)
            return;
        if (stopping)
            CefQuitMessageLoop();
        else if (closeCompleted)
        {
            auto completed = std::move(closeCompleted);
            closeCompleted = {};
            // OnBeforeClose 返回后再通知 Qt，宿主窗口此时才允许销毁。
            postCef(
                [completed]
                {
                    QMetaObject::invokeMethod(qApp, completed, Qt::QueuedConnection);
                });
        }
    }
    ~CefRuntime() { stop(); }

  private:
    std::thread thread_;
};

class PreviewApp final : public CefApp, public CefBrowserProcessHandler
{
  public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnContextInitialized() override
    {
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "RUNTIME context initialized");
        auto &runtime = CefRuntime::instance();
        runtime.state = CefRuntime::Ready;
        if (runtime.stopping)
            CefQuitMessageLoop();
    }

  private:
    IMPLEMENT_REFCOUNTING(PreviewApp);
};

void CefRuntime::start()
{
    if (state != Idle)
        return;
    state = Starting;
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { instance().stop(); });
    thread_ = std::thread(
        [this]
        {
            const std::vector<std::string> arguments = {
                "huiwen-cef",
                "--use-gl=egl",
                "--enable-features=UseOzonePlatform",
                "--ozone-platform=ohos",
                "--in-process-gpu",
                "--no-zygote",
                "--no-sandbox",
                "--bundle-installation-dir=/data/storage/el1/bundle/entry/resources/resfile/",
                "--user-data-dir=/data/storage/el2/base/cache/huiwen-cef",
                "--lang=zh-CN"};
            std::vector<char *> argv;
            for (const auto &argument : arguments)
                argv.push_back(const_cast<char *>(argument.c_str()));
            CefSettings settings;
            settings.no_sandbox = true;
            CefString(&settings.cache_path) = "/data/storage/el2/base/cache/huiwen-cef";
            if (!CefInitialize(CefMainArgs(static_cast<int>(argv.size()), argv.data()), settings,
                               new PreviewApp, nullptr))
            {
                OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF", "RUNTIME initialize failed");
                state = Failed;
                return;
            }
            CefRunMessageLoop();
            state = Stopped;
            CefShutdown();
        });
}

class PreviewClient final : public CefClient,
                            public CefLifeSpanHandler,
                            public CefLoadHandler,
                            public CefRequestHandler,
                            public CefContextMenuHandler
{
  public:
    using Notice = std::function<void(bool, const QString &)>;
    using ScrollNotice = std::function<void(int, double, double)>;
    PreviewClient(QString url, QString text, Notice notice, ScrollNotice scrollNotice)
        : url_(std::move(url)), text_(std::move(text)), notice_(std::move(notice)),
          scrollNotice_(std::move(scrollNotice))
    {
    }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefContextMenuParams>,
                             CefRefPtr<CefMenuModel> model) override
    {
        model->Clear();
    }
    void create(CefWindowHandle parent, const QRect &bounds)
    {
        if (closing_ || CefRuntime::instance().stopping)
            return;
        CefWindowInfo info;
        info.SetAsChild(parent, CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
        CefBrowserSettings settings;
        settings.background_color = CefColorSetARGB(255, 245, 247, 251);
        ++CefRuntime::instance().creating;
        if (!CefBrowserHost::CreateBrowser(info, this, url_.toStdString(), settings, nullptr, nullptr))
        {
            --CefRuntime::instance().creating;
            notice_(false, QStringLiteral("无法创建 Markdown 预览窗口，请返回文件后重试"));
            CefRuntime::instance().finishClose();
        }
    }
    void geometry(const QRect &bounds, bool visible)
    {
        // Qt 和 CEF 分属不同线程；队列里只留一个任务，未执行的尺寸直接替换为最新值。
        std::lock_guard<std::mutex> lock(geometryMutex_);
        requestedBounds_ = bounds;
        requestedVisible_ = visible;
        if (geometryQueued_)
            return;
        geometryQueued_ = true;
        CefRefPtr<PreviewClient> self(this);
        postCef(
            [self]
            {
                QRect bounds;
                bool visible;
                {
                    std::lock_guard<std::mutex> lock(self->geometryMutex_);
                    bounds = self->requestedBounds_;
                    visible = self->requestedVisible_;
                    self->geometryQueued_ = false;
                }
                self->applyGeometry(bounds, visible);
            });
    }
    void close()
    {
        closing_ = true;
        if (browser_)
        {
            using namespace EMBEDDED_WINDOW_ADAPTER;
            auto *node = reinterpret_cast<Node *>(browser_->GetHost()->GetWindowHandle());
            if (node)
                EWAdapterC::hideNode(node);
            browser_->GetHost()->CloseBrowser(true);
        }
    }
    void scrollTo(int page, double fraction, double progress)
    {
        if (!browser_ || !loaded_ || closing_)
            return;
        const auto arguments =
            QJsonDocument(QJsonArray{page, fraction, progress}).toJson(QJsonDocument::Compact);
        const auto script = QStringLiteral("window.HuiwenMarkdown.setScrollPosition(...%1);")
                                .arg(QString::fromUtf8(arguments));
        browser_->GetMainFrame()->ExecuteJavaScript(script.toStdString(), url_.toStdString(), 0);
    }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                  CefProcessId source, CefRefPtr<CefProcessMessage> message) override
    {
        if (message->GetName() != "Huiwen.PreviewScroll")
            return false;
        if (closing_ || !loaded_ || source != PID_RENDERER || !frame->IsMain() ||
            frame->GetURL().ToString() != url_.toStdString())
            return true;
        const auto values = message->GetArgumentList();
        if (values->GetSize() != 3 || values->GetType(0) != VTYPE_INT ||
            values->GetType(1) != VTYPE_DOUBLE || values->GetType(2) != VTYPE_DOUBLE)
            return true;
        const int page = values->GetInt(0);
        const double fraction = values->GetDouble(1);
        const double progress = values->GetDouble(2);
        if (page >= 0 && page <= 100000 && std::isfinite(fraction) && fraction >= 0 && fraction <= 1 &&
            std::isfinite(progress) && progress >= 0 && progress <= 1)
            scrollNotice_(page, fraction, progress);
        return true;
    }
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
    {
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "PREVIEW browser created id=%{public}d",
                     browser->GetIdentifier());
        browser_ = browser;
        --CefRuntime::instance().creating;
        CefRuntime::instance().browsers.push_back(browser);
        using namespace EMBEDDED_WINDOW_ADAPTER;
        auto *node = reinterpret_cast<Node *>(browser->GetHost()->GetWindowHandle());
        if (node)
            EWAdapterC::hideNode(node);
        browser->GetHost()->SetAccessibilityState(STATE_DISABLED);
        if (closing_ || CefRuntime::instance().stopping || CefRuntime::instance().closeCompleted)
            close();
    }
    bool DoClose(CefRefPtr<CefBrowser>) override { return false; }
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
    {
        CefRuntime::instance().closed(browser);
        browser_ = nullptr;
    }
    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int) override
    {
        if (!frame->IsMain() || closing_ || frame->GetURL().ToString() != url_.toStdString())
            return;
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "PREVIEW main frame loaded");
        const QByteArray argument = QJsonDocument(QJsonArray{text_}).toJson(QJsonDocument::Compact);
        const QString script =
            QStringLiteral("try { window.HuiwenMarkdown.showPreview(%1[0]); } "
                           "catch (error) { document.getElementById('markdown-body').textContent = "
                           "'预览失败：' + String(error.message); }")
                .arg(QString::fromUtf8(argument));
        frame->ExecuteJavaScript(script.toStdString(), url_.toStdString(), 0);
        loaded_ = true;
        notice_(true, {});
    }
    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code,
                     const CefString &message, const CefString &) override
    {
        if (frame->IsMain() && code != ERR_ABORTED && !closing_)
        {
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF", "PREVIEW load failed code=%{public}d",
                         static_cast<int>(code));
            notice_(false,
                    QStringLiteral("Markdown 预览加载失败：") + QString::fromStdString(message.ToString()));
        }
    }
    void OnRenderProcessTerminated(CefRefPtr<CefBrowser>, TerminationStatus status) override
    {
        if (closing_)
            return;
        loaded_ = false;
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF", "PREVIEW renderer terminated status=%{public}d",
                     static_cast<int>(status));
        notice_(false, QStringLiteral("Markdown 渲染进程已退出（状态 %1），请返回文件后重试")
                           .arg(static_cast<int>(status)));
    }
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request, bool,
                        bool) override
    {
        // 文档中的链接不能导航到外部页面或任意本地文件。
        return request->GetURL().ToString() != url_.toStdString();
    }
    bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &, const CefString &,
                       WindowOpenDisposition, bool, const CefPopupFeatures &, CefWindowInfo &,
                       CefRefPtr<CefClient> &, CefBrowserSettings &, CefRefPtr<CefDictionaryValue> &,
                       bool *) override
    {
        return true;
    }

  private:
    void applyGeometry(const QRect &bounds, bool visible)
    {
        // 只在 CEF UI 线程访问浏览器和已应用状态；关闭后的排队任务不能再触碰原生节点。
        if (!browser_ || closing_ || CefRuntime::instance().stopping)
            return;
        const bool move = !bounds.isEmpty() && bounds.topLeft() != appliedBounds_.topLeft();
        const bool resize = !bounds.isEmpty() && bounds.size() != appliedBounds_.size();
        visible = visible && loaded_ && !bounds.isEmpty();
        if (!move && !resize && visible == nodeVisible_)
            return;
        using namespace EMBEDDED_WINDOW_ADAPTER;
        auto *node = reinterpret_cast<Node *>(browser_->GetHost()->GetWindowHandle());
        if (!node)
            return;
        if ((move && !EWAdapterC::moveNode(node, bounds.x(), bounds.y())) ||
            (resize && !EWAdapterC::resizeNode(node, bounds.width(), bounds.height())) ||
            (visible != nodeVisible_ && !EWAdapterC::setNodeVisibility(node, visible)))
        {
            qWarning() << "HUIWEN_CEF stage=geometry_failed boundsWindow=" << bounds;
            return;
        }
        if (!bounds.isEmpty())
            appliedBounds_ = bounds;
        nodeVisible_ = visible;
        // 原生窗口依赖 XComponent 的尺寸通知；WasResized/WasHidden 仅用于离屏渲染。
    }

    std::mutex geometryMutex_;
    QRect requestedBounds_;
    bool requestedVisible_ = false;
    bool geometryQueued_ = false;
    QRect appliedBounds_;
    bool nodeVisible_ = false;
    QString url_;
    QString text_;
    Notice notice_;
    ScrollNotice scrollNotice_;
    CefRefPtr<CefBrowser> browser_;
    bool closing_ = false;
    bool loaded_ = false;
    IMPLEMENT_REFCOUNTING(PreviewClient);
};
} // namespace
#endif

class MarkdownPreviewWidget::Private
{
  public:
    QLabel *status = nullptr;
    QTimer *timer = nullptr;
    QElapsedTimer waiting;
    QString text;
    QString url;
    bool valid = false;
    bool ready = false;
    int generation = 0;
#ifdef Q_OS_OPENHARMONY
    CefRefPtr<PreviewClient> client;
#endif
};

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget *parent) : QWidget(parent), d_(new Private)
{
    d_->status = new QLabel(this);
    d_->status->setAlignment(Qt::AlignCenter);
    d_->status->setWordWrap(true);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(d_->status);
    d_->timer = new QTimer(this);
    d_->timer->setInterval(100);
    connect(d_->timer, &QTimer::timeout, this, &MarkdownPreviewWidget::openBrowser);
}

MarkdownPreviewWidget::~MarkdownPreviewWidget()
{
    blockSignals(true);
    if (window() != this)
        window()->removeEventFilter(this);
    closeBrowser();
}

void MarkdownPreviewWidget::setDocument(const QString &path)
{
    closeBrowser();
    d_->valid = false;
    d_->text.clear();
    d_->url.clear();
    d_->status->show();
    d_->status->clear();
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
    {
        d_->status->setText(QStringLiteral("无法预览：文件无法读取或超过 16 MiB"));
        return;
    }
    QTextCodec::ConverterState state;
    const QByteArray bytes = file.readAll();
    d_->text = QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
    if (state.invalidChars)
    {
        d_->status->setText(QStringLiteral("无法预览：Markdown 文件不是有效的 UTF-8 文本"));
        return;
    }
    QFile page(QStringLiteral(":/markdown/preview.html"));
    if (!page.open(QIODevice::ReadOnly))
    {
        d_->status->setText(QStringLiteral("缺少预览资源，请重新执行 Run 构建"));
        return;
    }
    d_->url = QStringLiteral("data:text/html;base64,") + QString::fromLatin1(page.readAll().toBase64());
    d_->valid = true;
    d_->status->setText(QStringLiteral("正在加载 Markdown 预览…"));
    if (isVisible())
        openBrowser();
}

void MarkdownPreviewWidget::openBrowser()
{
    if (!d_->valid || !isVisible())
        return;
#ifdef Q_OS_OPENHARMONY
    if (d_->client)
    {
        if (d_->waiting.elapsed() > 15000)
        {
            closeBrowser();
            d_->status->show();
            d_->status->setText(QStringLiteral("Markdown 页面加载超时，请返回文件后重试"));
        }
        return;
    }
    auto &runtime = CefRuntime::instance();
    runtime.start();
    if (!d_->timer->isActive())
    {
        d_->waiting.start();
        d_->timer->start();
    }
    if (runtime.state == CefRuntime::Failed || runtime.state == CefRuntime::Stopped)
    {
        d_->timer->stop();
        d_->status->setText(QStringLiteral("浏览器初始化失败，请重新启动应用"));
        return;
    }
    if (d_->waiting.elapsed() > 15000)
    {
        d_->timer->stop();
        d_->status->setText(QStringLiteral("浏览器初始化超时，请返回文件后重试"));
        return;
    }
    if (runtime.state != CefRuntime::Ready)
        return;
    const int generation = d_->generation;
    QPointer<MarkdownPreviewWidget> self(this);
    d_->client = new PreviewClient(d_->url, d_->text,
                                   [self, generation](bool ready, const QString &error)
                                   {
                                       QMetaObject::invokeMethod(
                                           qApp,
                                           [self, generation, ready, error]
                                           {
                                               if (!self || self->d_->generation != generation)
                                                   return;
                                               self->d_->timer->stop();
                                               self->d_->status->setVisible(!ready);
                                               if (!ready)
                                               {
                                                   self->d_->ready = false;
                                                   emit self->previewReadyChanged(false);
                                                   self->d_->status->setText(error);
                                               }
                                               self->syncGeometry();
                                           },
                                           Qt::QueuedConnection);
                                   },
                                   [self, generation](int page, double fraction, double progress)
                                   {
                                       QMetaObject::invokeMethod(
                                           qApp,
                                           [self, generation, page, fraction, progress]
                                           {
                                               if (!self || self->d_->generation != generation ||
                                                   !self->isVisible())
                                                   return;
                                               if (!self->d_->ready)
                                               {
                                                   self->d_->ready = true;
                                                   emit self->previewReadyChanged(true);
                                                   return;
                                               }
                                               emit self->scrollPositionChanged(page, fraction, progress);
                                           },
                                           Qt::QueuedConnection);
                                   });
    const auto client = d_->client;
    const auto parent = static_cast<CefWindowHandle>(window()->winId());
    const QRect bounds = browserRect();
    postCef([client, parent, bounds] { client->create(parent, bounds); });
#else
    d_->status->setText(QStringLiteral("当前桌面构建尚未配置 CEF，Markdown 预览在鸿蒙工作区启用"));
#endif
}

void MarkdownPreviewWidget::closeWorkspaceBrowsers(std::function<void()> completed)
{
#ifdef Q_OS_OPENHARMONY
    if (CefRuntime::instance().state == CefRuntime::Ready)
    {
        postCef(
            [completed = std::move(completed)]() mutable
            {
                auto &runtime = CefRuntime::instance();
                runtime.closeCompleted = std::move(completed);
                const auto browsers = runtime.browsers;
                for (const auto &browser : browsers)
                    browser->GetHost()->CloseBrowser(true);
                runtime.finishClose();
            });
        return;
    }
#endif
    // 窗体已隐藏；引擎尚未就绪时不存在浏览器，也不会再由预览定时器创建。
    completed();
}

void MarkdownPreviewWidget::closeBrowser()
{
    if (d_->ready)
    {
        d_->ready = false;
        emit previewReadyChanged(false);
    }
    ++d_->generation;
    d_->timer->stop();
#ifdef Q_OS_OPENHARMONY
    if (d_->client)
    {
        const auto client = d_->client;
        if (CefRuntime::instance().state == CefRuntime::Ready)
            postCef([client] { client->close(); });
        d_->client = nullptr;
    }
#endif
}

bool MarkdownPreviewWidget::isPreviewReady() const { return d_->ready && isVisible(); }

void MarkdownPreviewWidget::setScrollPosition(int page, double fraction, double progress)
{
    if (!isPreviewReady() || !std::isfinite(fraction) || !std::isfinite(progress))
        return;
#ifdef Q_OS_OPENHARMONY
    const auto client = d_->client;
    if (client && CefRuntime::instance().state == CefRuntime::Ready)
        postCef([client, page, fraction, progress] { client->scrollTo(page, fraction, progress); });
#endif
}

void MarkdownPreviewWidget::syncGeometry()
{
#ifdef Q_OS_OPENHARMONY
    if (d_->client && CefRuntime::instance().state == CefRuntime::Ready)
    {
        const auto client = d_->client;
        const QRect bounds = browserRect();
        const bool visible =
            !bounds.isEmpty() && isVisible() && window()->isVisible() && !window()->isMinimized();
        client->geometry(bounds, visible);
    }
#endif
}

QRect MarkdownPreviewWidget::browserRect() const
{
    return QRect(mapTo(window(), QPoint(0, 0)), size());
}

void MarkdownPreviewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    window()->installEventFilter(this);
    openBrowser();
    syncGeometry();
}

void MarkdownPreviewWidget::hideEvent(QHideEvent *event)
{
    closeBrowser();
    QWidget::hideEvent(event);
}

void MarkdownPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncGeometry();
}

void MarkdownPreviewWidget::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    syncGeometry();
}

bool MarkdownPreviewWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window())
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
            event->type() == QEvent::WindowStateChange)
            syncGeometry();
    }
    return QWidget::eventFilter(watched, event);
}
