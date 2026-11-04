/* MD 与 DOCX 共用 CEF 生命周期、原生窗口定位和只读页面通信。 */
#include "CefDocumentPreviewWidget.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
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
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <cmath>

#ifdef Q_OS_OPENHARMONY
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_menu_model.h"
#include "include/cef_process_message.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_byte_read_handler.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "include/cef_task.h"
#include "ohos/adapter_c/adapter_c.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <hilog/log.h>
#include <mutex>
#include <thread>
#include <unordered_map>
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
                                      [browser](const auto &item)
                                      { return item->IsSame(browser); }),
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
            postCef([completed]
                    { QMetaObject::invokeMethod(qApp, completed, Qt::QueuedConnection); });
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
                            public CefDisplayHandler,
                            public CefRequestHandler,
                            public CefResourceRequestHandler,
                            public CefContextMenuHandler
{
  public:
    using Notice = std::function<void(bool, const QString &)>;
    using ScrollNotice = std::function<void(int, double, double)>;
    PreviewClient(QString url, QByteArray html, QString text, QString imageRoot,
                  bool scrollBarVisible, Notice notice, ScrollNotice scrollNotice)
        : url_(std::move(url)), html_(std::move(html)), text_(std::move(text)),
          imageRoot_(std::move(imageRoot)), notice_(std::move(notice)),
          scrollNotice_(std::move(scrollNotice)), scrollBarVisible_(scrollBarVisible)
    {
    }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request, bool, bool,
        const CefString &, bool &disableDefaultHandling) override
    {
        const QUrl url(QString::fromStdString(request->GetURL().ToString()));
        if (url.scheme() != QStringLiteral("https") ||
            url.host() != QStringLiteral("huiwen-md.invalid"))
            return nullptr;
        // A browser-local resource address, never a network request (including
        // failures).
        disableDefaultHandling = true;
        return this;
    }
    CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser>,
                                                     CefRefPtr<CefFrame> frame,
                                                     CefRefPtr<CefRequest> request) override
    {
        const auto missing = []() -> CefRefPtr<CefResourceHandler> {
            return new CefStreamResourceHandler(404, "Not Found", "text/plain", {}, nullptr);
        };
        if (!frame || !frame->IsMain() || request->GetMethod() != "GET")
            return missing();
        if (!html_.isEmpty() && request->GetResourceType() == RT_MAIN_FRAME &&
            request->GetURL().ToString() == url_.toStdString())
        {
            // Hold the client while CEF reads its immutable page bytes asynchronously.
            auto stream = CefStreamReader::CreateForHandler(new CefByteReadHandler(
                reinterpret_cast<const unsigned char *>(html_.constData()), html_.size(), this));
            OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF",
                         "PREVIEW local page bytes=%{public}d", html_.size());
            return new CefStreamResourceHandler(
                200, "OK", "text/html",
                {{"Cache-Control", "no-store"}, {"X-Content-Type-Options", "nosniff"}}, stream);
        }
        if (imageRoot_.isEmpty() || request->GetResourceType() != RT_IMAGE)
            return missing();
        const QUrl url(QString::fromStdString(request->GetURL().ToString()));
        if (url.path() != QStringLiteral("/image") || url.hasFragment())
            return missing();
        const QString relative =
            QUrlQuery(url).queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded);
        if (relative.isEmpty() || relative.size() > 4096 || !QDir::isRelativePath(relative) ||
            relative.contains(QChar(0)) || relative.contains(QLatin1Char('\\')) ||
            relative.contains(QLatin1Char(':')))
            return missing();
        const QFileInfo file(QDir(imageRoot_).filePath(relative));
        const QString canonical = file.canonicalFilePath();
        if (!canonical.startsWith(imageRoot_ + QLatin1Char('/')) || !file.isFile() ||
            file.size() <= 0 || file.size() > 32 * 1024 * 1024)
            return missing();
        const QString extension = QFileInfo(canonical).suffix().toLower();
        QString mime;
        if (extension == QStringLiteral("png"))
            mime = QStringLiteral("image/png");
        else if (extension == QStringLiteral("jpg") || extension == QStringLiteral("jpeg"))
            mime = QStringLiteral("image/jpeg");
        else if (extension == QStringLiteral("webp"))
            mime = QStringLiteral("image/webp");
        else if (extension == QStringLiteral("gif"))
            mime = QStringLiteral("image/gif");
        else if (extension == QStringLiteral("bmp"))
            mime = QStringLiteral("image/bmp");
        else
            return missing();
        auto stream = CefStreamReader::CreateForFile(canonical.toStdString());
        if (!stream)
            return missing();
        return new CefStreamResourceHandler(
            200, "OK", mime.toStdString(),
            {{"Cache-Control", "no-store"}, {"X-Content-Type-Options", "nosniff"}}, stream);
    }
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                             CefRefPtr<CefContextMenuParams>,
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
        if (!CefBrowserHost::CreateBrowser(info, this, url_.toStdString(), settings, nullptr,
                                           nullptr))
        {
            --CefRuntime::instance().creating;
            notice_(false, QStringLiteral("无法创建文档预览窗口，请返回文件后重试"));
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
        const auto script = QStringLiteral("window.HuiwenPreview.setScrollPosition(...%1);")
                                .arg(QString::fromUtf8(arguments));
        browser_->GetMainFrame()->ExecuteJavaScript(script.toStdString(), url_.toStdString(), 0);
    }
    void publishScrollPosition()
    {
        if (!browser_ || !loaded_ || closing_)
            return;
        browser_->GetMainFrame()->ExecuteJavaScript(
            "(() => { const position = window.HuiwenPreview.scrollPosition();"
            "window.huiwenReportScroll(position.page, position.fraction, position.progress); })();",
            url_.toStdString(), 0);
    }
    void setScrollBarVisible(bool visible)
    {
        scrollBarVisible_ = visible;
        if (!browser_ || !loaded_ || closing_)
            return;
        // 隐藏浏览器滚动条，但仍由 CEF 处理滚动，不能在外面再叠一层 Qt 滚动区域。
        const auto script =
            QStringLiteral(
                "(() => { let style = document.getElementById('huiwen-scrollbar');"
                "if (!style) { style = document.createElement('style');"
                "style.id = 'huiwen-scrollbar'; document.head.appendChild(style); }"
                "style.textContent = %1 ? '' : "
                "'::-webkit-scrollbar { width: 0 !important; height: 0 !important; }'; })();")
                .arg(visible ? QStringLiteral("true") : QStringLiteral("false"));
        browser_->GetMainFrame()->ExecuteJavaScript(script.toStdString(), url_.toStdString(), 0);
    }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                  CefProcessId source,
                                  CefRefPtr<CefProcessMessage> message) override
    {
        if (message->GetName() != "Huiwen.PreviewScroll" &&
            message->GetName() != "Huiwen.PreviewState")
            return false;
        if (closing_ || !frameLoaded_ || source != PID_RENDERER || !frame->IsMain() ||
            frame->GetURL().ToString() != url_.toStdString())
            return true;
        const auto values = message->GetArgumentList();
        if (message->GetName() == "Huiwen.PreviewState")
        {
            if (values->GetSize() != 2 || values->GetType(0) != VTYPE_BOOL ||
                values->GetType(1) != VTYPE_STRING)
                return true;
            const auto error = values->GetString(1).ToString();
            if (error.size() > 4096)
                return true;
            const bool ready = values->GetBool(0);
            OH_LOG_Print(LOG_APP, ready ? LOG_INFO : LOG_ERROR, 0xFF00, "HuiwenCEF",
                         "PREVIEW state ready=%{public}d error=%{public}s", ready,
                         error.substr(0, 512).c_str());
            loaded_ = ready;
            if (ready)
                setScrollBarVisible(scrollBarVisible_);
            notice_(ready, QString::fromStdString(error));
            return true;
        }
        if (!loaded_)
            return true;
        if (values->GetSize() != 3 || values->GetType(0) != VTYPE_INT ||
            values->GetType(1) != VTYPE_DOUBLE || values->GetType(2) != VTYPE_DOUBLE)
            return true;
        const int page = values->GetInt(0);
        const double fraction = values->GetDouble(1);
        const double progress = values->GetDouble(2);
        if (page >= 0 && page <= 100000 && std::isfinite(fraction) && fraction >= 0 &&
            fraction <= 1 && std::isfinite(progress) && progress >= 0 && progress <= 1)
            scrollNotice_(page, fraction, progress);
        return true;
    }
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
    {
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF",
                     "PREVIEW browser created id=%{public}d", browser->GetIdentifier());
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
        if (!frame->IsMain() || closing_)
            return;
        if (frame->GetURL().ToString() != url_.toStdString())
        {
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF",
                         "PREVIEW unexpected main frame URL length=%{public}zu expected=%{public}d",
                         frame->GetURL().ToString().size(), url_.size());
            return;
        }
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "PREVIEW main frame loaded");
        const QByteArray argument = QJsonDocument(QJsonArray{text_}).toJson(QJsonDocument::Compact);
        const QString script =
            QStringLiteral("try { window.HuiwenPreview.showPreview(%1[0]); } "
                           "catch (error) { console.error('PREVIEW render failed', error); "
                           "window.huiwenReportPreview(false, "
                           "'预览失败：' + String(error.message || error).slice(0, 512)); }")
                .arg(QString::fromUtf8(argument));
        frameLoaded_ = true;
        frame->ExecuteJavaScript(script.toStdString(), url_.toStdString(), 0);
    }
    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code,
                     const CefString &message, const CefString &) override
    {
        if (frame->IsMain() && code != ERR_ABORTED && !closing_)
        {
            frameLoaded_ = false;
            loaded_ = false;
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF",
                         "PREVIEW load failed code=%{public}d message=%{public}s",
                         static_cast<int>(code), message.ToString().substr(0, 512).c_str());
            notice_(false, QStringLiteral("文档预览加载失败：") +
                               QString::fromStdString(message.ToString()));
        }
    }
    bool OnConsoleMessage(CefRefPtr<CefBrowser>, cef_log_severity_t level,
                          const CefString &message, const CefString &, int line) override
    {
        if (!closing_ && level >= LOGSEVERITY_WARNING && level < LOGSEVERITY_DISABLE)
            OH_LOG_Print(LOG_APP, level >= LOGSEVERITY_ERROR ? LOG_ERROR : LOG_WARN,
                         0xFF00, "HuiwenCEF", "PREVIEW JS line=%{public}d message=%{public}s",
                         line, message.ToString().substr(0, 1024).c_str());
        return false;
    }
    void OnRenderProcessTerminated(CefRefPtr<CefBrowser>, TerminationStatus status) override
    {
        if (closing_)
            return;
        loaded_ = false;
        frameLoaded_ = false;
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "HuiwenCEF",
                     "PREVIEW renderer terminated status=%{public}d", static_cast<int>(status));
        notice_(false, QStringLiteral("文档渲染进程已退出（状态 %1），请返回文件后重试")
                           .arg(static_cast<int>(status)));
    }
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request,
                        bool, bool) override
    {
        // 文档中的链接不能导航到外部页面或任意本地文件。
        return request->GetURL().ToString() != url_.toStdString();
    }
    bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &,
                       const CefString &, WindowOpenDisposition, bool, const CefPopupFeatures &,
                       CefWindowInfo &, CefRefPtr<CefClient> &, CefBrowserSettings &,
                       CefRefPtr<CefDictionaryValue> &, bool *) override
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
    const QByteArray html_;
    QString text_;
    const QString imageRoot_;
    Notice notice_;
    ScrollNotice scrollNotice_;
    CefRefPtr<CefBrowser> browser_;
    bool closing_ = false;
    bool loaded_ = false;
    bool frameLoaded_ = false;
    bool scrollBarVisible_;
    IMPLEMENT_REFCOUNTING(PreviewClient);
};
} // namespace
#endif

