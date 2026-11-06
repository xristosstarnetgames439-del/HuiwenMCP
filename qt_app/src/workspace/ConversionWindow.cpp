/* 预览和转化共用工作区；模式只改变内容栏数、文档类型和菜单，窗口交接保持一致。 */
#include "ConversionWindow.h"
#include "components/ConvertButton.h"
#include "components/functionBar/FileMenu.h"
#include "components/functionBar/FunctionBar.h"
#include "components/splitter/WorkspaceSplitter.h"
#include "documentView/DocumentView.h"
#include "documentView/renderers/markdown/MarkdownPreviewWidget.h"
#include "documentView/renderers/pdf/PdfPreviewWidget.h"
#include "documentView/renderers/word/WordPreviewWidget.h"
#include "sandbox/SandboxService.h"
#include "services/ConversionService.h"
#include "sidebars/agent/AgentSidebar.h"
#include "sidebars/navigation/NavigationSidebar.h"
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#endif
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSplitterHandle>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

ConversionWindow::ConversionWindow(const QString &mode, const QString &kind, QWidget *parent)
    : QWidget(parent), mode_(mode), kind_(kind), splitter_(new WorkspaceSplitter(this)),
      bodySplitter_(new WorkspaceSplitter(this)), agentSidebar_(new AgentSidebar(this)),
      navigationSidebar_(new NavigationSidebar(this)),
      source_(kind == "word" ? new DocumentView(new WordPreviewWidget,
                                                QStringLiteral("请选择 Word（DOC / DOCX）"), true)
              : kind == "md" && mode == "edit"
                  ? new DocumentView(editor_ = new CefDocumentPreviewWidget(
                                         CefDocumentPreviewWidget::Format::MarkdownEditor),
                                     QStringLiteral("请选择 Markdown"), true)
              : kind == "md"
                  ? new DocumentView(markdownPreview_ = new MarkdownPreviewWidget,
                                     QStringLiteral("请选择 Markdown"), true)
                  : new DocumentView(new PdfPreviewWidget, QStringLiteral("请选择 PDF"), true)),
      functionBar_(new FunctionBar(mode, this)), sandbox_(new SandboxService(this)),
      status_(new QLabel(this))
{
    const bool conversion = mode_ == "convert";
    if (editor_)
        result_ = new DocumentView(livePreview_ = new CefDocumentPreviewWidget(
                                       CefDocumentPreviewWidget::Format::MarkdownLive),
                                   QStringLiteral("编辑后实时预览"), false);
    // 单栏只创建实际需要的一个渲染器；保留现有工作区及分栏容器，不另建预览窗口。
    if (conversion)
    {
        result_ = new DocumentView(markdownPreview_ = new MarkdownPreviewWidget,
                                   QStringLiteral("待转化 PDF"), false);
        button_ = new ConvertButton(this);
        conversion_ = new ConversionService(this);
    }
    else if (kind_ == "word")
        conversion_ = new ConversionService(this);
    setWindowTitle(editor_ ? QStringLiteral("汇文MCP · Markdown 编辑")
                   : conversion ? QStringLiteral("汇文MCP · 转换工作区")
                              : QStringLiteral("汇文MCP · %1 预览")
                                    .arg(kind_ == "word" ? "Word"
                                         : kind_ == "md" ? "Markdown"
                                                         : "PDF"));
    setMinimumSize(640, 480);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#F5F7FB"));
    colors.setColor(QPalette::Base, colors.color(QPalette::Window));
    setPalette(colors);
    setObjectName(QStringLiteral("conversionWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("QWidget#conversionWindow, "
                                 "QWidget#documentCardBackground, QWidget#documentCardViewport "
                                 "{ background-color: %1; }")
                      .arg(colors.color(QPalette::Window).name()));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(functionBar_);
    splitter_->addWidget(source_);
    if (result_)
    {
        splitter_->addWidget(result_);
        splitter_->setSizes({600, 600});
    }
    splitter_->installEventFilter(this);
    // 只有一个子控件时 QSplitter 不显示分隔条，文档占满原有内容区。
    bodySplitter_->addWidget(navigationSidebar_);
    bodySplitter_->addWidget(splitter_);
    bodySplitter_->addWidget(agentSidebar_);
    navigationSidebar_->hide();
    bodySplitter_->setStretchFactor(0, 0);
    bodySplitter_->setStretchFactor(1, 1);
    bodySplitter_->setStretchFactor(2, 0);
    bodySplitter_->setSizes({0, 820, agentSidebarWidth_});
    layout->addWidget(bodySplitter_, 1);
    connect(functionBar_, &FunctionBar::navigationSidebarToggled, this, [this](bool visible) {
        toggleSidebar(navigationSidebar_, 0, navigationSidebarWidth_, visible);
    });
    connect(functionBar_, &FunctionBar::agentSidebarToggled, this,
            [this](bool visible) { toggleSidebar(agentSidebar_, 2, agentSidebarWidth_, visible); });
    connect(bodySplitter_, &QSplitter::splitterMoved, this, [this] {
        bodySplitter_->raiseAbovePreview();
        agentSidebar_->raiseAbovePreview();
        navigationSidebar_->raiseAbovePreview();
        placeConvertButton();
    });
    status_->setContentsMargins(16, 4, 16, 4);
    status_->setMinimumHeight(28);
    status_->setWordWrap(true);
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(status_, 0);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { placeConvertButton(); });
    const auto refreshPreview = [this]
    {
        splitter_->raiseAbovePreview();
        bodySplitter_->raiseAbovePreview();
        agentSidebar_->raiseAbovePreview();
        navigationSidebar_->raiseAbovePreview();
        refreshScrollSync();
        placeConvertButton();
        updateEditState();
    };
    connect(source_, &DocumentView::previewReadyChanged, this, refreshPreview);
    connect(source_, &DocumentView::cardActivated, this, &ConversionWindow::selectDocument);
    connect(source_, &DocumentView::documentOpened, this,
            [this] { sandbox_->markUsed(document_); });
    connect(functionBar_, &FunctionBar::scrollSyncChanged, this,
            &ConversionWindow::refreshScrollSync);
    connect(functionBar_->fileMenu(), &FileMenu::replacementRequested, this,
            &ConversionWindow::selectDocument);
    connect(functionBar_->fileMenu(), &FileMenu::previewModeRequested, this,
            [this](bool preview)
            {
                source_->setPreviewMode(preview);
                if (result_)
                    result_->setPreviewMode(preview);
                refreshScrollSync();
                placeConvertButton();
            });
    connect(functionBar_->fileMenu(), &FileMenu::directoryRequested, this,
            [this](bool output) { sandbox_->openTaskDirectory(document_, output); });
    connect(sandbox_, &SandboxService::documentSelected, this, &ConversionWindow::showDocument);
    connect(sandbox_, &SandboxService::restorationRequested, this, [this](const DocumentItem &document) {
        checkUnsaved([this, document](bool ok) {
            if (ok) restoreDocument(document);
            else sandbox_->finishRestoration(document, QStringLiteral("未替换当前编辑文档"));
        });
    });
    connect(sandbox_, &SandboxService::selectionFinished, this, [this] {
        picking_ = false;
        if (status_->text() == QStringLiteral("正在选择文件…"))
            status_->clear();
        refreshBusyState();
    });
    connect(sandbox_, &SandboxService::failed, status_, &QLabel::setText);
    if (markdownPreview_)
        connect(markdownPreview_, &CefDocumentPreviewWidget::markdownMessage, this,
                &ConversionWindow::handleOutline);
    connect(navigationSidebar_, &NavigationSidebar::headingActivated, this, [this](int line) {
        const QJsonObject message{
            {"type", "navigate"}, {"line", line}, {"revision", editRevision_}};
        if (editor_)
        {
            editor_->sendMarkdownMessage(message);
            livePreview_->sendMarkdownMessage(message);
        }
        else if (markdownPreview_)
            markdownPreview_->sendMarkdownMessage(message);
    });
    if (result_ && !editor_)
    {
        connect(result_, &DocumentView::previewReadyChanged, this, refreshPreview);
        connect(source_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        result_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        source_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::cardActivated, sandbox_,
                &SandboxService::openResultLocation);
        // 按钮独立接入转换服务，区域之间不互相调用。
        connect(button_, &QPushButton::clicked, this,
                [this]
                {
                    converting_ = true;
                    refreshBusyState();
                    result_->setDocument({}, QStringLiteral("md"));
                    navigationSidebar_->setOutline({}, false);
                    status_->setText(QStringLiteral("正在转化 PDF…"));
                    sandbox_->markUsed(document_);
                    conversion_->convertPdfToMd(document_.inputPath, document_.outputPath);
                });
        connect(conversion_, &ConversionService::succeeded, this,
                [this](const QString &path, const QString &message)
                {
                    converting_ = false;
                    retry_ = false;
                    sandbox_->markUsed(document_);
                    result_->setDocument(path, QStringLiteral("md"));
                    functionBar_->fileMenu()->setPreviewMode(true);
                    status_->setText(QStringLiteral("[code=0] %1 路径：%2").arg(message, path));
                    refreshBusyState();
                });
        connect(conversion_, &ConversionService::failed, this,
                [this](int code, const QString &message) {
                    converting_ = false;
                    retry_ = true;
                    sandbox_->markUsed(document_);
                    status_->setText(QStringLiteral("[code=%1] %2").arg(code).arg(message));
                    refreshBusyState();
                });
    }
    if (editor_)
    {
#ifdef Q_OS_OPENHARMONY
        connect(&QtBridge::instance(), &QtBridge::replyReceived, this, [this](const QString &json) {
            if (QJsonDocument::fromJson(json.toUtf8()).object().value("action").toString() == "closeAborted")
            {
                closeApproved_ = false;
                editor_->sendMarkdownMessage({{"type", "unlock"}});
            }
        });
#endif
        saveAction_ = functionBar_->addCommand(QStringLiteral("保存"));
        connect(saveAction_, &QAction::triggered, this, [this] {
            requestEditSnapshot([this](bool ok) { if (ok) saveEdit(); });
        });
        connect(editor_, &CefDocumentPreviewWidget::markdownMessage, this,
                [this](const QJsonObject &message) { handleMarkdown(message, true); });
        connect(livePreview_, &CefDocumentPreviewWidget::markdownMessage, this,
                [this](const QJsonObject &message) { handleMarkdown(message, false); });
        connect(result_, &DocumentView::previewReadyChanged, this, refreshPreview);
        connect(livePreview_, &CefDocumentPreviewWidget::documentOpened, this, [this] {
            livePreview_->sendMarkdownMessage({{"type", "content"}, {"text", editText_},
                                              {"revision", editRevision_}});
        });
    }
    if (!result_ && kind_ == "word")
    {
        connect(conversion_, &ConversionService::succeeded, this,
                [this](const QString &path, const QString &message) {
                    converting_ = false;
                    source_->setDocument(path, document_.kind, document_.name);
                    status_->setText(message);
                    refreshBusyState();
                    if (restoringWord_)
                    {
                        restoringWord_ = false;
                        sandbox_->finishRestoration(document_);
                    }
                });
        connect(conversion_, &ConversionService::failed, this,
                [this](int code, const QString &message) {
                    converting_ = false;
                    source_->setDocument({}, QStringLiteral("word"));
                    conversion_->releaseDocPreview();
                    const QString error = QStringLiteral("[code=%1] %2").arg(code).arg(message);
                    status_->setText(error);
                    refreshBusyState();
                    if (restoringWord_)
                    {
                        restoringWord_ = false;
                        sandbox_->finishRestoration(document_, error);
                    }
                });
    }
    functionBar_->fileMenu()->setPreviewMode(true);
    refreshBusyState();
}

