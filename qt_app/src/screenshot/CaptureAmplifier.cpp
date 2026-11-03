/* 汇文截图放大取色器：展示鼠标附近像素、选区尺寸和 RGB。 */
#include "CaptureAmplifier.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPixmap>

CaptureAmplifier::CaptureAmplifier(std::shared_ptr<QPixmap> originPainting, QWidget *parent)
    : QWidget(parent), originPainting_(originPainting)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setMouseTracking(true);
    sideLength_ = 300;
    imageHeight_ = 200;
    setFixedSize(sideLength_, imageHeight_ + 80);
    // 初始化为屏幕原画尺寸（即整个屏幕分辨率）
    if (originPainting_ && !originPainting_->isNull())
    {
        screenSize_ = originPainting_->size();
    }
    hide();
}

void CaptureAmplifier::onSizeChange(int w, int h)
{
    screenSize_ = QSize(w, h);
}

void CaptureAmplifier::onPostionChange(int x, int y)
{
    cursorPoint_ = QPoint(x, y);
    raise();
    int dest_x = x + 4;
    int dest_y = y + 26;
    const QSize parent_size = parentWidget()->size();
    if (dest_y + height() > parent_size.height())
    {
        dest_y = y - 26 - height();
    }
    if (dest_x + width() > parent_size.width())
    {
        dest_x = x - 4 - width();
    }
    move(dest_x, dest_y);
}

void CaptureAmplifier::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 160));

    QPixmap endPointImage;
    const QSize parent_size = parentWidget()->size();
    const bool inEdge =
        (cursorPoint_.x() + 15 < parent_size.width() && cursorPoint_.x() - 15 > 0) &&
        (cursorPoint_.y() + 11 < parent_size.height() && cursorPoint_.y() - 11 > 0);
    if (inEdge)
    {
        endPointImage =
            originPainting_->copy(QRect(cursorPoint_.x() - 15, cursorPoint_.y() - 11, 30, 22))
                .scaled(sideLength_, imageHeight_);
        painter.drawPixmap(0, 0, endPointImage);
    }
    else
    {
        endPointImage =
            originPainting_->copy(QRect(cursorPoint_.x() - 15, cursorPoint_.y() - 11, 30, 22));
    }

    painter.setPen(QPen(QColor(0, 180, 255, 180), 4));
    painter.drawLine(QPoint(sideLength_ >> 1, 0), QPoint(sideLength_ >> 1, imageHeight_ - 4));
    painter.drawLine(QPoint(0, imageHeight_ >> 1), QPoint(sideLength_, imageHeight_ >> 1));

    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(2, 2, width() - 4, imageHeight_ - 4);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(0, 0, width() - 1, height() - 1);

    const QString select_screen_info =
        QString("%1x%2").arg(screenSize_.width()).arg(screenSize_.height());
    const QImage image = originPainting_->toImage();
    const QColor cursor_pixel = image.pixel(cursorPoint_);
    const QString select_pt_rgb = QString("RGB:(%1,%2,%3)")
                                      .arg(cursor_pixel.red())
                                      .arg(cursor_pixel.green())
                                      .arg(cursor_pixel.blue());

    painter.setPen(Qt::white);
    painter.drawText(QPoint(6, imageHeight_ + 30), select_screen_info);
    painter.drawText(QPoint(6, imageHeight_ + 60), select_pt_rgb);
}
