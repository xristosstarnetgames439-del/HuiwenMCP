/* 鸿蒙事件桥：Qt 请求切回 ArkTS 线程，ArkTS 结果排队投递到 Qt 线程。 */
#pragma once
#include <QObject>
#include <QString>
#include <napi/native_api.h>

class QtBridge : public QObject
{
    Q_OBJECT
  public:
    static QtBridge &instance();
    bool request(const QString &action, const QString &path = {});
  signals:
    void replyReceived(const QString &json);
};

void InitQtBridge(napi_env env, napi_value exports);
