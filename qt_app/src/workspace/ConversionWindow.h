/* 转换工作区主窗口：只组装输入、输出、按钮和服务，不实现区域内部界面。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QWidget>
class SourceDocumentView;
class MarkdownResultView;
class ConvertButton;
class ConversionService;
class SandboxService;
class QSplitter;
class QLabel;

class ConversionWindow : public QWidget
{
  public:
    explicit ConversionWindow(QWidget *parent = nullptr);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void placeConvertButton();
    void refreshBusyState();
    QSplitter *splitter_;
    SourceDocumentView *source_;
    MarkdownResultView *result_;
    ConvertButton *button_;
    ConversionService *conversion_;
    SandboxService *sandbox_;
    QLabel *status_;
    DocumentItem document_;
    bool picking_ = false;
    bool converting_ = false;
};
