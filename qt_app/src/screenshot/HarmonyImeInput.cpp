/* 汇文截图鸿蒙输入法客户端：在 Qt 线程提交文字，结束时释放进程内 IME。 */
#include "HarmonyImeInput.h"

#include <QDebug>

#include <QApplication>
#include <QLineEdit>
#include <QList>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QString>
#include <QTextCursor>
#include <QWidget>

#include <inputmethod/inputmethod_attach_options_capi.h>
#include <inputmethod/inputmethod_controller_capi.h>
#include <inputmethod/inputmethod_cursor_info_capi.h>
#include <inputmethod/inputmethod_inputmethod_proxy_capi.h>
#include <inputmethod/inputmethod_text_config_capi.h>
#include <inputmethod/inputmethod_text_editor_proxy_capi.h>
#include <inputmethod/inputmethod_types_capi.h>

#include <atomic>
#include <mutex>

namespace
{

InputMethod_TextEditorProxy *g_textProxy = nullptr;
InputMethod_InputMethodProxy *g_imeProxy = nullptr;
InputMethod_AttachOptions *g_options = nullptr;
QPointer<QWidget> g_editor;
std::function<void()> g_enterCb;
std::atomic<bool> g_started{false};
std::atomic<unsigned int> g_session{0};
QList<QMetaObject::Connection> g_conns;

// 输入法回调运行在 IPC 线程，QWidget 只能在 Qt 主线程访问；
// 因此把编辑器内容/光标同步一份影子数据，供输入法同步读取。
std::mutex g_shadowMtx;
QString g_shadowText;
int g_shadowCursor = 0;

void postToQt(const std::function<void()> &fn)
{
    const unsigned int session = g_session.load();
    QMetaObject::invokeMethod(
        qApp,
        [session, fn]() {
            // 切换编辑器后丢弃上一连接排队中的回调，避免文字落入另一个窗口。
            if (session == g_session.load())
                fn();
        },
        Qt::QueuedConnection);
}

void snapshotShadow(QString &text, int &cursor)
{
    std::lock_guard<std::mutex> lock(g_shadowMtx);
    text = g_shadowText;
    cursor = g_shadowCursor;
}

// 把 source 的 [start, start+count) 按 UTF-16 拷入输入法提供的缓冲区；
// length 入参为缓冲区容量，出参为实际写入长度（见 native_text_changed_listener.cpp）
void copyTextToBuffer(const QString &source, int start, int count, char16_t text[], size_t *length)
{
    if (text == nullptr || length == nullptr)
        return;
    const int capacity = static_cast<int>(*length);
    if (start < 0)
        start = 0;
    if (start > source.size())
        start = source.size();
    int copyCount = qMin(count, source.size() - start);
    copyCount = qBound(0, copyCount, qMax(0, capacity));
    const QString part = source.mid(start, copyCount);
    for (int i = 0; i < part.size(); ++i)
    {
        text[i] = static_cast<char16_t>(part.at(i).unicode());
    }
    *length = static_cast<size_t>(copyCount);
}

void onInsertText(InputMethod_TextEditorProxy *, const char16_t *text, size_t length)
{
    if (text == nullptr || length == 0)
        return;
    const QString inserted = QString::fromUtf16(text, static_cast<int>(length));
    if (inserted.isEmpty())
        return;
    postToQt([inserted]() {
        if (QLineEdit *edit = qobject_cast<QLineEdit *>(g_editor.data()))
        {
            edit->insert(inserted);
            HarmonyImeInput::instance().notifyCursor();
        }
        else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            edit->insertPlainText(inserted);
        }
    });
}

void onDeleteBackward(InputMethod_TextEditorProxy *, int32_t length)
{
    if (length <= 0)
        return;
    postToQt([length]() {
        if (auto *edit = qobject_cast<QLineEdit *>(g_editor.data()))
        {
            for (int32_t i = 0; i < length; ++i)
                edit->backspace();
        }
        else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            QTextCursor cursor = edit->textCursor();
            cursor.beginEditBlock();
            for (int32_t i = 0; i < length; ++i)
                cursor.deletePreviousChar();
            cursor.endEditBlock();
            edit->setTextCursor(cursor);
        }
    });
}

void onDeleteForward(InputMethod_TextEditorProxy *, int32_t length)
{
    if (length <= 0)
        return;
    postToQt([length]() {
        if (auto *edit = qobject_cast<QLineEdit *>(g_editor.data()))
        {
            for (int32_t i = 0; i < length; ++i)
                edit->del();
        }
        else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            QTextCursor cursor = edit->textCursor();
            cursor.beginEditBlock();
            for (int32_t i = 0; i < length; ++i)
                cursor.deleteChar();
            cursor.endEditBlock();
            edit->setTextCursor(cursor);
        }
    });
}

