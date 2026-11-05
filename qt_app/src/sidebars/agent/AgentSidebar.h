/* 文档助手侧栏：组装界面与本地会话，不创建模型进程，也不依赖任何 Agent 框架。 */
#pragma once
#include "AgentTypes.h"
#include "sandbox/DocumentItem.h"
#include <QWidget>
class AgentComposer;
class AgentMessageView;

class AgentSidebar : public QWidget
{
    Q_OBJECT
  public:
    explicit AgentSidebar(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void setDocument(const DocumentItem &document);
    void raiseAbovePreview();
  signals:
    // 后端接入点：会话自带关联工作区和消息，界面不传密钥，也不触碰文档渲染器。
    void sendRequested(const AgentConversation &conversation);

  private:
    void newConversation(bool clearDraft = true);
    void openHistory();
    void openSettings();
    void submit(const QString &text);
    AgentMessageView *messages_;
    AgentComposer *composer_;
    AgentConversation conversation_;
    DocumentItem document_;
};
