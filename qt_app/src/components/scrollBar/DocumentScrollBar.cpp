/* 文档滚动条样式：24px 宽、最小高度 30px、灰色直角滑块与透明轨道。 */
#include "DocumentScrollBar.h"

DocumentScrollBar::DocumentScrollBar(QWidget *parent) : QScrollBar(Qt::Vertical, parent)
{
    setStyleSheet(QStringLiteral(
        "QScrollBar:vertical { width: 24px; margin: 0; border: none; background: transparent; }"
        "QScrollBar::handle:vertical { min-height: 30px; "
        "background: #c6c6c6; }"
        "QScrollBar::handle:vertical:hover { background: rgba(0, 0, 0, 128); }"
        "QScrollBar::handle:vertical:pressed { background: rgba(0, 0, 0, 160); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical "
        "{ height: 0; border: none; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical "
        "{ background: transparent; }"));
}
