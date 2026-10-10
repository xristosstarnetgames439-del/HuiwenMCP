/* 输入文档区域的独立入口：显示选取状态，通过信号请求系统选择器。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include <QVector>
#include <QWidget>
class FileCard;

class SourceDocumentView : public QWidget
{
    Q_OBJECT
  public:
    explicit SourceDocumentView(QWidget *parent = nullptr);
    void setDocuments(const QVector<DocumentItem> &documents);
    void setBusy(bool busy);
  signals:
    void selectionRequested();

  private:
    FileCard *card_;
};
