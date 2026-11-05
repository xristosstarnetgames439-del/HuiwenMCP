/* 共用工作区主窗口：按模式组装单栏预览或双栏转化，不实现文档内部渲染。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QWidget>
#include <QJsonObject>
#include <functional>
class DocumentView;
class ConvertButton;
class FunctionBar;
class ConversionService;
class SandboxService;
class WorkspaceSplitter;
class QLabel;
class QAction;
class CefDocumentPreviewWidget;

class ConversionWindow : public QWidget
{
  public:
    explicit ConversionWindow(const QString &mode = "convert", const QString &kind = "pdf",
                              QWidget *parent = nullptr);
    void requestClosePermission(std::function<void(bool)> completed);

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
    void handleMarkdown(const QJsonObject &message, bool fromEditor);
    void requestEditSnapshot(std::function<void(bool)> completed);
    void finishSnapshot(bool ok);
    void checkUnsaved(std::function<void(bool)> completed);
    bool saveEdit();
    void updateEditState();
    const QString mode_;
    const QString kind_;
    CefDocumentPreviewWidget *editor_ = nullptr;
    CefDocumentPreviewWidget *livePreview_ = nullptr;
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
    bool restoringWord_ = false;
    bool retry_ = false;
    QAction *saveAction_ = nullptr;
    QString editText_;
    QString savedText_;
    int editRevision_ = 0;
    int snapshotRequest_ = 0;
    bool editDirty_ = false;
    bool closeApproved_ = false;
    std::function<void(bool)> pendingSnapshot_;
};
