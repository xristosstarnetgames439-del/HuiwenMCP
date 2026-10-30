/* 文档渲染契约：与 PDF、Markdown、Word 及输入输出角色无关，不新增可视容器。 */
#pragma once
#include <QString>

enum class DocumentScrollOwner
{
    View,
    Backend
};

struct DocumentViewOptions
{
    // 只控制 Qt 侧追加的惯性；Backend 模式保留浏览器或文档引擎自己的手势处理。
    bool inertiaEnabled = true;
    bool scrollBarVisible = true;
};

class DocumentRenderer
{
  public:
    virtual ~DocumentRenderer() = default;
    virtual void setDocument(const QString &path) = 0;
    virtual bool isPreviewReady() const = 0;
    virtual void setScrollPosition(int page, double fraction, double progress) = 0;
    virtual void publishScrollPosition() = 0;
    virtual DocumentScrollOwner scrollOwner() const = 0;
    virtual void setViewOptions(const DocumentViewOptions &options) = 0;
    // 实现类同时是 QWidget，并提供 documentOpened、previewReadyChanged、scrollPositionChanged
    // 信号。
};
