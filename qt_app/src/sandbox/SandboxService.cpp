/* 沙箱服务实现：鸿蒙异步请求 ArkTS；桌面使用 Qt 选择器及应用数据目录。 */
#include "SandboxService.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#endif

SandboxService::SandboxService(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_OPENHARMONY
    connect(&QtBridge::instance(), &QtBridge::replyReceived, this,
            [this](const QString &json)
            {
                const auto result = QJsonDocument::fromJson(json.toUtf8()).object();
                const auto action = result.value("action").toString();
                if (action == "pick")
                {
                    if (!result.value("inputPath").toString().isEmpty())
                        emit documentSelected({"pdf", result.value("name").toString(),
                                               result.value("inputPath").toString(),
                                               result.value("outputPath").toString()});
                    emit selectionFinished();
                }
                if (!result.value("error").toString().isEmpty())
                    emit failed(result.value("error").toString());
            });
#endif
}

void SandboxService::selectPdf()
{
#ifdef Q_OS_OPENHARMONY
    if (!QtBridge::instance().request("pick"))
    {
        emit selectionFinished();
        emit failed(QStringLiteral("鸿蒙文件选择接口尚未就绪"));
    }
#else
    const QString file =
        QFileDialog::getOpenFileName(nullptr, QStringLiteral("请选择 PDF"), {}, "PDF (*.pdf)");
    if (!file.isEmpty())
    {
        const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString input = root + "/inputs/pdf/" + id + ".pdf";
        const QString output = root + "/outputs/md/" + QFileInfo(file).completeBaseName() + "-" + id + ".md";
        if (QDir().mkpath(QFileInfo(input).absolutePath()) &&
            QDir().mkpath(QFileInfo(output).absolutePath()) && QFile::copy(file, input))
            emit documentSelected({"pdf", QFileInfo(file).fileName(), input, output});
        else
            emit failed(QStringLiteral("复制 PDF 到应用数据目录失败"));
    }
    emit selectionFinished();
#endif
}

void SandboxService::openResultLocation(const QString &path)
{
#ifdef Q_OS_OPENHARMONY
    if (!QtBridge::instance().request("open", path))
        emit failed(QStringLiteral("鸿蒙输出位置接口尚未就绪"));
#else
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())))
        emit failed(QStringLiteral("无法打开输出目录"));
#endif
}
