/* 可复用文档区域：注入渲染器，统一文件卡片、预览切换和对外信号。 */
#pragma once
#include "DocumentRenderer.h"
#include <QStackedWidget>

class FileCard;

class DocumentView : public QStackedWidget
{
    Q_OBJECT
  public:
    // 用渲染器自身的 Qt 信号接线，避免增加一层只负责转发的 QWidget 或 QObject 适配器。
    template <typename Renderer>
    DocumentView(Renderer *renderer, const QString &emptyPrompt, bool selectableWhenEmpty,
                 QWidget *parent = nullptr)
        : DocumentView(renderer, renderer, emptyPrompt, selectableWhenEmpty, parent)
    {
        connect(renderer, &Renderer::previewReadyChanged, this, &DocumentView::previewReadyChanged);
        connect(renderer, &Renderer::scrollPositionChanged, this,
                &DocumentView::scrollPositionChanged);
        connect(renderer, &Renderer::documentOpened, this,
                [this](const QString &path)
                {
                    if (path == path_)
                        emit documentOpened();
                });
    }

    void setDocument(const QString &path, const QString &kind, const QString &name = {});
    bool hasDocument() const { return !path_.isEmpty(); }
    void setBusy(bool busy);
    void setPreviewMode(bool preview);
    bool isPreviewReady() const;
    void setScrollPosition(int page, double fraction, double progress);
    void publishScrollPosition();
    DocumentScrollOwner scrollOwner() const { return renderer_->scrollOwner(); }
    void setViewOptions(const DocumentViewOptions &options);

  signals:
    void cardActivated(const QString &path);
    void documentOpened();
    void previewReadyChanged(bool ready);
    void scrollPositionChanged(int page, double fraction, double progress);

  private:
    DocumentView(QWidget *rendererWidget, DocumentRenderer *renderer, const QString &emptyPrompt,
                 bool selectableWhenEmpty, QWidget *parent);
    FileCard *card_;
    QWidget *rendererWidget_;
    DocumentRenderer *renderer_;
    QString emptyPrompt_;
    QString path_;
    bool selectableWhenEmpty_;
    bool previewMode_ = false;
    bool busy_ = false;
};
