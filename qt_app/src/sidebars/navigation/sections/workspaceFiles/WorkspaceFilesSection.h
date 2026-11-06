/* 当前工作区文件树：只展示传入目录，文件激活事件由工作区协调层接收。 */
#pragma once
#include <QWidget>
class QFileSystemModel;
class QLabel;
class QTreeView;
class WorkspaceFileFilter;

class WorkspaceFilesSection : public QWidget
{
    Q_OBJECT
  public:
    explicit WorkspaceFilesSection(QWidget *parent = nullptr);
    void setWorkspace(const QString &taskPath);

  signals:
    void fileActivated(const QString &path);

  private:
    QFileSystemModel *files_;
    WorkspaceFileFilter *filter_;
    QTreeView *tree_;
    QLabel *hint_;
    QString root_;
};
