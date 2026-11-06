/* 标题按级别组成树，跳级标题挂到最近的上级；同一目录重复回传时保留展开状态。 */
#include "MarkdownOutlineSection.h"
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariant>

MarkdownOutlineSection::MarkdownOutlineSection(QWidget *parent)
    : QWidget(parent), tree_(new QTreeWidget(this)), hint_(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setIndentation(20);
    tree_->setTextElideMode(Qt::ElideRight);
    tree_->setUniformRowHeights(true);
    tree_->setAccessibleName(QStringLiteral("Markdown 标题目录"));
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        emit headingActivated(item->data(0, Qt::UserRole).toInt());
    });
    hint_->setText(QStringLiteral("暂无标题"));
    tree_->hide();
}

void MarkdownOutlineSection::setHeadings(const QVector<MarkdownHeading> &headings, bool truncated)
{
    bool same = headings_.size() == headings.size() && truncated_ == truncated;
    for (int index = 0; same && index < headings.size(); ++index)
        same = headings_[index].title == headings[index].title &&
               headings_[index].level == headings[index].level &&
               headings_[index].line == headings[index].line;
    if (same)
        return;
    headings_ = headings;
    truncated_ = truncated;
    tree_->clear();
    QVector<QPair<int, QTreeWidgetItem *>> parents;
    for (const auto &heading : headings)
    {
        if (heading.level < 1 || heading.level > 6 || heading.line < 1)
            continue;
        while (!parents.isEmpty() && parents.last().first >= heading.level)
            parents.removeLast();
        auto *item = new QTreeWidgetItem;
        item->setText(0, heading.title);
        item->setData(0, Qt::UserRole, heading.line);
        if (parents.isEmpty())
            tree_->addTopLevelItem(item);
        else
            parents.last().second->addChild(item);
        parents.append({heading.level, item});
    }
    hint_->setText(truncated ? QStringLiteral("仅显示前 10000 个标题")
                             : QStringLiteral("暂无标题"));
    hint_->setVisible(headings.isEmpty() || truncated);
    tree_->setVisible(!headings.isEmpty());
    tree_->expandAll();
}
