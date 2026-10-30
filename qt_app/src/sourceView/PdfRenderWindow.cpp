/* PDF 通过自身 QBackingStore 提交画面，不从工作区共享画布截取原生子窗口图像。 */
#include "PdfRenderWindow.h"
#include <QCoreApplication>
#include <QEvent>
#include <QExposeEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QWidget>
#include <utility>
#ifdef Q_OS_OPENHARMONY
#include <hilog/log.h>
#endif

PdfRenderWindow::PdfRenderWindow(QWidget *inputTarget, Paint paint)
    : backingStore_(this), inputTarget_(inputTarget), paint_(std::move(paint))
{
    setSurfaceType(QSurface::RasterSurface);
    setObjectName(QStringLiteral("pdfRenderWindow"));
#ifdef Q_OS_OPENHARMONY
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenPDF",
                 "HUIWEN_PDF_SURFACE backend=independent-raster created");
#endif
    connect(this, &QWindow::screenChanged, this,
            [this]
            {
                // 换屏可能改变像素比，同一逻辑尺寸也需要重新分配缓冲。
                backingStore_.resize(size());
                frameValid_ = false;
                invalidate();
            });
}

QRect PdfRenderWindow::bounds() const { return QRect(QPoint(), size()); }

void PdfRenderWindow::invalidate(const QRegion &region)
{
    dirty_ += (region.isEmpty() ? QRegion(bounds()) : region.intersected(bounds()));
    if (isExposed() && !dirty_.isEmpty())
        requestUpdate();
}

void PdfRenderWindow::scrollBy(int dx, int dy)
{
    if (!dx && !dy)
        return;
    const QRegion area(bounds());
    if (!isExposed() || !frameValid_ || backingStore_.size() != size() ||
        !backingStore_.scroll(area, dx, dy))
    {
        invalidate();
        return;
    }
    // 未补画的区域跟随旧画面移动，再补画本次新露出的条带。
    dirty_ = (dirty_.translated(dx, dy) + area.subtracted(area.translated(dx, dy))) & area;
    flushAll_ = true;
    requestUpdate();
}

void PdfRenderWindow::stopRendering()
{
    paint_ = {};
    inputTarget_.clear();
}

bool PdfRenderWindow::event(QEvent *event)
{
    if (event->type() == QEvent::UpdateRequest)
    {
        renderNow();
        return true;
    }
    // 原生窗口收到的滚轮、触控板阶段和键盘事件仍交给原有滚动控制器。
    if (inputTarget_ && (event->type() == QEvent::Wheel || event->type() == QEvent::KeyPress ||
                         event->type() == QEvent::KeyRelease))
        return QCoreApplication::sendEvent(inputTarget_, event);
    if (inputTarget_ && event->type() == QEvent::MouseButtonPress)
        inputTarget_->setFocus(Qt::MouseFocusReason);
    return QWindow::event(event);
}

void PdfRenderWindow::exposeEvent(QExposeEvent *event)
{
    QWindow::exposeEvent(event);
    if (isExposed())
    {
        invalidate();
        renderNow();
    }
}

void PdfRenderWindow::resizeEvent(QResizeEvent *event)
{
    QWindow::resizeEvent(event);
    frameValid_ = false;
    invalidate();
}

void PdfRenderWindow::renderNow()
{
    if (!isExposed() || size().isEmpty() || !paint_)
        return;
    if (backingStore_.size() != size())
    {
        backingStore_.resize(size());
        frameValid_ = false;
    }
    const QRegion area(bounds());
    const QRegion region = frameValid_ ? dirty_.intersected(area) : area;
    dirty_ = {};
    const bool flushAll = flushAll_;
    flushAll_ = false;
    if (!region.isEmpty())
    {
        backingStore_.beginPaint(region);
        QPainter painter(backingStore_.paintDevice());
        painter.setClipRegion(region);
        paint_(painter, region);
        painter.end();
        backingStore_.endPaint();
        frameValid_ = true;
    }
    // 滚动复制也改变了已有像素，提交整个视口；实际绘制仍只处理脏区。
    if (!region.isEmpty() || flushAll)
        backingStore_.flush(flushAll ? area : region);
}
