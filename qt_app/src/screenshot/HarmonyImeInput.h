/* 汇文截图鸿蒙输入法客户端：在 Qt 线程提交文字，结束时释放进程内 IME。 */
#ifndef HUIWEN_HARMONY_IME_INPUT_H
#define HUIWEN_HARMONY_IME_INPUT_H

#include <QPointer>
#include <functional>

class QLineEdit;
class QPlainTextEdit;
class QWidget;

// 鸿蒙输入法（IME Kit C API）客户端：供汇文截图文字编辑使用。
//
// IME C API 每个进程只允许一个客户端（NDK 里 g_inputMethodProxy 是全局单例）。CEF 走同一套
// C API，attach 时会把 Qt 插件已注册的客户端置为 detached，而 Qt 侧 isAttached() 仍为真、
// 不再重新 attach → 文字输入只剩物理按键（hilog: "Input method client detached"）。
// 故绕开 Qt 插件自建客户端：start() attach + showTextInput + showKeyboard，stop() detach 释放，
// 提交的文字经 insertText 回调写回当前绑定的编辑器。
class HarmonyImeInput
{
  public:
    static HarmonyImeInput &instance();

    // 接入系统输入法并绑定输入框；失败返回 false（此时仍可用物理键盘输入英文）
    bool start(QLineEdit *editor);
    bool start(QPlainTextEdit *editor);
    // 指定 editor 时仅释放该编辑器的连接，防止其他编辑器失焦／销毁误关当前输入。
    void stop(QWidget *editor = nullptr);
    bool isStarted(QWidget *editor = nullptr) const;

    // 输入法按下回车时回调（用于提交当前文字）
    void setEnterCallback(std::function<void()> cb);
    // 通知输入法刷新光标位置（编辑器移动/内容变化后调用）
    void notifyCursor();

  private:
    bool startEditor(QWidget *editor);
    HarmonyImeInput() = default;
    ~HarmonyImeInput() = default;
    HarmonyImeInput(const HarmonyImeInput &) = delete;
    HarmonyImeInput &operator=(const HarmonyImeInput &) = delete;
};

#endif // HUIWEN_HARMONY_IME_INPUT_H