// 待迁移到窗口基类：QPA 已将系统关闭转为 closeEvent，这里只通过 NAPI 发出返回首页请求。
void ConversionWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OPENHARMONY
    event->ignore();
    QtBridge::instance().request("close");
#else
    if (closeApproved_ || !editor_)
        QWidget::closeEvent(event);
    else
    {
        event->ignore();
        requestClosePermission([this](bool ok) { if (ok) close(); });
    }
#endif
}

void ConversionWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeConvertButton();
}

void ConversionWindow::toggleSidebar(QWidget *sidebar, int index, int &width, bool visible)
{
    auto sizes = bodySplitter_->sizes();
    if (!visible && sizes[index] > 0)
        width = sizes[index];
    sidebar->setVisible(visible);
    sizes[index] = visible ? std::max(sidebar->minimumWidth(), width) : 0;
    const int handles =
        (navigationSidebar_->isHidden() ? 0 : 1) + (agentSidebar_->isHidden() ? 0 : 1);
    sizes[1] = std::max(260, bodySplitter_->width() - sizes[0] - sizes[2] -
                                 handles * bodySplitter_->handleWidth());
    bodySplitter_->setSizes(sizes);
    if (visible)
        sidebar->setFocus(Qt::OtherFocusReason);
    agentSidebar_->raiseAbovePreview();
    navigationSidebar_->raiseAbovePreview();
    bodySplitter_->raiseAbovePreview();
    placeConvertButton();
}

