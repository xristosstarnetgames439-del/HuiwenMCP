/* Qt 的 PDF 转 MD API：异步调用固定 Python 转换入口并返回文件路径。 */
#pragma once
#include <QFutureWatcher>
#include <QObject>

class ConversionService : public QObject
{
    Q_OBJECT
  public:
    explicit ConversionService(QObject *parent = nullptr);
    void convertPdfToMd(const QString &inputPath, const QString &outputPath);
  signals:
    void succeeded(const QString &path);
    void failed(const QString &message);

  private:
    QFutureWatcher<QPair<QString, QString>> watcher_;
};
