/* 有序区块列表：分配展开空间、上下吸附和排序，不读取任何业务文件。 */
#pragma once
#include <QList>
#include <QWidget>
class QVBoxLayout;
class SidebarSection;
class QLabel;

class SidebarSectionList : public QWidget
{
  public:
    explicit SidebarSectionList(QWidget *parent = nullptr);
    void addSection(SidebarSection *section);

  private:
    void arrange();
    void moveDrag(const QPoint &globalPosition);
    void finishDrag(bool cancelled);
    QVBoxLayout *layout_;
    QWidget *indicator_;
    QLabel *preview_;
    QPoint dragOffset_;
    QList<SidebarSection *> sections_;
    SidebarSection *dragged_ = nullptr;
    int destination_ = -1;
};