bool ConversionWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == splitter_ && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this] { placeConvertButton(); });
    return QWidget::eventFilter(watched, event);
}

void ConversionWindow::placeConvertButton()
{
    if (!button_ || button_->isHidden())
        return;
    const QPoint center = splitter_->handle(1)->mapTo(this, splitter_->handle(1)->rect().center());
    button_->move(center.x() - button_->width() / 2,
                  splitter_->mapTo(this, QPoint(0, splitter_->height())).y() - button_->height() -
                      72);
    button_->raiseAbovePreview();
}

void ConversionWindow::refreshBusyState()
{
    source_->setBusy(picking_ || converting_);
    functionBar_->fileMenu()->setDocumentState(!document_.inputPath.isEmpty(),
                                               picking_ || converting_);
    if (button_)
    {
        button_->setState(!document_.inputPath.isEmpty(), picking_ || converting_, retry_);
        // 单栏不创建按钮；双栏仅用结果文件控制显隐，取消选取不改变结果。
        button_->setVisible(!result_->hasDocument());
    }
    placeConvertButton();
    updateEditState();
}

void ConversionWindow::selectDocument()
{
    if (picking_ || converting_)
        return;
    if (editor_)
    {
        checkUnsaved([this](bool ok) {
            if (!ok) return;
            picking_ = true;
            status_->setText(QStringLiteral("正在选择文件…"));
            refreshBusyState();
            sandbox_->selectDocument(kind_, mode_);
        });
        return;
    }
    picking_ = true;
    status_->setText(QStringLiteral("正在选择文件…"));
    refreshBusyState();
    sandbox_->selectDocument(kind_, mode_);
}

