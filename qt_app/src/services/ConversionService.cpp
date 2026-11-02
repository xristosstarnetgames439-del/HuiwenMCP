/* 转换服务复用鸿蒙 PythonRunner；桌面以固定脚本参数调用同一 Python API。 */
#include "ConversionService.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
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
                if (result.code == 0)
                {
                    qInfo().noquote() << "HUIWEN_OUTPUT stage=convert_success HUIWEN_QT_CONVERT_OK code=" << result.code
                                     << "message=" << result.message << "path=" << result.outputPath
                                     << "bytes=" << QFileInfo(result.outputPath).size();
                    emit succeeded(result.outputPath, result.message);
                }
                else
                {
                    qWarning().noquote() << "HUIWEN_OUTPUT stage=convert_failed HUIWEN_QT_CONVERT_FAILED code=" << result.code
                                        << "message=" << result.message;
                    emit failed(result.code, result.message);
                }
            });
}

void ConversionService::convertPdfToMd(const QString &inputPath, const QString &outputPath)
{
    if (watcher_.isRunning())
        return;
    qInfo().noquote() << "HUIWEN_OUTPUT stage=convert_start HUIWEN_QT_CONVERT_START input=" << inputPath << "output=" << outputPath;
    watcher_.setFuture(QtConcurrent::run(
        [inputPath, outputPath]() -> ConversionResult
        {
            try
            {
#ifdef Q_OS_OPENHARMONY
                const auto result = RunPdfConversionResult(inputPath.toStdString(), outputPath.toStdString());
                if (result.code != 0)
                    return {result.code, QString::fromStdString(result.message), {}};
                // 进程正常退出不等于文件落盘成功，读取最终文件后才通知界面成功。
                const QFileInfo output(outputPath);
                QFile file(outputPath);
                if (!output.isAbsolute() || output.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) != 0 ||
                    !output.isFile() || output.size() <= 0 || !file.open(QIODevice::ReadOnly) || file.read(1).isEmpty())
                {
                    qWarning().noquote() << "HUIWEN_OUTPUT stage=verify_failed path=" << outputPath
                                         << "exists=" << output.exists() << "bytes=" << output.size();
                    return {2002, QStringLiteral("转化失败：Markdown 文件未正确生成，请重新转化。"), {}};
                }
                qInfo().noquote() << "HUIWEN_OUTPUT stage=verified path=" << output.absoluteFilePath()
                                 << "directory=" << output.absolutePath() << "bytes=" << output.size();
                return {0, QString::fromStdString(result.message), output.absoluteFilePath()};
#else
                const QString python = qEnvironmentVariable("HUIWEN_PYTHON", "python3");
                const QString script = qEnvironmentVariable("HUIWEN_CONVERT_SCRIPT");
                if (script.isEmpty())
                    return {2001, QStringLiteral("请设置 HUIWEN_CONVERT_SCRIPT 为 convert.py 的绝对路径"), {}};
                QProcess process;
                process.start(python, {"-B", script, inputPath, outputPath});
                if (!process.waitForStarted())
                    return {2001, process.errorString(), {}};
                process.waitForFinished(-1);
                if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 ||
                    !QFileInfo::exists(outputPath))
                {
                    const QString error = QString::fromUtf8(process.readAllStandardError());
                    return {2001, error.isEmpty() ? QStringLiteral("Python 未生成 MD 文件") : error, {}};
                }
                return {0, QStringLiteral("转化成功，Markdown 文件已生成。"), outputPath};
#endif
            }
            catch (const std::exception &error)
            {
                qWarning().noquote() << "HUIWEN_OUTPUT stage=runtime_failed detail=" << error.what();
                return {2001, QStringLiteral("转化失败：转换服务暂不可用，请重新转化；若仍失败，请重启应用。"), {}};
            }
        }));
}

void ConversionService::releaseDocPreview()
{
    wordDirectory_.reset();
}

void ConversionService::convertDocToDocx(const QString &inputPath)
{
    if (watcher_.isRunning())
        return;
    // 在当前沙箱任务内生成临时副本；后台任务持有目录，关窗也不会提前删除正在写的文件。
    wordDirectory_ = std::make_shared<QTemporaryDir>(QFileInfo(inputPath).absolutePath() +
                                                     QStringLiteral("/.word-preview-XXXXXX"));
    const auto directory = wordDirectory_;
    watcher_.setFuture(QtConcurrent::run([inputPath, directory]() -> ConversionResult {
        try
        {
            if (!directory->isValid())
                return {2001, QStringLiteral("无法创建 DOC 预览临时目录"), {}};
            const QString outputPath = directory->filePath(QStringLiteral("preview.docx"));
#ifdef Q_OS_OPENHARMONY
            RunDocConversion(inputPath.toStdString(), outputPath.toStdString());
#else
            const QString script = qEnvironmentVariable("HUIWEN_DOC_CONVERT_SCRIPT");
            if (script.isEmpty())
                return {2001,
                        QStringLiteral("桌面调试请设置 HUIWEN_DOC_CONVERT_SCRIPT 为 "
                                       "word2docx/convert.py 的绝对路径"),
                        {}};
            QProcess process;
            process.start(qEnvironmentVariable("HUIWEN_PYTHON", "python3"),
                          {"-B", script, inputPath, outputPath});
            if (!process.waitForStarted())
                return {2001, process.errorString(), {}};
            process.waitForFinished(-1);
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
                return {2001, QString::fromUtf8(process.readAllStandardError()), {}};
#endif
            QFile output(outputPath);
            if (!output.open(QIODevice::ReadOnly) || output.size() <= 0 ||
                output.read(4) != QByteArray("PK\x03\x04", 4))
                return {2002, QStringLiteral("DOC 转换未生成有效的 DOCX 文件"), {}};
            return {0, QStringLiteral("DOC 转换完成"), outputPath};
        }
        catch (const std::exception &error)
        {
            return {2001, QStringLiteral("DOC 转换失败：") + QString::fromUtf8(error.what()), {}};
        }
    }));
}
