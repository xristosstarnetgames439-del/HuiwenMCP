/* 文件选择和输出位置的统一入口；鸿蒙复用 DocumentStore 的沙箱规则。 */
#pragma once
#include "DocumentItem.h"
#include <QObject>

class SandboxService : public QObject
{
    Q_OBJECT
  public:
    explicit SandboxService(QObject *parent = nullptr);
    void selectPdf();
    void openResultLocation(const QString &path);
  signals:
    void documentSelected(const DocumentItem &document);
    void selectionFinished();
    void failed(const QString &message);
};