void ConversionWindow::showDocument(const DocumentItem &document)
{
    document_ = document;
    agentSidebar_->setDocument(document);
    navigationSidebar_->setDocument(document);
    retry_ = false;
    status_->clear();
    if (document.kind == "word" &&
        QFileInfo(document.inputPath).suffix().compare("doc", Qt::CaseInsensitive) == 0)
    {
        converting_ = true;
        const QString message = QStringLiteral("正在转换 DOC，请稍候…");
        source_->showLoading(message);
        status_->setText(message);
        refreshBusyState();
        conversion_->convertDocToDocx(document.inputPath);
        return;
    }
    const QString workingPath = editor_ && !document.openPath.isEmpty()
                                    ? document.openPath : document.inputPath;
    source_->setDocument(workingPath, document.kind, document.name);
    if (document.kind == "word")
        conversion_->releaseDocPreview();
    if (editor_)
    {
        QFile input(workingPath);
        if (!input.open(QIODevice::ReadOnly) || input.size() > 16 * 1024 * 1024)
        {
            status_->setText(QStringLiteral("无法读取 Markdown，或文件超过 16 MiB"));
            return;
        }
        editText_ = savedText_ = QString::fromUtf8(input.readAll())
                                    .replace("\r\n", "\n").replace('\r', '\n');
        editRevision_ = 0;
        editDirty_ = false;
        document_.openPath = workingPath;
        result_->setDocument(workingPath, QStringLiteral("md"), document.name);
        updateEditState();
    }
    else if (result_)
    {
        const QFileInfo output(document.outputPath);
        result_->setDocument(output.isFile() && output.size() > 0 ? document.outputPath : QString(),
                             QStringLiteral("md"));
    }
}

void ConversionWindow::restoreDocument(const DocumentItem &document)
{
    if (picking_ || converting_)
    {
        sandbox_->finishRestoration(document,
                                    QStringLiteral("工作区正在选择或转换文件，请稍后重试"));
        return;
    }
    if (document.mode != mode_ || document.kind != kind_)
    {
        sandbox_->finishRestoration(document, QStringLiteral("任务类型与当前工作区不一致"));
        return;
    }
    // 编辑任务复核当前版本，避免已有 output 失效时悄悄打开 input 原件。
    QFile input(editor_ && !document.openPath.isEmpty() ? document.openPath : document.inputPath);
    if (!input.open(QIODevice::ReadOnly) || (!editor_ && input.read(1).isEmpty()))
    {
        sandbox_->finishRestoration(document, QStringLiteral("原任务的文档已无法读取"));
        return;
    }
    const bool deferred =
        document.kind == "word" &&
        QFileInfo(document.inputPath).suffix().compare("doc", Qt::CaseInsensitive) == 0;
    restoringWord_ = deferred;
    showDocument(document);
    functionBar_->fileMenu()->setPreviewMode(true);
    refreshBusyState();
    if (!deferred)
        sandbox_->finishRestoration(document);
}

