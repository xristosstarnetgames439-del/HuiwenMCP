/* 栅格文档共用的窗口承载与输入处理；PDF／其他页面引擎只需实现自己的绘制和定位。 */
#include "RasterDocumentViewport.h"
#include "RasterRenderWindow.h"
#include "components/scrollBar/DocumentScrollBar.h"
#include "components/scrolling/DocumentScrollController.h"
#include <QHideEvent>
#include <QResizeEvent>
#include <QScroller>
#include <QScrollerProperties>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

RasterDocumentViewport::RasterDocumentViewport(QWidget *parent) : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBar(new DocumentScrollBar(this));
    verticalScrollBar()->setSingleStep(40);
    scrollController_ = new DocumentScrollController(verticalScrollBar(), this);
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    touchScroller_ = QScroller::scroller(viewport());
    auto properties = touchScroller_->scrollerProperties();
    // 禁止越界回弹移动 viewport，保持独立原生绘图窗口与视口重合。
    const auto overshoot = QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshoot);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshoot);
    touchScroller_->setScrollerProperties(properties);
    connect(verticalScrollBar(), &QScrollBar::sliderPressed, touchScroller_, &QScroller::stop);
    connect(verticalScrollBar(), &QScrollBar::actionTriggered, touchScroller_, &QScroller::stop);
    // QAbstractScrollArea 自身会忽略滚轮事件；经 viewport 的事件过滤器才能进入 wheelEvent。
    // viewport 已由 Qt 设置焦点代理，键盘和鼠标聚焦仍交给当前滚动区域。
    renderWindow_ =
        new RasterRenderWindow(viewport(), [this](QPainter &painter, const QRegion &region)
                               { paintDocument(painter, region); });
    auto *container = QWidget::createWindowContainer(renderWindow_, viewport());
    // 保留独立原生缓冲；只提升容器节点，不把整个分栏提升为不透明原生窗口。
    container->setAttribute(Qt::WA_DontCreateNativeAncestors);
    container->setFocusPolicy(Qt::StrongFocus);
    auto *layout = new QVBoxLayout(viewport());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(container);
    connect(renderWindow_, &QWindow::visibleChanged, this,
            [this](bool visible)
            {
                if (visible)
                    emit previewReadyChanged(isPreviewReady());
            });
}

RasterDocumentViewport::~RasterDocumentViewport() { renderWindow_->stopRendering(); }

void RasterDocumentViewport::setViewOptions(const DocumentViewOptions &options)
{
    touchScroller_->stop();
    touchInertiaEnabled_ = options.inertiaEnabled;
    scrollController_->setInertiaEnabled(options.inertiaEnabled);
    setVerticalScrollBarPolicy(options.scrollBarVisible ? Qt::ScrollBarAsNeeded
                                                        : Qt::ScrollBarAlwaysOff);
}

void RasterDocumentViewport::stopScrolling()
{
    scrollController_->stopScrolling();
    touchScroller_->stop();
}

bool RasterDocumentViewport::viewportEvent(QEvent *event)
{
    const auto type = event->type();
    if (type != QEvent::TouchBegin && type != QEvent::TouchUpdate && type != QEvent::TouchEnd &&
        type != QEvent::TouchCancel)
        return QAbstractScrollArea::viewportEvent(event);

    auto *touch = static_cast<QTouchEvent *>(event);
    if (touch->device() ? touch->device()->type() != QTouchDevice::TouchScreen
                        : type != QEvent::TouchCancel)
        return QAbstractScrollArea::viewportEvent(event);

    if (type == QEvent::TouchCancel || !isPreviewReady() || touch->touchPoints().size() != 1)
        stopScrolling();
    else
    {
        if (type == QEvent::TouchBegin)
        {
            stopScrolling();
            setFocus(Qt::MouseFocusReason);
        }
        const auto input = type == QEvent::TouchBegin ? QScroller::InputPress
                           : type == QEvent::TouchEnd ? QScroller::InputRelease
                                                      : QScroller::InputMove;
        // 直接送入触屏位置；不注册 TouchGesture，避免拦截现有触控板 Wheel 链路。
        // QAbstractScrollArea 原生处理 QScroller 的 ScrollPrepare/Scroll 并更新滚动条。
        touchScroller_->handleInput(input, touch->touchPoints().first().pos(), touch->timestamp());
        if (type == QEvent::TouchEnd && !touchInertiaEnabled_)
            touchScroller_->stop();
    }
    touch->accept();
    return true;
}

void RasterDocumentViewport::wheelEvent(QWheelEvent *event)
{
    touchScroller_->stop();
    if (isPreviewReady() &&
        scrollController_->handleWheelEvent(event, viewport()->devicePixelRatioF()))
        return;
    stopScrolling();
    QAbstractScrollArea::wheelEvent(event);
}

void RasterDocumentViewport::hideEvent(QHideEvent *event)
{
    stopScrolling();
    QAbstractScrollArea::hideEvent(event);
}

void RasterDocumentViewport::resizeEvent(QResizeEvent *event)
{
    stopScrolling();
    QAbstractScrollArea::resizeEvent(event);
}
