/* 通用栅格绘图窗口：独立缓冲隔离相邻原生窗口，复用滚动后的画面。 */
#pragma once
#include <QBackingStore>
#include <QPointer>
#include <QRegion>
#include <QWindow>
#include <functional>

class QPainter;
class QWidget;

class RasterRenderWindow final : public QWindow
{
  public:
    using Paint = std::function<void(QPainter &, const QRegion &)>;
    RasterRenderWindow(QWidget *inputTarget, Paint paint);
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
