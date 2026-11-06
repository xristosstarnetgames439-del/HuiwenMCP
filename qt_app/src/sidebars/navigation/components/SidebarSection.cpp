/* 鼠标长按 300ms 才进入排序；普通点击仍负责折叠，正文树控件不参与拖动。 */
#include "SidebarSection.h"
#include <QApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

SidebarSection::SidebarSection(const QString &title, QWidget *content, QWidget *parent)
    : QWidget(parent), header_(new QToolButton(this)), content_(content)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    header_->setObjectName("navigationSectionHeader");
    header_->setText(title);
    header_->setAccessibleName(title);
    header_->setToolTip(QStringLiteral("点击折叠；长按标题栏拖动排序"));
    header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header_->setFixedHeight(44);
    header_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header_->setCursor(Qt::PointingHandCursor);
    header_->setProperty("available", true);
    header_->installEventFilter(this);
    content_->setMinimumHeight(0);
    layout->addWidget(header_);
    layout->addWidget(content_, 1);
    connect(header_, &QToolButton::clicked, this, [this] {
        if (available_)
            setExpanded(!expanded_);
    });
    holdTimer_.setSingleShot(true);
    holdTimer_.setInterval(300);
    connect(&holdTimer_, &QTimer::timeout, this, [this] {
        if (!pressed_)
            return;
        dragging_ = true;
        header_->setDown(false);
        header_->setCursor(Qt::ClosedHandCursor);
        emit dragStarted(pointerPosition_);
    });
    setExpanded(true);
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
    setMaximumHeight(expanded_ ? QWIDGETSIZE_MAX : header_->height());
    setMinimumHeight(header_->height());
    setSizePolicy(QSizePolicy::Expanding, expanded_ ? QSizePolicy::Expanding : QSizePolicy::Fixed);
    emit stateChanged();
}

void SidebarSection::setAvailable(bool available)
{
    if (available_ == available)
        return;
    available_ = available;
    header_->setProperty("available", available);
    header_->setToolTip(available
                            ? QStringLiteral("点击折叠；长按标题栏拖动排序")
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
    if (!dragging_)
        return;
    dragging_ = false;
    header_->setDown(false);
    header_->setCursor(Qt::PointingHandCursor);
    emit dragFinished(cancelled);
}

bool SidebarSection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != header_)
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            pressed_ = true;
            pressPosition_ = pointerPosition_ = mouse->globalPos();
            holdTimer_.start();
        }
    }
    else if (event->type() == QEvent::MouseMove && pressed_)
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
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            const bool wasDragging = dragging_;
            finishDrag(false);
            if (wasDragging)
                return true;
        }
    }
    else if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::Hide ||
             event->type() == QEvent::WindowDeactivate)
        finishDrag(true);
    return QWidget::eventFilter(watched, event);
}
