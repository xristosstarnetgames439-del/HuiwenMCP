/* 排序只移动现有区块；折叠时伸缩空间留在中间，不增加可视窗口或拖动浮窗。 */
#include "SidebarSectionList.h"
#include "SidebarSection.h"
#include <QVBoxLayout>
#include <algorithm>

SidebarSectionList::SidebarSectionList(QWidget *parent)
    : QWidget(parent), layout_(new QVBoxLayout(this)), indicator_(new QWidget(this))
{
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
    indicator_->setFixedHeight(3);
    indicator_->setStyleSheet("background: #777777;");
    indicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
    indicator_->hide();
}

void SidebarSectionList::addSection(SidebarSection *section)
{
    sections_.append(section);
    connect(section, &SidebarSection::stateChanged, this, [this] { arrange(); });
    connect(section, &SidebarSection::dragStarted, this, [this, section](const QPoint &position) {
        dragged_ = section;
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
    auto remaining = sections_;
    remaining.removeAll(dragged_);
    const int y = mapFromGlobal(globalPosition).y();
    destination_ = 0;
    for (auto *section : remaining)
        if (y > section->geometry().center().y())
            ++destination_;
    if (destination_ == sections_.indexOf(dragged_))
    {
        indicator_->hide();
        return;
    }
    const int line = destination_ < remaining.size() ? remaining[destination_]->geometry().top()
                     : remaining.isEmpty()           ? 0
                                                     : remaining.last()->geometry().bottom() + 1;
    indicator_->setGeometry(0, std::clamp(line, 0, std::max(0, height() - 3)), width(), 3);
    indicator_->show();
    indicator_->raise();
}

void SidebarSectionList::finishDrag(bool cancelled)
{
    indicator_->hide();
    if (!dragged_)
        return;
    if (!cancelled && destination_ >= 0)
    {
        sections_.removeAll(dragged_);
        sections_.insert(destination_, dragged_);
        arrange();
    }
    dragged_ = nullptr;
    destination_ = -1;
}