void ConversionWindow::refreshScrollSync()
{
    if (scrollSyncEnabled())
    {
        if (editor_) editor_->sendMarkdownMessage({{"type", "publish"}});
        else source_->publishScrollPosition();
    }
}

void ConversionWindow::updateEditState()
{
    if (!editor_) return;
    setWindowTitle(QStringLiteral("汇文MCP · Markdown 编辑%1").arg(editDirty_ ? " *" : ""));
    if (saveAction_)
        saveAction_->setEnabled(!document_.inputPath.isEmpty() && editor_->isPreviewReady() &&
                                !picking_ && !pendingSnapshot_);
}

bool ConversionWindow::saveEdit()
{
    const QByteArray bytes = editText_.toUtf8();
    if (document_.inputPath.isEmpty() || bytes.size() > 16 * 1024 * 1024)
    {
        status_->setText(QStringLiteral("无法保存：文档未打开或内容超过 16 MiB"));
        return false;
    }
    const QFileInfo output(document_.outputPath);
    const QString expected = QDir(document_.taskPath).filePath(
        QStringLiteral("output/") + QFileInfo(document_.inputPath).fileName());
    if (document_.outputPath != expected || output.isSymLink() ||
        QFileInfo(output.absolutePath()).canonicalFilePath() != output.absolutePath() ||
        (output.exists() && (!output.isFile() || !output.isReadable() ||
                             output.canonicalFilePath() != document_.outputPath)))
    {
        status_->setText(QStringLiteral("无法保存：编辑输出路径已失效"));
        return false;
    }
    if (!editDirty_ && document_.openPath != document_.outputPath)
    {
        status_->setText(QStringLiteral("内容未修改，input 原件保持不变"));
        return true;
    }
    QSaveFile file(document_.outputPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        status_->setText(QStringLiteral("保存失败：") + file.errorString());
        return false;
    }
    savedText_ = editText_;
    editDirty_ = false;
    document_.openPath = document_.outputPath;
    // Rebind resource paths without rebuilding the editors or losing their undo history.
    const QJsonObject location{{"type", "location"}, {"path", document_.openPath}};
    editor_->sendMarkdownMessage(location);
    livePreview_->sendMarkdownMessage(location);
    sandbox_->markUsed(document_);
    status_->setText(QStringLiteral("已保存到 output，后续继续编辑此版本"));
    updateEditState();
    return true;
}

void ConversionWindow::requestEditSnapshot(std::function<void(bool)> completed)
{
    if (!editor_ || document_.inputPath.isEmpty()) { completed(true); return; }
    if (pendingSnapshot_ || !editor_->isPreviewReady())
    {
        status_->setText(QStringLiteral("编辑器尚未就绪，无法获取最新内容"));
        completed(false);
        return;
    }
    pendingSnapshot_ = std::move(completed);
    const int request = ++snapshotRequest_;
    editor_->sendMarkdownMessage({{"type", "snapshot"}, {"request", request}});
    updateEditState();
    QTimer::singleShot(5000, this, [this, request] {
        if (pendingSnapshot_ && request == snapshotRequest_)
        {
            status_->setText(QStringLiteral("读取编辑内容超时，未写入 output"));
            finishSnapshot(false);
        }
    });
}

void ConversionWindow::finishSnapshot(bool ok)
{
    if (!pendingSnapshot_) return;
    auto done = std::move(pendingSnapshot_);
    pendingSnapshot_ = {};
    done(ok);
    if (!closeApproved_) editor_->sendMarkdownMessage({{"type", "unlock"}});
    updateEditState();
}

void ConversionWindow::checkUnsaved(std::function<void(bool)> completed)
{
    if (pendingSnapshot_) { completed(false); return; }
    if (editor_ && !editor_->isPreviewReady() && !editDirty_)
    {
        completed(true);
        return;
    }
    requestEditSnapshot([this, completed](bool ok) {
        if (ok && !editDirty_) { completed(true); return; }
        // Native CEF nodes sit above QWidget painting; hide them while the Qt dialog is open.
        editor_->hide();
        livePreview_->hide();
        const auto choice = QMessageBox::warning(this, QStringLiteral("未保存的修改"),
            ok ? QStringLiteral("保存当前 Markdown 的修改吗？")
               : QStringLiteral("无法获取最新编辑内容，不会保存旧缓存。放弃所有未保存修改并继续吗？"),
            (ok ? QMessageBox::Save : QMessageBox::NoButton) | QMessageBox::Discard | QMessageBox::Cancel,
            ok ? QMessageBox::Save : QMessageBox::Cancel);
        editor_->show();
        livePreview_->show();
        completed(choice == QMessageBox::Discard || (choice == QMessageBox::Save && saveEdit()));
    });
}