void onSendEnterKey(InputMethod_TextEditorProxy *, InputMethod_EnterKeyType)
{
    postToQt([]() {
        if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            edit->insertPlainText(QStringLiteral("\n"));
            return;
        }
        // 先拷贝再调用：回调内部会 stop()，从而销毁 g_enterCb
        const std::function<void()> callback = g_enterCb;
        if (callback)
            callback();
    });
}

void onMoveCursor(InputMethod_TextEditorProxy *, InputMethod_Direction direction)
{
    postToQt([direction]() {
        if (auto *edit = qobject_cast<QLineEdit *>(g_editor.data()))
        {
            if (direction == IME_DIRECTION_LEFT)
            {
                edit->cursorBackward(false, 1);
            }
            else if (direction == IME_DIRECTION_RIGHT)
            {
                edit->cursorForward(false, 1);
            }
        }
        else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            if (direction == IME_DIRECTION_LEFT)
                edit->moveCursor(QTextCursor::Left);
            else if (direction == IME_DIRECTION_RIGHT)
                edit->moveCursor(QTextCursor::Right);
            else if (direction == IME_DIRECTION_UP)
                edit->moveCursor(QTextCursor::Up);
            else if (direction == IME_DIRECTION_DOWN)
                edit->moveCursor(QTextCursor::Down);
        }
    });
}

void onHandleSetSelection(InputMethod_TextEditorProxy *, int32_t start, int32_t end)
{
    postToQt([start, end]() {
        if (auto *edit = qobject_cast<QLineEdit *>(g_editor.data()))
        {
            const int selStart = qMin(start, end);
            edit->setSelection(selStart, qAbs(end - start));
        }
        else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
        {
            const int size = edit->toPlainText().size();
            QTextCursor cursor = edit->textCursor();
            cursor.setPosition(qBound(0, start, size));
            cursor.setPosition(qBound(0, end, size), QTextCursor::KeepAnchor);
            edit->setTextCursor(cursor);
        }
    });
}

// 输入法查询编辑器配置；本回调在调用 Attach 的线程（Qt 主线程）执行
void onGetTextConfig(InputMethod_TextEditorProxy *, InputMethod_TextConfig *config)
{
    if (config == nullptr)
        return;
    QString text;
    int cursor = 0;
    snapshotShadow(text, cursor);
    OH_TextConfig_SetInputType(config, IME_TEXT_INPUT_TYPE_TEXT);
    OH_TextConfig_SetEnterKeyType(config, qobject_cast<QPlainTextEdit *>(g_editor.data())
                                              ? IME_ENTER_KEY_NEWLINE
                                              : IME_ENTER_KEY_DONE);
    // 关闭预览文本：拼音中间态由输入法候选窗展示，不写入编辑器
    OH_TextConfig_SetPreviewTextSupport(config, false);
    OH_TextConfig_SetSelection(config, cursor, cursor);
}

void onGetLeftTextOfCursor(InputMethod_TextEditorProxy *, int32_t number, char16_t text[],
                           size_t *length)
{
    QString source;
    int cursor = 0;
    snapshotShadow(source, cursor);
    cursor = qBound(0, cursor, source.size());
    const int count = qMin<int>(qMax(number, 0), cursor);
    copyTextToBuffer(source, cursor - count, count, text, length);
}

void onGetRightTextOfCursor(InputMethod_TextEditorProxy *, int32_t number, char16_t text[],
                            size_t *length)
{
    QString source;
    int cursor = 0;
    snapshotShadow(source, cursor);
    cursor = qBound(0, cursor, source.size());
    const int count = qMin<int>(qMax(number, 0), source.size() - cursor);
    copyTextToBuffer(source, cursor, count, text, length);
}

int32_t onGetTextIndexAtCursor(InputMethod_TextEditorProxy *)
{
    QString source;
    int cursor = 0;
    snapshotShadow(source, cursor);
    return cursor;
}

int32_t onReceivePrivateCommand(InputMethod_TextEditorProxy *, InputMethod_PrivateCommand **,
                                size_t)
{
    return 0;
}

int32_t onSetPreviewText(InputMethod_TextEditorProxy *, const char16_t[], size_t, int32_t, int32_t)
{
    return 0;
}

void onFinishTextPreview(InputMethod_TextEditorProxy *)
{
}

