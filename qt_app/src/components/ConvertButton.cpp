/* 中部分割线上的悬浮转化按钮；固定小尺寸，不占用整行。 */
#include "ConvertButton.h"

ConvertButton::ConvertButton(QWidget *parent) : QPushButton(QStringLiteral("转化"), parent)
{
    setFixedSize(96, 36);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("QPushButton { background: #2968D9; color: white; border: "
                  "none; border-radius: 8px; }"
                  "QPushButton:disabled { background: #B2BDD0; }");
    setState(false, false);
}

void ConvertButton::setState(bool hasDocument, bool busy)
{
    setEnabled(hasDocument && !busy);
    setText(busy ? QStringLiteral("转化中…") : QStringLiteral("转化"));
}