class CefDocumentPreviewWidget::Private
{
  public:
    QLabel *status = nullptr;
    QTimer *timer = nullptr;
    QElapsedTimer waiting;
    Format format;
    QString label;
    QString text;
    QString url;
    QByteArray html;
    QString path;
    bool valid = false;
    bool ready = false;
    int generation = 0;
    bool scrollBarVisible = true;
#ifdef Q_OS_OPENHARMONY
    CefRefPtr<PreviewClient> client;
#endif
};

CefDocumentPreviewWidget::CefDocumentPreviewWidget(Format format, QWidget *parent)
    : QWidget(parent), d_(new Private)
{
    d_->format = format;
    d_->label = format == Format::Word ? QStringLiteral("Word") : QStringLiteral("Markdown");
    d_->status = new QLabel(this);
    d_->status->setAlignment(Qt::AlignCenter);
    d_->status->setWordWrap(true);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(d_->status);
    d_->timer = new QTimer(this);
    d_->timer->setInterval(100);
    connect(d_->timer, &QTimer::timeout, this, &CefDocumentPreviewWidget::openBrowser);
}

CefDocumentPreviewWidget::~CefDocumentPreviewWidget()
{
    blockSignals(true);
    if (window() != this)
        window()->removeEventFilter(this);
    closeBrowser();
}