void onSendKeyboardStatus(InputMethod_TextEditorProxy *, InputMethod_KeyboardStatus)
{
    // 键盘显示/隐藏通知：截图文字工具无需额外处理
}

void onHandleExtendAction(InputMethod_TextEditorProxy *, InputMethod_ExtendAction)
{
}

// C API 要求 15 个回调全部非空，缺一不可（见 IsValidTextEditorProxy）
bool setupCallbacks(InputMethod_TextEditorProxy *proxy)
{
    bool ok = true;
    ok = ok && OH_TextEditorProxy_SetGetTextConfigFunc(proxy, onGetTextConfig) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetInsertTextFunc(proxy, onInsertText) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetDeleteForwardFunc(proxy, onDeleteForward) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetDeleteBackwardFunc(proxy, onDeleteBackward) == IME_ERR_OK;
    ok = ok &&
         OH_TextEditorProxy_SetSendKeyboardStatusFunc(proxy, onSendKeyboardStatus) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetSendEnterKeyFunc(proxy, onSendEnterKey) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetMoveCursorFunc(proxy, onMoveCursor) == IME_ERR_OK;
    ok = ok &&
         OH_TextEditorProxy_SetHandleSetSelectionFunc(proxy, onHandleSetSelection) == IME_ERR_OK;
    ok = ok &&
         OH_TextEditorProxy_SetHandleExtendActionFunc(proxy, onHandleExtendAction) == IME_ERR_OK;
    ok = ok &&
         OH_TextEditorProxy_SetGetLeftTextOfCursorFunc(proxy, onGetLeftTextOfCursor) == IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetGetRightTextOfCursorFunc(proxy, onGetRightTextOfCursor) ==
                   IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetGetTextIndexAtCursorFunc(proxy, onGetTextIndexAtCursor) ==
                   IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetReceivePrivateCommandFunc(proxy, onReceivePrivateCommand) ==
                   IME_ERR_OK;
    ok = ok && OH_TextEditorProxy_SetSetPreviewTextFunc(proxy, onSetPreviewText) == IME_ERR_OK;
    ok =
        ok && OH_TextEditorProxy_SetFinishTextPreviewFunc(proxy, onFinishTextPreview) == IME_ERR_OK;
    if (!ok)
        qWarning().noquote() << (QStringLiteral(
            "HUIWEN_SCREENSHOT_IME: set text editor proxy callbacks failed"));
    return ok;
}

bool ensureClient()
{
    if (g_textProxy == nullptr)
    {
        g_textProxy = OH_TextEditorProxy_Create();
        if (g_textProxy == nullptr || !setupCallbacks(g_textProxy))
            return false;
    }
    if (g_options == nullptr)
    {
        g_options =
            OH_AttachOptions_CreateWithRequestKeyboardReason(true, IME_REQUEST_REASON_MOUSE);
        if (g_options == nullptr)
            return false;
    }
    return true;
}

void unbindEditorSignals()
{
    for (const QMetaObject::Connection &conn : g_conns)
        QObject::disconnect(conn);
    g_conns.clear();
}

void bindEditorSignals(QWidget *widget)
{
    unbindEditorSignals();
    if (auto *editor = qobject_cast<QPlainTextEdit *>(widget))
    {
        const auto sync = [editor]() {
            {
                std::lock_guard<std::mutex> lock(g_shadowMtx);
                g_shadowText = editor->toPlainText();
                g_shadowCursor = editor->textCursor().position();
            }
            HarmonyImeInput::instance().notifyCursor();
        };
        g_conns << QObject::connect(editor, &QPlainTextEdit::textChanged, editor, sync);
        g_conns << QObject::connect(editor, &QPlainTextEdit::cursorPositionChanged, editor, sync);
        g_conns << QObject::connect(editor, &QPlainTextEdit::updateRequest, editor,
                                    []() { HarmonyImeInput::instance().notifyCursor(); });
        sync();
        return;
    }
    auto *editor = qobject_cast<QLineEdit *>(widget);
    g_conns << QObject::connect(editor, &QLineEdit::textChanged, editor, [](const QString &text) {
        std::lock_guard<std::mutex> lock(g_shadowMtx);
        g_shadowText = text;
    });
    g_conns << QObject::connect(editor, &QLineEdit::cursorPositionChanged, editor,
                                [](int, int newPos) {
                                    {
                                        std::lock_guard<std::mutex> lock(g_shadowMtx);
                                        g_shadowCursor = newPos;
                                    }
                                    HarmonyImeInput::instance().notifyCursor();
                                });
}

