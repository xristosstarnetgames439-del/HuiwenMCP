/* 栅格文档共用的窗口承载与输入处理；PDF／其他页面引擎只需实现自己的绘制和定位。 */
#include "RasterDocumentViewport.h"
#include "RasterRenderWindow.h"
#include "components/scrollBar/DocumentScrollBar.h"
#include "components/scrolling/DocumentScrollController.h"
#include <QHideEvent>
#include <QResizeEvent>
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
    scrollController_->setInertiaEnabled(options.inertiaEnabled);
    setVerticalScrollBarPolicy(options.scrollBarVisible ? Qt::ScrollBarAsNeeded
                                                        : Qt::ScrollBarAlwaysOff);
}

void RasterDocumentViewport::stopScrolling() { scrollController_->stopScrolling(); }

void RasterDocumentViewport::wheelEvent(QWheelEvent *event)
{
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
