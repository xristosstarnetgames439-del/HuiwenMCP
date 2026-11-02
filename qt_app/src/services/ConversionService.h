/* Qt 后台转换 API：PDF 转 MD，DOC 转临时 DOCX；不负责文档渲染。 */
#pragma once
#include <QFutureWatcher>
#include <QObject>
#include <memory>

class QTemporaryDir;

struct ConversionResult
{
    int code;
    QString message;
    QString outputPath;
};

class ConversionService : public QObject
{
    Q_OBJECT
  public:
    explicit ConversionService(QObject *parent = nullptr);
    void convertPdfToMd(const QString &inputPath, const QString &outputPath);
    void convertDocToDocx(const QString &inputPath);
    void releaseDocPreview();
  signals:
    void succeeded(const QString &path, const QString &message);
    void failed(int code, const QString &message);

  private:
    QFutureWatcher<ConversionResult> watcher_;
    std::shared_ptr<QTemporaryDir> wordDirectory_;
};
