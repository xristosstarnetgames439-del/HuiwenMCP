/* 工作区顶部功能条：组装文件与滚动菜单，不依赖预览或转换服务。 */
#pragma once
#include <QToolBar>
class FileMenu;
class QAction;

class FunctionBar : public QToolBar
{
    Q_OBJECT
  public:
    explicit FunctionBar(const QString &mode, QWidget *parent = nullptr);
    FileMenu *fileMenu() const;
    bool isScrollSynchronized() const;

  signals:
    void scrollSyncChanged(bool synchronized);

  private:
    FileMenu *fileMenu_;
    QAction *synchronizedAction_ = nullptr;
};
