/* 通用栅格视口：将独立绘图缓冲、Qt 滚动条与滚动能力组合起来，不依赖文档格式。 */
#pragma once
#include "documentView/DocumentRenderer.h"
#include <QAbstractScrollArea>

class DocumentScrollController;
class RasterRenderWindow;
class QPainter;

class RasterDocumentViewport : public QAbstractScrollArea, public DocumentRenderer
{
    Q_OBJECT
  public:
    explicit RasterDocumentViewport(QWidget *parent = nullptr);
    ~RasterDocumentViewport() override;
    DocumentScrollOwner scrollOwner() const override { return DocumentScrollOwner::View; }
    void setViewOptions(const DocumentViewOptions &options) override;

  signals:
    void documentOpened(const QString &path);
    void previewReadyChanged(bool ready);
    void scrollPositionChanged(int page, double fraction, double progress);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void stopScrolling();
    virtual void paintDocument(QPainter &painter, const QRegion &region) = 0;
    RasterRenderWindow *renderWindow_;

  private:
    DocumentScrollController *scrollController_;
};
