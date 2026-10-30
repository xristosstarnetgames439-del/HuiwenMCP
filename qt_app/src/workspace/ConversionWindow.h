/* 转换工作区主窗口：只组装输入、输出、按钮和服务，不实现区域内部界面。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QWidget>
class DocumentView;
class ConvertButton;
class FunctionBar;
class ConversionService;
class SandboxService;
class WorkspaceSplitter;
class QLabel;

class ConversionWindow : public QWidget
{
  public:
    explicit ConversionWindow(QWidget *parent = nullptr);

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void placeConvertButton();
    void refreshBusyState();
    void refreshScrollSync();
    bool scrollSyncEnabled() const;
    void selectDocument();
    void restoreDocument(const DocumentItem &document);
    WorkspaceSplitter *splitter_;
    DocumentView *source_;
    DocumentView *result_;
    ConvertButton *button_;
    FunctionBar *functionBar_;
    ConversionService *conversion_;
    SandboxService *sandbox_;
    QLabel *status_;
    DocumentItem document_;
    bool picking_ = false;
    bool converting_ = false;
    bool retry_ = false;
};