void CefDocumentPreviewWidget::setDocument(const QString &path)
{
    closeBrowser();
    d_->path = path;
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
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        d_->status->setText(QStringLiteral("无法预览：读取文档失败"));
        return;
    }
    if (d_->format == Format::Word)
    {
        if (QFileInfo(path).suffix().compare(QStringLiteral("docx"), Qt::CaseInsensitive) != 0 ||
            bytes.isEmpty())
        {
            d_->status->setText(QStringLiteral("Word 预览仅支持非空 DOCX 文件"));
            return;
        }
        d_->text = QString::fromLatin1(bytes.toBase64());
    }
    else
    {
        QTextCodec::ConverterState state;
        d_->text =
            QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars)
        {
            d_->status->setText(QStringLiteral("无法预览：Markdown 文件不是有效的 UTF-8 文本"));
            return;
        }
    }
    QFile page(d_->format == Format::Word ? QStringLiteral(":/word/preview.html")
                                          : QStringLiteral(":/markdown/preview.html"));
    if (!page.open(QIODevice::ReadOnly))
    {
        d_->status->setText(QStringLiteral("缺少预览资源，请重新执行 Run 构建"));
        return;
    }
    d_->html.clear();
    if (d_->format == Format::Markdown)
    {
        // Rich preview assets exceed Chromium's URL limit; serve bytes through CEF instead.
        d_->html = page.readAll();
        d_->url = QStringLiteral("https://huiwen-md.invalid/preview.html");
    }
    else
        d_->url = QStringLiteral("data:text/html;base64,") +
                  QString::fromLatin1(page.readAll().toBase64());
    d_->valid = true;
    d_->status->setText(QStringLiteral("正在加载 %1 预览…").arg(d_->label));
    if (isVisible())
        openBrowser();
}

