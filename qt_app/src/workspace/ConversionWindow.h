/* 共用工作区主窗口：按模式组装单栏预览或双栏转化，不实现文档内部渲染。 */
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
    explicit ConversionWindow(const QString &mode = "convert", const QString &kind = "pdf",
                              QWidget *parent = nullptr);

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
    void showDocument(const DocumentItem &document);
    void restoreDocument(const DocumentItem &document);
    const QString mode_;
    const QString kind_;
    WorkspaceSplitter *splitter_;
    DocumentView *source_;
    DocumentView *result_ = nullptr;
    ConvertButton *button_ = nullptr;
    FunctionBar *functionBar_;
    ConversionService *conversion_ = nullptr;
    SandboxService *sandbox_;
    QLabel *status_;
    DocumentItem document_;
    bool picking_ = false;
    bool converting_ = false;
    bool retry_ = false;
};
