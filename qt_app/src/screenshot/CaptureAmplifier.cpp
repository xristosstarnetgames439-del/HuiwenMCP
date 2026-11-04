/* 汇文截图放大取色器：原图像素网格、HEX／RGB 色值，不显示坐标与尺寸。 */
#include "CaptureAmplifier.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QtMath>

namespace
{
constexpr int kColumns = 15;
constexpr int kRows = 9;
constexpr int kCellSize = 20;
constexpr int kWidth = kColumns * kCellSize;
constexpr int kGridHeight = kRows * kCellSize;
constexpr int kInfoHeight = 104;
} // namespace

CaptureAmplifier::CaptureAmplifier(const QPixmap &originPainting, QWidget *parent)
    : QWidget(parent), originImage_(originPainting.toImage())
{
    // 仅显示，不抢鼠标和键盘；原图只转换一次，移动时不再整屏转成 QImage。
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(kWidth, kGridHeight + kInfoHeight);
    hide();
}

void CaptureAmplifier::onPostionChange(const QPointF &point)
{
    const QSize canvas = parentWidget()->size();
    if (originImage_.isNull() || canvas.isEmpty())
        return;
    // 覆盖层使用逻辑坐标，色卡和中心格始终使用同一个原图像素。
    sourcePixel_ = QPoint(qBound(0, qFloor(point.x() * originImage_.width() / canvas.width()),
                                 originImage_.width() - 1),
                          qBound(0, qFloor(point.y() * originImage_.height() / canvas.height()),
                                 originImage_.height() - 1));
    const int x = qRound(point.x());
    const int y = qRound(point.y());
    int left = x + 16;
    int top = y + 26;
    if (left + width() > canvas.width())
        left = x - 16 - width();
    if (top + height() > canvas.height())
        top = y - 26 - height();
    move(qBound(0, left, qMax(0, canvas.width() - width())),
         qBound(0, top, qMax(0, canvas.height() - height())));
    update();
}

QString CaptureAmplifier::colorText() const
{
    if (originImage_.isNull())
        return {};
    const QColor color = originImage_.pixelColor(sourcePixel_);
    if (hexFormat_)
        return color.name(QColor::HexRgb).toUpper();
    return QStringLiteral("RGB(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
}

void CaptureAmplifier::toggleColorFormat()
{
    hexFormat_ = !hexFormat_;
    update();
}

void CaptureAmplifier::paintEvent(QPaintEvent *)
{
    if (originImage_.isNull())
        return;
    QPainter painter(this);
    // 逐格填充原图颜色，禁用插值；靠近屏幕边缘时也保持中心像素不偏移。
    const QPoint firstPixel = sourcePixel_ - QPoint(kColumns / 2, kRows / 2);
    for (int row = 0; row < kRows; ++row)
    {
        for (int column = 0; column < kColumns; ++column)
        {
            const QPoint point = firstPixel + QPoint(column, row);
            const QColor color = originImage_.rect().contains(point)
                                     ? originImage_.pixelColor(point)
                                     : QColor(0xe8, 0xe8, 0xe8);
            painter.fillRect(column * kCellSize, row * kCellSize, kCellSize, kCellSize, color);
        }
    }
    const QColor crossColor(0x39, 0x83, 0xf7, 80);
    painter.fillRect(kWidth / 2 - kCellSize / 2, 0, kCellSize, kGridHeight, crossColor);
    painter.fillRect(0, kGridHeight / 2 - kCellSize / 2, kWidth, kCellSize, crossColor);
    painter.setPen(QPen(QColor(0, 0, 0, 70), 1));
    for (int column = 1; column < kColumns; ++column)
        painter.drawLine(column * kCellSize, 0, column * kCellSize, kGridHeight);
    for (int row = 1; row < kRows; ++row)
        painter.drawLine(0, row * kCellSize, kWidth, row * kCellSize);

    const QColor color = originImage_.pixelColor(sourcePixel_);
    const QRect center(kWidth / 2 - kCellSize / 2, kGridHeight / 2 - kCellSize / 2, kCellSize,
                       kCellSize);
    painter.fillRect(center, color); // 中心格不叠加蓝色，保留真实色值。
    painter.setPen(QPen(Qt::black, 2));
    painter.drawRect(center);
    painter.setPen(QPen(QColor(0xcb, 0xcb, 0xcb), 1));
    painter.drawRect(0, 0, kWidth - 1, kGridHeight - 1);

    QColor background(Qt::black);
    background.setAlphaF(0.96);
    painter.fillRect(0, kGridHeight, kWidth, kInfoHeight, background);
    QFont textFont = font();
    textFont.setPixelSize(20);
    painter.setFont(textFont);
    const QString text = colorText();
    const int textWidth = QFontMetrics(textFont).horizontalAdvance(text);
    const int swatchLeft = (kWidth - 20 - 12 - textWidth) / 2;
    const QRect swatch(swatchLeft, kGridHeight + 12, 20, 20);
    painter.fillRect(swatch, color);
    painter.setPen(QColor(0xaa, 0xaa, 0xaa));
    painter.drawRect(swatch);
    painter.setPen(Qt::white);
    painter.drawText(QRect(swatch.right() + 12, swatch.top() - 4, textWidth + 1, 28),
                     Qt::AlignVCenter | Qt::AlignLeft, text);
    textFont.setPixelSize(18);
    painter.setFont(textFont);
    painter.drawText(QRect(0, kGridHeight + 44, kWidth, 28), Qt::AlignCenter,
                     QStringLiteral("Shift：切换颜色格式"));
    painter.drawText(QRect(0, kGridHeight + 72, kWidth, 28), Qt::AlignCenter,
                     QStringLiteral("C：复制颜色值"));
}
