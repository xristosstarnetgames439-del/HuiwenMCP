/* 垂直分隔器管理展开高度；全部折叠时贴上下边，标题拖动仍按区块身份排序。 */
#include "SidebarSectionList.h"
#include "SidebarSection.h"
#include <QCoreApplication>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QHoverEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSplitterHandle>
#include <QVBoxLayout>
#include <algorithm>

SidebarSectionList::SidebarSectionList(QWidget *parent)
    : QWidget(parent), layout_(new QVBoxLayout(this)), splitter_(new QSplitter(Qt::Vertical, this)),
      indicator_(new QWidget(this)), preview_(new QLabel(this))
{
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
    splitter_->setObjectName("navigationSectionSplitter");
    splitter_->setChildrenCollapsible(false);
    splitter_->setOpaqueResize(true);
    splitter_->setHandleWidth(12);
    splitter_->setStyleSheet(QStringLiteral(
        "QSplitter#navigationSectionSplitter { border: none; }"
        "QSplitter#navigationSectionSplitter::handle:vertical { background: transparent; }"
        "QSplitter#navigationSectionSplitter::handle:vertical:enabled:hover, "
        "QSplitter#navigationSectionSplitter::handle:vertical:enabled:pressed { background: "
        "#666666; }"));
    connect(splitter_, &QSplitter::splitterMoved, this, [this] {
        const int expanded =
            std::count_if(sections_.begin(), sections_.end(),
                          [](SidebarSection *section) { return section->isExpanded(); });
        if (expanded < 2)
            return;
        const auto sizes = splitter_->sizes();
        for (int index = 0; index < splitter_->count(); ++index)
        {
            auto *section = static_cast<SidebarSection *>(splitter_->widget(index));
            if (section->isExpanded() && sizes[index] > 0)
                preferredHeights_[section] = sizes[index];
        }
    });
    indicator_->setFixedHeight(3);
    indicator_->setStyleSheet("background: #000000;");
    indicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
    indicator_->hide();
    preview_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *opacity = new QGraphicsOpacityEffect(preview_);
    opacity->setOpacity(0.7);
    preview_->setGraphicsEffect(opacity);
    preview_->hide();
}

void SidebarSectionList::addSection(SidebarSection *section)
{
    sections_.append(section);
    connect(section, &SidebarSection::stateChanged, this, [this] { arrange(); });
    connect(section, &SidebarSection::dragStarted, this, [this, section](const QPoint &position) {
        dragged_ = section;
        destination_ = sections_.indexOf(section);
        const auto title = section->titleBarRect();
        dragOffset_ = mapFromGlobal(position) - section->mapTo(this, title.topLeft());
        preview_->setPixmap(section->titleBarPixmap());
        preview_->resize(title.size());
        preview_->show();
        moveDrag(position);
    });
    connect(section, &SidebarSection::dragMoved, this,
            [this](const QPoint &position) { moveDrag(position); });
    connect(section, &SidebarSection::dragFinished, this,
            [this](bool cancelled) { finishDrag(cancelled); });
    arrange();
}

void SidebarSectionList::arrange()
{
    const QSignalBlocker blocker(splitter_); // 折叠、恢复和换位不覆盖用户拖动记录。
    while (auto *item = layout_->takeAt(0))
        delete item;
    const int expanded =
        std::count_if(sections_.begin(), sections_.end(),
                      [](SidebarSection *section) { return section->isExpanded(); });
    if (expanded > 0)
    {
        for (int index = 0; index < sections_.size(); ++index)
        {
            auto *section = sections_[index];
            splitter_->insertWidget(index, section);
            splitter_->setStretchFactor(index, section->isExpanded() ? 1 : 0);
            section->show();
        }
        layout_->addWidget(splitter_, 1);
        splitter_->show();
        layout_->activate();
        for (int index = 1; index < splitter_->count(); ++index)
        {
            auto *handle = splitter_->handle(index);
            handle->setAttribute(Qt::WA_Hover);
            handle->setMouseTracking(true);
            handle->installEventFilter(this);
            handle->setEnabled(expanded > 1);
            handle->setCursor(expanded > 1 ? Qt::SplitVCursor : Qt::ArrowCursor);
        }
        restoreHeights();
        return;
    }
    splitter_->hide();
    for (int index = 0; index < sections_.size(); ++index)
    {
        // 全部折叠后，首尾区块贴上下边，中间空白由布局伸缩项承担。
        if (index == sections_.size() - 1)
            layout_->addStretch(1);
        auto *section = sections_[index];
        layout_->addWidget(section);
        section->show();
    }
    layout_->activate();
}