// attach 前必须先 detach：C API 的 GetInputMethodProxy 在 textEditor 相同时会直接返回成功，
// 不会真正重新注册（见 inputmethod_controller_capi.cpp）
bool attachAndShow()
{
    if (g_imeProxy != nullptr)
    {
        OH_InputMethodController_Detach(g_imeProxy);
        g_imeProxy = nullptr;
    }
    const InputMethod_ErrorCode attachErr =
        OH_InputMethodController_Attach(g_textProxy, g_options, &g_imeProxy);
    if (attachErr != IME_ERR_OK || g_imeProxy == nullptr)
    {
        qWarning().noquote() << (QStringLiteral("HUIWEN_SCREENSHOT_IME: attach failed: %1")
                                     .arg(static_cast<int>(attachErr)));
        return false;
    }
    const InputMethod_ErrorCode showErr = OH_InputMethodProxy_ShowTextInput(g_imeProxy, g_options);
    OH_InputMethodProxy_ShowKeyboard(g_imeProxy);
    if (showErr != IME_ERR_OK)
    {
        qWarning().noquote() << (QStringLiteral("HUIWEN_SCREENSHOT_IME: showTextInput failed: %1")
                                     .arg(static_cast<int>(showErr)));
        return false;
    }
    return true;
}

} // namespace

HarmonyImeInput &HarmonyImeInput::instance()
{
    static HarmonyImeInput s_instance;
    return s_instance;
}

void HarmonyImeInput::setEnterCallback(std::function<void()> cb)
{
    g_enterCb = std::move(cb);
}

bool HarmonyImeInput::isStarted(QWidget *editor) const
{
    return g_started.load() && (!editor || g_editor == editor);
}

bool HarmonyImeInput::start(QLineEdit *editor)
{
    return startEditor(editor);
}

bool HarmonyImeInput::start(QPlainTextEdit *editor)
{
    return startEditor(editor);
}

bool HarmonyImeInput::startEditor(QWidget *editor)
{
    if (editor == nullptr)
        return false;
    stop();
    if (!ensureClient())
        return false;
    g_editor = editor;
    if (auto *lineEdit = qobject_cast<QLineEdit *>(editor))
    {
        std::lock_guard<std::mutex> lock(g_shadowMtx);
        g_shadowText = lineEdit->text();
        g_shadowCursor = lineEdit->cursorPosition();
    }
    bindEditorSignals(editor);
    // 进程内唯一客户端可能刚被 CEF 抢占（showTextInput 返回 12800009），失败后重建一次
    if (!attachAndShow() && !attachAndShow())
    {
        qWarning().noquote() << (QStringLiteral(
            "HUIWEN_SCREENSHOT_IME: input method attach give up"));
        stop();
        return false;
    }
    g_started.store(true);
    notifyCursor();
    qInfo().noquote() << (QStringLiteral("HUIWEN_SCREENSHOT_IME: input method attached"));
    return true;
}

void HarmonyImeInput::stop(QWidget *editor)
{
    if (editor && g_editor != editor)
        return;
    ++g_session;
    g_started.store(false);
    unbindEditorSignals();
    g_enterCb = nullptr;
    g_editor = nullptr;
    if (g_imeProxy != nullptr)
    {
        OH_InputMethodProxy_HideKeyboard(g_imeProxy);
        // 释放进程内唯一客户端，CEF(Chromium) 之后才能重新 attach
        OH_InputMethodController_Detach(g_imeProxy);
        g_imeProxy = nullptr;
    }
}

void HarmonyImeInput::notifyCursor()
{
    if (g_imeProxy == nullptr || !g_editor)
        return;
    QPoint caret;
    int height = 0;
    if (auto *edit = qobject_cast<QLineEdit *>(g_editor.data()))
    {
        const int caretX = edit->fontMetrics().horizontalAdvance(
            edit->text().left(qMax(0, edit->cursorPosition())));
        caret = edit->mapToGlobal(QPoint(caretX, 0));
        height = edit->fontMetrics().height();
    }
    else if (auto *edit = qobject_cast<QPlainTextEdit *>(g_editor.data()))
    {
        const QRect rect = edit->cursorRect();
        caret = edit->viewport()->mapToGlobal(rect.topLeft());
        height = rect.height();
    }
    InputMethod_CursorInfo *info = OH_CursorInfo_Create(caret.x(), caret.y(), 1, height);
    if (info == nullptr)
        return;
    OH_InputMethodProxy_NotifyCursorUpdate(g_imeProxy, info);
    OH_CursorInfo_Destroy(info);
}
