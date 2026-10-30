/* 工作区水平分隔组件：沿用 Qt 拖动行为，独立维护分隔线外观。 */
#pragma once
#include <QSplitter>

class WorkspaceSplitter : public QSplitter
{
  public:
    explicit WorkspaceSplitter(QWidget *parent = nullptr);
    void raiseAbovePreview();

  protected:
    QSplitterHandle *createHandle() override;
};
