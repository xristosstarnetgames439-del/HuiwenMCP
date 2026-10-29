/* 分隔线拖动反馈：5px 主线与随鼠标移动的 10×180px 圆角指示条。 */
#include "WorkspaceSplitter.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitterHandle>

namespace
{
class WorkspaceSplitterHandle : public QSplitterHandle
{
  public:
    explicit WorkspaceSplitterHandle(QSplitter *parent) : QSplitterHandle(Qt::Horizontal, parent)
    {
        setCursor(Qt::SplitHCursor);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Window));
        const QColor color(dragging_ ? "#546987" : "#D4DEEC");
        const int lineWidth = dragging_ ? 5 : 1;
        painter.fillRect(QRect((width() - lineWidth) / 2, 0, lineWidth, height()), color);
        if (dragging_)
        {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            // 以鼠标 Y 坐标为中心，圆角半径为宽度的一半。
            painter.drawRoundedRect(QRectF((width() - 10) / 2.0, pointerY_ - 90, 10, 180), 5, 5);
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            dragging_ = true;
            pointerY_ = event->localPos().y();
            update();
        }
        QSplitterHandle::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (dragging_)
        {
            pointerY_ = event->localPos().y();
            update();
        }
        QSplitterHandle::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            dragging_ = false;
            update();
        }
        QSplitterHandle::mouseReleaseEvent(event);
    }

    bool event(QEvent *event) override
    {
        if (dragging_ &&
            (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate ||
             event->type() == QEvent::Hide))
        {
            dragging_ = false;
            update();
        }
        return QSplitterHandle::event(event);
    }

  private:
    bool dragging_ = false;
    qreal pointerY_ = 0;
};
} // namespace

WorkspaceSplitter::WorkspaceSplitter(QWidget *parent) : QSplitter(Qt::Horizontal, parent)
{
    setChildrenCollapsible(false);
    // 11px 命中区容纳 10px 指示条，按下或松开时不因线条变粗改变布局。
    setHandleWidth(11);
}

QSplitterHandle *WorkspaceSplitter::createHandle() { return new WorkspaceSplitterHandle(this); }