void ConversionWindow::requestClosePermission(std::function<void(bool)> completed)
{
    if (!editor_ || closeApproved_) { completed(true); return; }
    checkUnsaved([this, completed](bool ok) {
        if (ok) closeApproved_ = true;
        completed(ok);
    });
}

void ConversionWindow::handleMarkdown(const QJsonObject &message, bool fromEditor)
{
    const QString type = message.value("type").toString();
    if (type == "outline")
    {
        if (fromEditor)
            handleOutline(message);
        return;
    }
    const double revision = message.value("revision").toDouble(-1);
    if (!std::isfinite(revision) || revision < 0 || revision > 2147483647 ||
        std::floor(revision) != revision) return;
    if (type == "position")
    {
        const double line = message.value("line").toDouble(-1);
        const double fraction = message.value("fraction").toDouble(-1);
        const double progress = message.value("progress").toDouble(-1);
        if (scrollSyncEnabled() && revision == editRevision_ &&
            std::isfinite(line) && line >= 1 && line <= 16777217 && std::floor(line) == line &&
            std::isfinite(fraction) && fraction >= 0 && fraction <= 1 &&
            std::isfinite(progress) && progress >= 0 && progress <= 1)
            (fromEditor ? livePreview_ : editor_)->sendMarkdownMessage(message);
        return;
    }
    if (type == "saveRequest" && !fromEditor)
    {
        requestEditSnapshot([this](bool ok) { if (ok) saveEdit(); });
        return;
    }
    if (!fromEditor || revision < editRevision_) return;
    if (type == "dirty")
    {
        // This revision still has no complete text; never label the old cache with it.
        editDirty_ = true;
        updateEditState();
        return;
    }
    const bool snapshot = message.value("request").toInt(-1) == snapshotRequest_ && pendingSnapshot_;
    if (type == "tooLarge")
    {
        status_->setText(QStringLiteral("内容超过 16 MiB，未保存或更新预览；可缩减内容或放弃修改退出"));
        if (snapshot) finishSnapshot(false);
        return;
    }
    if (type != "change" && type != "save" && type != "snapshot") return;
    if (!message.value("text").isString()) return;
    const QString text = message.value("text").toString();
    if (text.toUtf8().size() > 16 * 1024 * 1024) { if (snapshot) finishSnapshot(false); return; }
    editText_ = text;
    editRevision_ = static_cast<int>(revision);
    editDirty_ = editText_ != savedText_;
    livePreview_->sendMarkdownMessage({{"type", "content"}, {"text", text}, {"revision", editRevision_}});
    if (type == "save") saveEdit();
    if (type == "snapshot" && snapshot) finishSnapshot(true);
    updateEditState();
}

void ConversionWindow::handleOutline(const QJsonObject &message)
{
    if (message.value("type").toString() != "outline" || !message.value("headings").isArray() ||
        !message.value("truncated").isBool() ||
        message.value("revision").toDouble(-1) != editRevision_)
        return;
    const auto values = message.value("headings").toArray();
    if (values.size() > 10000)
        return;
    QVector<MarkdownHeading> headings;
    headings.reserve(values.size());
    int previousLine = 0;
    for (const auto &value : values)
    {
        const auto item = value.toObject();
        const auto title = item.value("title");
        const double level = item.value("level").toDouble(-1);
        const double line = item.value("line").toDouble(-1);
        if (!title.isString() || title.toString().size() > 512 || !std::isfinite(level) ||
            level < 1 || level > 6 || std::floor(level) != level || !std::isfinite(line) ||
            line < 1 || line > 16777217 || std::floor(line) != line || line <= previousLine)
            return;
        headings.append({title.toString(), static_cast<int>(level), static_cast<int>(line)});
        previousLine = static_cast<int>(line);
    }
    navigationSidebar_->setOutline(headings, true, message.value("truncated").toBool());
}

bool ConversionWindow::scrollSyncEnabled() const
{
    // 单向滚动不向另一侧转发位置；同步滚动仍要求左右预览均已就绪。
    return result_ && functionBar_->isScrollSynchronized() && source_->isPreviewReady() &&
           result_->isPreviewReady();
}