void CefDocumentPreviewWidget::openBrowser()
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
            d_->status->setText(QStringLiteral("%1 页面加载超时，请返回文件后重试").arg(d_->label));
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
    QPointer<CefDocumentPreviewWidget> self(this);
    d_->client = new PreviewClient(
        d_->url, d_->html, d_->text,
        d_->format == Format::Markdown ? QFileInfo(d_->path).canonicalPath() : QString(),
        d_->scrollBarVisible,
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
                    if (!self || self->d_->generation != generation || !self->isVisible())
                        return;
                    if (!self->d_->ready)
                    {
                        self->d_->ready = true;
                        emit self->documentOpened(self->d_->path);
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
    d_->status->setText(
        QStringLiteral("当前桌面构建尚未配置 CEF，%1 预览在鸿蒙工作区启用").arg(d_->label));
#endif
}

void CefDocumentPreviewWidget::closeWorkspaceBrowsers(std::function<void()> completed)
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

void CefDocumentPreviewWidget::closeBrowser()
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

bool CefDocumentPreviewWidget::isPreviewReady() const { return d_->ready && isVisible(); }

void CefDocumentPreviewWidget::setScrollPosition(int page, double fraction, double progress)
{
    if (!isPreviewReady() || !std::isfinite(fraction) || !std::isfinite(progress))
        return;
#ifdef Q_OS_OPENHARMONY
    const auto client = d_->client;
    if (client && CefRuntime::instance().state == CefRuntime::Ready)
        postCef([client, page, fraction, progress] { client->scrollTo(page, fraction, progress); });
#endif
}

void CefDocumentPreviewWidget::publishScrollPosition()
{
#ifdef Q_OS_OPENHARMONY
    const auto client = d_->client;
    if (isPreviewReady() && client && CefRuntime::instance().state == CefRuntime::Ready)
        postCef([client] { client->publishScrollPosition(); });
#endif
}

void CefDocumentPreviewWidget::setViewOptions(const DocumentViewOptions &options)
{
    // CEF 保留自身惯性；此处只映射通用滚动条选项，不接入 Qt 的惯性控制器。
    d_->scrollBarVisible = options.scrollBarVisible;
#ifdef Q_OS_OPENHARMONY
    const auto client = d_->client;
    if (client && CefRuntime::instance().state == CefRuntime::Ready)
        postCef([client, visible = options.scrollBarVisible]
                { client->setScrollBarVisible(visible); });
#endif
}

void CefDocumentPreviewWidget::syncGeometry()
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

QRect CefDocumentPreviewWidget::browserRect() const
{
    return QRect(mapTo(window(), QPoint(0, 0)), size());
}

void CefDocumentPreviewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    window()->installEventFilter(this);
    openBrowser();
    syncGeometry();
}

void CefDocumentPreviewWidget::hideEvent(QHideEvent *event)
{
    closeBrowser();
    QWidget::hideEvent(event);
}

void CefDocumentPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncGeometry();
}

void CefDocumentPreviewWidget::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    syncGeometry();
}

bool CefDocumentPreviewWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window())
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
            event->type() == QEvent::WindowStateChange)
            syncGeometry();
    }
    return QWidget::eventFilter(watched, event);
}
