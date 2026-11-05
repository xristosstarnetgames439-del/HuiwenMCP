/* 助手共用矢量图标：直接由 QtGui 绘制，不增加 SVG 插件或浏览器依赖。 */
#pragma once
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

enum class AgentIcon
{
    Sidebar,
    Search,
    Plus,
    Settings,
    Send,
    Folder
};

inline QIcon agentIcon(AgentIcon icon, const QColor &color = QColor("#666666"))
{
    QPixmap image(96, 96);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2); // 保留 24px 设计坐标，为放大后的图标提供清晰位图。
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (icon)
    {
    case AgentIcon::Sidebar:
        painter.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
        painter.fillRect(QRectF(15, 4, 5, 16), color);
        break;
    case AgentIcon::Search:
        painter.drawEllipse(QRectF(4, 3, 12, 12));
        painter.drawLine(QPointF(14, 14), QPointF(21, 21));
        break;
    case AgentIcon::Plus:
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        break;
    case AgentIcon::Settings:
        painter.drawEllipse(QRectF(7, 7, 10, 10));
        for (int index = 0; index < 8; ++index)
        {
            painter.save();
            painter.translate(12, 12);
            painter.rotate(index * 45);
            painter.drawLine(QPointF(0, -8), QPointF(0, -10));
            painter.restore();
        }
        painter.drawEllipse(QRectF(10, 10, 4, 4));
        break;
    case AgentIcon::Send:
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
        painter.drawLine(QPointF(13, 6), QPointF(19, 12));
        painter.drawLine(QPointF(13, 18), QPointF(19, 12));
        break;
    case AgentIcon::Folder: {
        QPainterPath path;
        path.moveTo(3, 7);
        path.lineTo(10, 7);
        path.lineTo(12, 10);
        path.lineTo(21, 10);
        path.lineTo(21, 20);
        path.lineTo(3, 20);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    }
    painter.end();
    return QIcon(image);
}
