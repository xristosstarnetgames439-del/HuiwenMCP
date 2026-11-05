/* 使用 Qt 文本控件展示聊天，不另建 CEF 实例；用户文本始终按纯文本转义后渲染。 */
#include "AgentMessageView.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

AgentMessageView::AgentMessageView(QWidget *parent)
    : QWidget(parent), searchBar_(new QWidget(this)), search_(new QLineEdit(searchBar_)),
      searchStatus_(new QLabel(searchBar_)), pages_(new QStackedWidget(this)),
      messages_(new QTextBrowser(pages_))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    auto *searchLayout = new QHBoxLayout(searchBar_);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    search_->setPlaceholderText(QStringLiteral("搜索当前对话"));
    search_->setClearButtonEnabled(true);
    searchLayout->addWidget(search_, 1);
    searchLayout->addWidget(searchStatus_);
    const auto addSearchButton = [this, searchLayout](const QString &text, const QString &label) {
        auto *button = new QToolButton(searchBar_);
        button->setText(text);
        button->setFixedSize(26, 30);
        button->setAccessibleName(label);
        searchLayout->addWidget(button);
        return button;
    };
    connect(addSearchButton(QStringLiteral("↑"), QStringLiteral("上一个结果")),
            &QToolButton::clicked, this, [this] { findMessage(true); });
    connect(addSearchButton(QStringLiteral("↓"), QStringLiteral("下一个结果")),
            &QToolButton::clicked, this, [this] { findMessage(false); });
    connect(addSearchButton(QStringLiteral("×"), QStringLiteral("关闭搜索")), &QToolButton::clicked,
            searchBar_, &QWidget::hide);
    connect(search_, &QLineEdit::textChanged, this, [this] { findMessage(false, true); });
    connect(search_, &QLineEdit::returnPressed, this, [this] { findMessage(false); });
    layout->addWidget(searchBar_);
    searchBar_->hide();

    auto *welcome = new QWidget(pages_);
    auto *welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->setContentsMargins(12, 0, 12, 0);
    auto *logo = new QLabel(welcome);
    logo->setPixmap(QPixmap(QStringLiteral(":/branding/app_icon.png"))
                        .scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    auto *greeting = new QLabel(QStringLiteral("你好，有什么可以帮你？"), welcome);
    greeting->setAlignment(Qt::AlignCenter);
    greeting->setWordWrap(true);
    greeting->setStyleSheet("font-size: 28px; color: #333333;");
    welcomeLayout->addStretch();
    welcomeLayout->addWidget(logo);
    welcomeLayout->addSpacing(24);
    welcomeLayout->addWidget(greeting);
    welcomeLayout->addStretch();
    pages_->addWidget(welcome);
    messages_->setFrameShape(QFrame::NoFrame);
    messages_->setOpenLinks(false);
    messages_->setOpenExternalLinks(false);
    pages_->addWidget(messages_);
    layout->addWidget(pages_, 1);
}

void AgentMessageView::setMessages(const QVector<AgentMessage> &messages)
{
    QString html = QStringLiteral("<html><body style='color:#333333; font-size:18px;'>");
    for (const auto &message : messages)
    {
        const QString label = message.role == "user"        ? QStringLiteral("你")
                              : message.role == "assistant" ? QStringLiteral("文档助手")
                                                            : QStringLiteral("提示");
        QString text = message.text.toHtmlEscaped();
        text.replace('\n', QStringLiteral("<br>"));
        html += QStringLiteral("<p style='color:#777777;'>%1</p>"
                               "<table width='100%' cellspacing='0' cellpadding='12'><tr>"
                               "<td bgcolor='%2'>%3</td></tr></table><p></p>")
                    .arg(label, message.role == "user" ? "#F5F5F5" : "#FFFFFF", text);
    }
    messages_->setHtml(html + QStringLiteral("</body></html>"));
    pages_->setCurrentIndex(messages.isEmpty() ? 0 : 1);
    searchStatus_->clear();
    QTimer::singleShot(0, this, [this] {
        messages_->verticalScrollBar()->setValue(messages_->verticalScrollBar()->maximum());
    });
}

void AgentMessageView::showSearch()
{
    searchBar_->show();
    search_->setFocus(Qt::ShortcutFocusReason);
    search_->selectAll();
}

void AgentMessageView::findMessage(bool backward, bool fromStart)
{
    if (search_->text().isEmpty())
    {
        auto cursor = messages_->textCursor();
        cursor.clearSelection();
        messages_->setTextCursor(cursor);
        searchStatus_->clear();
        return;
    }
    if (fromStart)
        messages_->moveCursor(QTextCursor::Start);
    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    bool found = messages_->find(search_->text(), flags);
    if (!found)
    {
        messages_->moveCursor(backward ? QTextCursor::End : QTextCursor::Start);
        found = messages_->find(search_->text(), flags);
    }
    searchStatus_->setText(found ? QString() : QStringLiteral("无结果"));
}
