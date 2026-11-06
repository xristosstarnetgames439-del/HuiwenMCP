/* 整行标题统一处理点击和 300ms 长按；文字按钮仅绘制箭头、文字及接收键盘操作。 */
#include "SidebarSection.h"
#include <QApplication>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

SidebarSection::SidebarSection(const QString &title, QWidget *content, QWidget *parent)
    : QWidget(parent), headerRow_(new QFrame(this)), header_(new QToolButton(headerRow_)),
      content_(content), headerOpacity_(new QGraphicsOpacityEffect(headerRow_))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    headerRow_->setObjectName("navigationSectionHeaderRow");
    headerRow_->setFixedHeight(44);
    headerRow_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerRow_->setCursor(Qt::PointingHandCursor);
    headerRow_->setAttribute(Qt::WA_StyledBackground);
    headerRow_->setAttribute(Qt::WA_Hover);
    headerRow_->installEventFilter(this);
    auto *rowLayout = new QHBoxLayout(headerRow_);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(header_, 1);
    header_->setObjectName("navigationSectionHeader");
    header_->setText(title);
    header_->setAccessibleName(title);
    headerRow_->setToolTip(QStringLiteral("点击折叠；长按标题栏拖动排序"));
    header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header_->setFixedHeight(44);
    header_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header_->setAttribute(Qt::WA_TransparentForMouseEvents); // 文字及空白都交给整行接收。
    header_->setProperty("available", true);
    header_->installEventFilter(this);
    headerOpacity_->setOpacity(1);
    headerRow_->setGraphicsEffect(headerOpacity_);
    content_->setMinimumHeight(0);
    layout->addWidget(headerRow_);
    layout->addWidget(content_, 1);
    connect(header_, &QToolButton::clicked, this, [this] {
        if (available_ && !dragging_)
            setExpanded(!expanded_);
    });
    holdTimer_.setSingleShot(true);
    holdTimer_.setInterval(300);
    connect(&holdTimer_, &QTimer::timeout, this, [this] {
        if (!pressed_)
            return;
        dragging_ = true;
        header_->setDown(false);
        headerRow_->setCursor(Qt::ClosedHandCursor);
        header_->setFocus(Qt::MouseFocusReason);
        // 列表先取得完整标题快照，再将原位置淡化。
        emit dragStarted(pointerPosition_);
        headerOpacity_->setOpacity(0.35);
    });
    setExpanded(true);
}

QRect SidebarSection::titleBarRect() const
{
    return headerRow_->geometry();
}

QPixmap SidebarSection::titleBarPixmap() const
{
    return headerRow_->grab();
}

void SidebarSection::setExpanded(bool expanded)
{
    expanded = expanded && available_;
    if (expanded_ == expanded)
        return;
    expanded_ = expanded;
    content_->setVisible(expanded_);
    header_->setArrowType(expanded_ ? Qt::DownArrow : Qt::RightArrow);
    header_->setAccessibleDescription(expanded_ ? QStringLiteral("已展开，点击折叠")
                                                : QStringLiteral("已折叠，点击展开"));
    setMaximumHeight(expanded_ ? QWIDGETSIZE_MAX : headerRow_->height());
    setMinimumHeight(expanded_ ? headerRow_->height() * 2 : headerRow_->height());
    setSizePolicy(QSizePolicy::Expanding, expanded_ ? QSizePolicy::Expanding : QSizePolicy::Fixed);
    emit stateChanged();
}

void SidebarSection::setAvailable(bool available)
{
    if (available_ == available)
        return;
    available_ = available;
    header_->setProperty("available", available);
    headerRow_->setToolTip(
        available ? QStringLiteral("点击折叠；长按标题栏拖动排序")
                  : QStringLiteral("当前工作区暂无可用 Markdown；长按标题栏可拖动排序"));
    header_->style()->unpolish(header_);
    header_->style()->polish(header_);
    header_->update();
    if (!available)
        setExpanded(false);
}

void SidebarSection::finishDrag(bool cancelled)
{
    holdTimer_.stop();
    pressed_ = false;
    header_->setDown(false);
    if (!dragging_)
        return;
    dragging_ = false;
    headerRow_->setCursor(Qt::PointingHandCursor);
    headerOpacity_->setOpacity(1);
    emit dragFinished(cancelled);
}

bool SidebarSection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != headerRow_ && watched != header_)
        return QWidget::eventFilter(watched, event);
    if (watched == headerRow_ && event->type() == QEvent::MouseButtonPress)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            pressed_ = true;
            header_->setDown(true);
            pressPosition_ = pointerPosition_ = mouse->globalPos();
            holdTimer_.start();
            return true;
        }
    }
    else if (watched == headerRow_ && event->type() == QEvent::MouseMove && pressed_)
    {
        pointerPosition_ = static_cast<QMouseEvent *>(event)->globalPos();
        if (dragging_)
        {
            emit dragMoved(pointerPosition_);
            return true;
        }
        if ((pointerPosition_ - pressPosition_).manhattanLength() >
            QApplication::startDragDistance())
            holdTimer_.stop();
        header_->setDown(headerRow_->rect().contains(static_cast<QMouseEvent *>(event)->pos()));
        return true;
    }
    else if (watched == headerRow_ && event->type() == QEvent::MouseButtonRelease)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            const bool wasDragging = dragging_;
            const bool clicked =
                pressed_ && !wasDragging && headerRow_->rect().contains(mouse->pos());
            if (wasDragging)
                emit dragMoved(mouse->globalPos());
            finishDrag(false);
            if (clicked && available_)
                setExpanded(!expanded_);
            return true;
        }
    }
    else if (event->type() == QEvent::KeyPress && dragging_ &&
             static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
    {
        finishDrag(true);
        return true;
    }
    else if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::Hide ||
             event->type() == QEvent::WindowDeactivate)
        finishDrag(true);
    return QWidget::eventFilter(watched, event);
}
