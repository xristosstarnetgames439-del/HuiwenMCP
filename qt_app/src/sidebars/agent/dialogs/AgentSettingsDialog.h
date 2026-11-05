/* 助手模型配置界面；只收集输入，不发送网络请求、不自行持久化密钥。 */
#pragma once
#include "../AgentTypes.h"
#include <QDialog>
class QLabel;
class QLineEdit;

class AgentSettingsDialog : public QDialog
{
  public:
    explicit AgentSettingsDialog(const AgentModelConfig &config, QWidget *parent = nullptr);
    AgentModelConfig modelConfig() const;

  private:
    QLineEdit *baseUrl_;
    QLineEdit *apiKey_;
    QLineEdit *model_;
    QLabel *error_;
};
