/* PDF 独立绘图窗口：自有缓冲隔离分隔条像素，保留滚动时的局部画面复用。 */
#pragma once
#include <QBackingStore>
#include <QPointer>
#include <QRegion>
#include <QWindow>
#include <functional>

class QPainter;
class QWidget;

class PdfRenderWindow final : public QWindow
{
  public:
    using Paint = std::function<void(QPainter &, const QRegion &)>;
    PdfRenderWindow(QWidget *inputTarget, Paint paint);
    void invalidate(const QRegion &region = {});
    void scrollBy(int dx, int dy);
    void stopRendering();

  protected:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    QRect bounds() const;
    void renderNow();
    QBackingStore backingStore_;
    QPointer<QWidget> inputTarget_;
    Paint paint_;
    QRegion dirty_;
    bool frameValid_ = false;
    bool flushAll_ = false;
};
