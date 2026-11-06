/* 有序区块列表：分配展开空间、上下吸附和排序，不读取任何业务文件。 */
#pragma once
#include <QHash>
#include <QList>
#include <QWidget>
class QVBoxLayout;
class SidebarSection;
class QLabel;
class QSplitter;

class SidebarSectionList : public QWidget
{
  public:
    explicit SidebarSectionList(QWidget *parent = nullptr);
    void addSection(SidebarSection *section);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void arrange();
    void restoreHeights();
    void moveDrag(const QPoint &globalPosition);
    void finishDrag(bool cancelled);
    QVBoxLayout *layout_;
    QSplitter *splitter_;
    QWidget *indicator_;
    QLabel *preview_;
    QPoint dragOffset_;
    QList<SidebarSection *> sections_;
    QHash<SidebarSection *, int> preferredHeights_;
    SidebarSection *dragged_ = nullptr;
    int destination_ = -1;
};
