/* PDF 正文预览：按区域宽度排版，后台渲染可见页并预取相邻页。 */
#pragma once
#include <QAbstractScrollArea>
#include <QCache>
#include <QFutureWatcher>
#include <QImage>
#include <QPropertyAnimation>
#include <QRectF>
#include <QTimer>
#include <QVector>
#include <memory>

struct PdfPreviewDocument;
struct PdfPreviewResult
{
    quint64 generation = 0;
    int page = -1;
    QVector<QSizeF> pageSizes;
    QImage image;
    QString error;
};

class PdfPreviewWidget : public QAbstractScrollArea
{
    Q_OBJECT
  public:
    explicit PdfPreviewWidget(QWidget *parent = nullptr);
    ~PdfPreviewWidget() override;
    void setDocument(const QString &path);
    bool isPreviewReady() const;
    void setScrollPosition(int page, double fraction, double progress);
    void publishScrollPosition();

  signals:
    void documentOpened(const QString &path);
    void previewReadyChanged(bool ready);
    void scrollPositionChanged(int page, double fraction, double progress);

  protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

  private:
    void layoutPages();
    void requestRender();
    int firstVisiblePage() const;
    QSize renderSize(int page) const;
    QString path_;
    QString message_;
    QString layoutError_;
    quint64 generation_ = 0;
    bool loadPending_ = false;
    bool taskPending_ = false;
    bool applyingScroll_ = false;
    int renderPixelBudget_ = 1;
    std::shared_ptr<PdfPreviewDocument> document_;
    QVector<QSizeF> pageSizes_;
    QVector<QRectF> pageRects_;
    QCache<int, QImage> images_{64 * 1024 * 1024};
    QFutureWatcher<PdfPreviewResult> watcher_;
    QPropertyAnimation scrollAnimation_;
    QTimer resizeTimer_;
    QTimer scrollReportTimer_;
};
