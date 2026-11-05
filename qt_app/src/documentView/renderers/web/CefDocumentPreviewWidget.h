/* 共用 CEF 只读预览承载；MD 与 Word 共用同一个运行时。 */
#pragma once
#include "documentView/DocumentRenderer.h"
#include <QWidget>
#include <QJsonObject>
#include <functional>
#include <memory>

class CefDocumentPreviewWidget : public QWidget, public DocumentRenderer
{
    Q_OBJECT
  public:
    enum class Format
    {
        Markdown,
        Word,
        MarkdownEditor,
        MarkdownLive
    };
    explicit CefDocumentPreviewWidget(Format format, QWidget *parent = nullptr);
    ~CefDocumentPreviewWidget() override;
    void setDocument(const QString &path) override;
    bool isPreviewReady() const override;
    void setScrollPosition(int page, double fraction, double progress) override;
    void publishScrollPosition() override;
    DocumentScrollOwner scrollOwner() const override { return DocumentScrollOwner::Backend; }
    void setViewOptions(const DocumentViewOptions &options) override;
    void sendMarkdownMessage(const QJsonObject &message);
    // 单工作区关闭时等待全部浏览器退出；不停止进程级 CEF 引擎。
    static void closeWorkspaceBrowsers(std::function<void()> completed);

  signals:
    void documentOpened(const QString &path);
    void previewReadyChanged(bool ready);
    void scrollPositionChanged(int page, double fraction, double progress);
    void markdownMessage(const QJsonObject &message);

  protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void openBrowser();
    void closeBrowser();
    void syncGeometry();
    QRect browserRect() const;
    class Private;
    std::unique_ptr<Private> d_;
};
