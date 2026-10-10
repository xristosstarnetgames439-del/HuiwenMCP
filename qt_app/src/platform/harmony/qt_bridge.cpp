/* 鸿蒙 NAPI 双向桥接；线程安全回调属于 Qt Ability，销毁时主动解除。 */
#include "qt_bridge.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>
#include <mutex>

namespace
{
std::mutex callbackMutex;
napi_threadsafe_function callback = nullptr;

void CallArkTs(napi_env env, napi_value fn, void *, void *data)
{
    std::unique_ptr<QByteArray> json(static_cast<QByteArray *>(data));
    if (!env || !fn)
        return;
    napi_value argument, receiver;
    napi_create_string_utf8(env, json->constData(), json->size(), &argument);
    napi_get_undefined(env, &receiver);
    napi_call_function(env, receiver, fn, 1, &argument, nullptr);
}

napi_value Clear(napi_env env, napi_callback_info)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (callback)
        napi_release_threadsafe_function(callback, napi_tsfn_abort);
    callback = nullptr;
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

napi_value Register(napi_env env, napi_callback_info info)
{
    size_t count = 1;
    napi_value fn, name;
    napi_valuetype type;
    napi_get_cb_info(env, info, &count, &fn, nullptr, nullptr);
    if (count != 1 || napi_typeof(env, fn, &type) != napi_ok || type != napi_function)
    {
        napi_throw_type_error(env, nullptr, "需要工作区事件回调");
        return nullptr;
    }
    Clear(env, info);
    napi_create_string_utf8(env, "HuiwenQtWorkspace", NAPI_AUTO_LENGTH, &name);
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (napi_create_threadsafe_function(env, fn, nullptr, name, 0, 1, nullptr, nullptr, nullptr, CallArkTs,
                                        &callback) != napi_ok)
    {
        napi_throw_error(env, nullptr, "无法注册工作区回调");
        return nullptr;
    }
    napi_unref_threadsafe_function(env, callback);
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

napi_value Reply(napi_env env, napi_callback_info info)
{
    size_t count = 1, size = 0;
    napi_value value;
    napi_valuetype type;
    napi_get_cb_info(env, info, &count, &value, nullptr, nullptr);
    if (count != 1 || napi_typeof(env, value, &type) != napi_ok || type != napi_string ||
        napi_get_value_string_utf8(env, value, nullptr, 0, &size) != napi_ok || size > 32768)
    {
        napi_throw_type_error(env, nullptr, "工作区结果必须是有效 JSON 字符串");
        return nullptr;
    }
    QByteArray json(static_cast<int>(size + 1), '\0');
    napi_get_value_string_utf8(env, value, json.data(), json.size(), &size);
    json.resize(static_cast<int>(size));
    if (auto *app = QCoreApplication::instance())
        QMetaObject::invokeMethod(
            app, [json] { emit QtBridge::instance().replyReceived(QString::fromUtf8(json)); },
            Qt::QueuedConnection);
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}
} // namespace

QtBridge &QtBridge::instance()
{
    static QtBridge bridge;
    return bridge;
}

bool QtBridge::request(const QString &action, const QString &path)
{
    auto json = std::make_unique<QByteArray>(
        QJsonDocument(QJsonObject{{"action", action}, {"path", path}}).toJson(QJsonDocument::Compact));
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (!callback || napi_call_threadsafe_function(callback, json.get(), napi_tsfn_nonblocking) != napi_ok)
        return false;
    json.release();
    return true;
}

void InitQtBridge(napi_env env, napi_value exports)
{
    napi_property_descriptor methods[] = {
        {"registerWorkspace", nullptr, Register, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"clearWorkspace", nullptr, Clear, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"workspaceReply", nullptr, Reply, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, 3, methods);
}
