/* 转换服务复用鸿蒙 PythonRunner；桌面以固定脚本参数调用同一 Python API。 */
#include "ConversionService.h"
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QtConcurrent>
#include <exception>
#ifdef Q_OS_OPENHARMONY
#include "python_runner.h"
#endif

ConversionService::ConversionService(QObject *parent) : QObject(parent)
{
    connect(&watcher_, &QFutureWatcherBase::finished, this,
            [this]
            {
                const auto result = watcher_.result();
                if (result.second.isEmpty())
                {
                    qInfo("HUIWEN_QT_CONVERT_OK");
                    emit succeeded(result.first);
                }
                else
                {
                    qWarning().noquote() << "HUIWEN_QT_CONVERT_FAILED:" << result.second;
                    emit failed(result.second);
                }
            });
}

void ConversionService::convertPdfToMd(const QString &inputPath, const QString &outputPath)
{
    if (watcher_.isRunning())
        return;
    qInfo("HUIWEN_QT_CONVERT_START");
    watcher_.setFuture(QtConcurrent::run(
        [inputPath, outputPath]() -> QPair<QString, QString>
        {
            try
            {
#ifdef Q_OS_OPENHARMONY
                return {QString::fromStdString(
                            RunPdfConversion(inputPath.toStdString(), outputPath.toStdString())),
                        {}};
#else
                const QString python = qEnvironmentVariable("HUIWEN_PYTHON", "python3");
                const QString script = qEnvironmentVariable("HUIWEN_CONVERT_SCRIPT");
                if (script.isEmpty())
                    return {{}, QStringLiteral("请设置 HUIWEN_CONVERT_SCRIPT 为 convert.py 的绝对路径")};
                QProcess process;
                process.start(python, {"-B", script, inputPath, outputPath});
                if (!process.waitForStarted())
                    return {{}, process.errorString()};
                process.waitForFinished(-1);
                if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 ||
                    !QFileInfo::exists(outputPath))
                {
                    const QString error = QString::fromUtf8(process.readAllStandardError());
                    return {{}, error.isEmpty() ? QStringLiteral("Python 未生成 MD 文件") : error};
                }
                return {outputPath, {}};
#endif
            }
            catch (const std::exception &error)
            {
                return {{}, QString::fromUtf8(error.what())};
            }
        }));
}
