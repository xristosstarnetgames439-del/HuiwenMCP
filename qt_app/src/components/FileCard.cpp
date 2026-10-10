/* 通用文件卡片的占位绘制；PDF 与 MD 共享样式，不负责文件读写。 */
#include "FileCard.h"
#include <QPainter>

FileCard::FileCard(QWidget *parent) : QPushButton(parent)
{
    setMinimumSize(200, 240);
    setMaximumSize(400, 320);
    setCursor(Qt::PointingHandCursor);
}

void FileCard::setFile(const QString &kind, const QString &name)
{
    kind_ = kind.toUpper();
    name_ = name;
    prompt_.clear();
    setAccessibleName(name);
    setToolTip(name);
    update();
}

void FileCard::setPrompt(const QString &prompt, bool selectable)
{
    kind_.clear();
    name_.clear();
    prompt_ = prompt;
    setEnabled(selectable);
    setAccessibleName(prompt);
    update();
}

void FileCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#667B98"));
    if (kind_.isEmpty())
    {
        if (isEnabled())
        {
            painter.setPen(QPen(QColor("#A7B7CB"), 2, Qt::DashLine));
            painter.drawRoundedRect(rect().adjusted(8, 8, -8, -8), 12, 12);
            painter.setFont(QFont(QStringLiteral("sans-serif"), 36));
            painter.drawText(rect().adjusted(0, 0, 0, -35), Qt::AlignCenter, "+");
        }
        painter.setFont(QFont(QStringLiteral("sans-serif"), 13));
        painter.drawText(rect().adjusted(12, isEnabled() ? 90 : 0, -12, 0), Qt::AlignCenter, prompt_);
        return;
    }
    const QRect icon(width() / 2 - 44, height() / 2 - 82, 88, 110);
    painter.setBrush(QColor("#EAF2FF"));
    painter.setPen(QPen(QColor("#809DC4"), 2));
    painter.drawRoundedRect(icon, 8, 8);
    painter.setFont(QFont(QStringLiteral("sans-serif"), 19, QFont::Bold));
    painter.drawText(icon, Qt::AlignCenter, kind_);
    painter.setFont(QFont(QStringLiteral("sans-serif"), 12));
    painter.setPen(QColor("#183153"));
    painter.drawText(QRect(12, icon.bottom() + 18, width() - 24, 60), Qt::AlignHCenter | Qt::TextWordWrap,
                     name_);
}
