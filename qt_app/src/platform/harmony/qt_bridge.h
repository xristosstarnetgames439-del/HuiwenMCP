/* 鸿蒙事件桥：Qt 请求切回 ArkTS 线程，ArkTS 结果排队投递到 Qt 线程。 */
#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <napi/native_api.h>

class QtBridge : public QObject
{
    Q_OBJECT
  public:
    static QtBridge &instance();
    static quint64 workspaceId();
    // 读取 ArkTS 注册时发布的 mode/kind，同一工作区窗口据此组装内容。
    static QJsonObject workspaceOptions();
    bool request(const QString &action, const QString &path = {});
    bool request(const QString &action, QJsonObject parameters);
  signals:
    void workspaceChanged();
    void closeRequested();
    void replyReceived(const QString &json);
};

void InitQtBridge(napi_env env, napi_value exports);
