/* 助手输入区：显示当前工作区和配置模型，发送请求不直接访问后端。 */
#pragma once
#include <QWidget>
class QLabel;
class QPlainTextEdit;
class QToolButton;

class AgentComposer : public QWidget
{
    Q_OBJECT
  public:
    explicit AgentComposer(QWidget *parent = nullptr);
    void setWorkspace(const QString &name);
    void setModel(const QString &model);
    void setStatus(const QString &text);
    void clearInput();
  signals:
    void submitted(const QString &text);
    void settingsRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void submit();
    void refreshLabels();
    QString workspaceName_;
    QString modelName_;
    QLabel *workspace_;
    QLabel *status_;
    QPlainTextEdit *input_;
    QToolButton *model_;
    QToolButton *send_;
    bool composing_ = false;
};