void SidebarSectionList::restoreHeights()
{
    int available =
        splitter_->height() - std::max(0, splitter_->count() - 1) * splitter_->handleWidth();
    double total = 0;
    for (auto *section : sections_)
    {
        if (section->isExpanded())
            total += preferredHeights_.value(section, 1);
        else
            available -= section->titleBarRect().height();
    }
    if (total <= 0)
        return;
    available = std::max(0, available);
    QList<int> sizes;
    for (auto *section : sections_)
        sizes.append(section->isExpanded()
                         ? static_cast<int>(available * preferredHeights_.value(section, 1) / total)
                         : section->titleBarRect().height());
    splitter_->setSizes(sizes);
}

bool SidebarSectionList::eventFilter(QObject *watched, QEvent *event)
{
    auto *handle = qobject_cast<QSplitterHandle *>(watched);
    const auto type = event->type();
    if (handle && handle->splitter() == splitter_ &&
        (type == QEvent::Enter || type == QEvent::Leave || type == QEvent::MouseMove ||
         type == QEvent::MouseButtonRelease))
    {
        const QPoint position = type == QEvent::MouseMove || type == QEvent::MouseButtonRelease
                                    ? static_cast<QMouseEvent *>(event)->pos()
                                    : handle->mapFromGlobal(QCursor::pos());
        const bool hovered = handle->isEnabled() && type != QEvent::Leave &&
                             (type == QEvent::Enter || handle->rect().contains(position));
        // 用普通鼠标事件补齐 Qt 分隔条自身的悬停状态；按压态仍由原生拖动处理。
        QHoverEvent hover(hovered ? QEvent::HoverEnter : QEvent::HoverLeave,
                          hovered ? QPointF(position) : QPointF(-1, -1), QPointF(position));
        QCoreApplication::sendEvent(handle, &hover);
    }
    return QWidget::eventFilter(watched, event);
}

void SidebarSectionList::moveDrag(const QPoint &globalPosition)
{
    if (!dragged_)
        return;
    const QPoint pointer = mapFromGlobal(globalPosition);
    preview_->move(pointer - dragOffset_);
    preview_->raise();
    const int original = sections_.indexOf(dragged_);
    // 允许越过上下边缘，首尾标题也能插前/插后；横向离开侧栏则不接受换位。
    if (pointer.x() < 0 || pointer.x() >= width())
    {
        destination_ = original;
        indicator_->hide();
        return;
    }
    auto remaining = sections_;
    remaining.removeAll(dragged_);
    int next = 0;
    for (int index = 0; index < remaining.size(); ++index)
    {
        auto *section = remaining[index];
        const QRect title(section->mapTo(this, section->titleBarRect().topLeft()),
                          section->titleBarRect().size());
        // 向下越过标题底边即可插后；回到标题内部时保留结果，越过上边才插前。
        if (pointer.y() > title.bottom() || (pointer.y() >= title.top() && index < destination_))
            ++next;
    }
    destination_ = next;
    if (destination_ == sections_.indexOf(dragged_))
    {
        indicator_->hide();
        return;
    }
    auto *target = sections_[destination_];
    const int line =
        target->mapTo(this, QPoint(0, 0)).y() + (destination_ > original ? target->height() : 0);
    indicator_->setGeometry(0, std::clamp(line, 0, std::max(0, height() - 3)), width(), 3);
    indicator_->show();
    indicator_->raise();
}

void SidebarSectionList::finishDrag(bool cancelled)
{
    indicator_->hide();
    preview_->hide();
    preview_->clear();
    if (!dragged_)
        return;
    if (!cancelled && destination_ >= 0 && destination_ != sections_.indexOf(dragged_))
    {
        sections_.removeAll(dragged_);
        sections_.insert(destination_, dragged_);
        arrange();
    }
    dragged_ = nullptr;
    destination_ = -1;
}
