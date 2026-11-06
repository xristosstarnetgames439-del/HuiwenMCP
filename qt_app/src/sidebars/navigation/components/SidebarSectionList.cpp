/* 标题条半透明跟随指针；以其他标题条的边缘判定排序，黑线提示目标区块边界。 */
#include "SidebarSectionList.h"
#include "SidebarSection.h"
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <algorithm>

SidebarSectionList::SidebarSectionList(QWidget *parent)
    : QWidget(parent), layout_(new QVBoxLayout(this)), indicator_(new QWidget(this)),
      preview_(new QLabel(this))
{
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
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
        dragOffset_ = mapFromGlobal(position) - section->pos() - title.topLeft();
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
    while (auto *item = layout_->takeAt(0))
        delete item;
    const bool expanded =
        std::any_of(sections_.begin(), sections_.end(),
                    [](SidebarSection *section) { return section->isExpanded(); });
    for (int index = 0; index < sections_.size(); ++index)
    {
        // 全部折叠后，首尾区块贴上下边，中间空白由布局伸缩项承担。
        if (!expanded && index == sections_.size() - 1)
            layout_->addStretch(1);
        auto *section = sections_[index];
        layout_->addWidget(section, section->isExpanded() ? 1 : 0);
    }
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
        const QRect title = section->titleBarRect().translated(section->pos());
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
        destination_ > original ? target->geometry().bottom() + 1 : target->geometry().top();
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
