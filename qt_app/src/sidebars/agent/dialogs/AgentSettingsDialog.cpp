/* 模型设置：地址和模型必填；Key 可供无认证本地服务留空，首版仅保存至进程内。 */
#include "AgentSettingsDialog.h"
#include "../components/AgentStyle.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

AgentSettingsDialog::AgentSettingsDialog(const AgentModelConfig &config, QWidget *parent)
    : QDialog(parent), baseUrl_(new QLineEdit(config.baseUrl, this)),
      apiKey_(new QLineEdit(config.apiKey, this)), model_(new QLineEdit(config.model, this)),
      error_(new QLabel(this))
{
    setObjectName("agentDialog");
    setWindowTitle(QStringLiteral("文档助手设置"));
    applyAgentStyle(this);
    resize(460, 330);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);
    auto *form = new QFormLayout;
    form->setSpacing(14);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    baseUrl_->setPlaceholderText(QStringLiteral("https://模型服务地址/v1"));
    baseUrl_->setAccessibleName(QStringLiteral("API 地址"));
    form->addRow(QStringLiteral("API 地址"), baseUrl_);
    auto *keyRow = new QHBoxLayout;
    apiKey_->setEchoMode(QLineEdit::Password);
    apiKey_->setPlaceholderText(QStringLiteral("输入 API Key"));
    apiKey_->setAccessibleName(QStringLiteral("API Key"));
    auto *showKey = new QPushButton(QStringLiteral("显示"), this);
    showKey->setCheckable(true);
    showKey->setAutoDefault(false);
    connect(showKey, &QPushButton::toggled, this, [this, showKey](bool shown) {
        apiKey_->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        showKey->setText(shown ? QStringLiteral("隐藏") : QStringLiteral("显示"));
    });
    keyRow->addWidget(apiKey_, 1);
    keyRow->addWidget(showKey);
    form->addRow(QStringLiteral("API Key"), keyRow);
    model_->setPlaceholderText(QStringLiteral("填写模型服务提供的模型名称"));
    model_->setAccessibleName(QStringLiteral("模型名称"));
    form->addRow(QStringLiteral("模型名称"), model_);
    layout->addLayout(form);
    auto *hint =
        new QLabel(QStringLiteral("API Key 仅在本次应用会话保留，不写入本地配置。\n当前尚未接入 "
                                  "Agent 后端，设置不会发起网络请求。"),
                   this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #777777; font-size: 16px;");
    layout->addWidget(hint);
    error_->setWordWrap(true);
    error_->setStyleSheet("color: #B3261E; font-size: 18px;");
    layout->addWidget(error_);
    auto *actions = new QHBoxLayout;
    auto *test = new QPushButton(QStringLiteral("测试连接"), this);
    test->setEnabled(false); // TODO：真实模型后端接入后启用，首版不模拟连接成功。
    actions->addWidget(test);
    actions->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Save)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QUrl url(baseUrl_->text().trimmed(), QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme() != "http" && url.scheme() != "https") || !url.userInfo().isEmpty() ||
            url.hasQuery() || url.hasFragment())
        {
            error_->setText(
                QStringLiteral("请填写有效的 HTTP／HTTPS 服务地址，密钥请单独填入 API Key。"));
            return;
        }
        if (model_->text().trimmed().isEmpty())
        {
            error_->setText(QStringLiteral("请填写模型名称"));
            return;
        }
        accept();
    });
    actions->addWidget(buttons);
    layout->addLayout(actions);
}

AgentModelConfig AgentSettingsDialog::modelConfig() const
{
    return {baseUrl_->text().trimmed(), apiKey_->text().trimmed(), model_->text().trimmed()};
}
