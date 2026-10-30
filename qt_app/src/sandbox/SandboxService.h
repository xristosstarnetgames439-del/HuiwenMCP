/* 文件选择、任务恢复和目录打开的统一入口；路径与使用时间规则复用 SandboxStore。 */
#pragma once
#include "DocumentItem.h"
#include <QObject>

class SandboxService : public QObject
{
    Q_OBJECT
  public:
    explicit SandboxService(QObject *parent = nullptr);
    void selectDocument(const QString &kind, const QString &mode);
    void openResultLocation(const QString &path);
    void openTaskDirectory(const DocumentItem &document, bool output);
    void markUsed(const DocumentItem &document);
    void finishRestoration(const DocumentItem &document, const QString &error = {});
  signals:
    void documentSelected(const DocumentItem &document);
    void restorationRequested(const DocumentItem &document);
    void selectionFinished();
    void failed(const QString &message);
};
