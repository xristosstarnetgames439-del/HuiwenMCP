// 与通达适配层约定一致：libcefsimple.so 导出 CefMain，供原生子进程调用。
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_render_process_handler.h"
#include "log/tongda_log.h"
#include <cmath>
#include <hilog/log.h>

// 包装层在主进程和子进程均使用此日志 ABI，不依赖 Qt 或通达业务目录。
namespace tongda_log
{
__attribute__((visibility("default"))) void Write(int severity, const char *, int, const std::string &message)
{
    OH_LOG_Print(LOG_APP, severity >= 2 ? LOG_ERROR : LOG_INFO, 0xFF00, "HuiwenCEF", "%{public}s",
                 message.c_str());
}
} // namespace tongda_log

namespace
{
class PreviewReporter final : public CefV8Handler
{
  public:
    bool Execute(const CefString &, CefRefPtr<CefV8Value>, const CefV8ValueList &arguments,
                 CefRefPtr<CefV8Value> &retval, CefString &) override
    {
        if (arguments.size() != 2 || !arguments[0]->IsBool() || !arguments[1]->IsString())
            return false;
        const auto error = arguments[1]->GetStringValue().ToString();
        const auto context = CefV8Context::GetCurrentContext();
        if (error.size() > 4096 || !context || !context->GetFrame()->IsMain())
            return false;
        const auto message = CefProcessMessage::Create("Huiwen.PreviewState");
        message->GetArgumentList()->SetBool(0, arguments[0]->GetBoolValue());
        message->GetArgumentList()->SetString(1, error);
        context->GetFrame()->SendProcessMessage(PID_BROWSER, message);
        retval = CefV8Value::CreateUndefined();
        return true;
    }

  private:
    IMPLEMENT_REFCOUNTING(PreviewReporter);
};

class ScrollReporter final : public CefV8Handler
{
  public:
    bool Execute(const CefString &, CefRefPtr<CefV8Value>, const CefV8ValueList &arguments,
                 CefRefPtr<CefV8Value> &retval, CefString &) override
    {
        if (arguments.size() != 3)
            return false;
        for (const auto &value : arguments)
            if (!value->IsInt() && !value->IsUInt() && !value->IsDouble())
                return false;
        const double page = arguments[0]->GetDoubleValue();
        const double fraction = arguments[1]->GetDoubleValue();
        const double progress = arguments[2]->GetDoubleValue();
        if (!std::isfinite(page) || page < 0 || page > 100000 || std::floor(page) != page ||
            !std::isfinite(fraction) || fraction < 0 || fraction > 1 ||
            !std::isfinite(progress) || progress < 0 || progress > 1)
            return false;
        const auto context = CefV8Context::GetCurrentContext();
        if (!context || !context->GetFrame()->IsMain())
            return false;
        const auto message = CefProcessMessage::Create("Huiwen.PreviewScroll");
        const auto values = message->GetArgumentList();
        values->SetInt(0, static_cast<int>(page));
        values->SetDouble(1, fraction);
        values->SetDouble(2, progress);
        context->GetFrame()->SendProcessMessage(PID_BROWSER, message);
        retval = CefV8Value::CreateUndefined();
        return true;
    }

  private:
    IMPLEMENT_REFCOUNTING(ScrollReporter);
};

class PreviewRenderApp final : public CefApp, public CefRenderProcessHandler
{
  public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
    void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override
    {
        const auto url = frame->GetURL().ToString();
        if (frame->IsMain() && (url == "https://huiwen-md.invalid/preview.html" ||
                               url.rfind("data:text/html;base64,", 0) == 0))
        {
            context->GetGlobal()->SetValue(
                "huiwenReportScroll", CefV8Value::CreateFunction("huiwenReportScroll", new ScrollReporter),
                V8_PROPERTY_ATTRIBUTE_READONLY);
            context->GetGlobal()->SetValue(
                "huiwenReportPreview",
                CefV8Value::CreateFunction("huiwenReportPreview", new PreviewReporter),
                V8_PROPERTY_ATTRIBUTE_READONLY);
        }
    }

  private:
    IMPLEMENT_REFCOUNTING(PreviewRenderApp);
};
} // namespace

extern "C" __attribute__((visibility("default"))) int CefMain(int argc, char *argv[])
{
    const auto commandLine = CefCommandLine::CreateCommandLine();
    commandLine->InitFromArgv(argc, argv);
    const std::string processType = commandLine->GetSwitchValue("type").ToString();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "SUBPROCESS start type=%{public}s argc=%{public}d",
                 processType.c_str(), argc);
    // 子进程只执行 Chromium 工作循环；不创建 Qt 应用，也不再次初始化浏览器主进程。
    const int exitCode = CefExecuteProcess(CefMainArgs(argc, argv), new PreviewRenderApp, nullptr);
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "HuiwenCEF", "SUBPROCESS exit type=%{public}s code=%{public}d",
                 processType.c_str(), exitCode);
    return exitCode;
}
