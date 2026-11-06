/* 左侧导航入口：组装有序区块，工作区文件树与 Markdown 目录互不调用。 */
#pragma once
#include "sandbox/DocumentItem.h"
#include "sections/markdownOutline/MarkdownHeading.h"
#include <QVector>
#include <QWidget>
class SidebarSection;
class WorkspaceFilesSection;
class MarkdownOutlineSection;

class NavigationSidebar : public QWidget
{
    Q_OBJECT
  public:
    explicit NavigationSidebar(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void setDocument(const DocumentItem &document);
    void setOutline(const QVector<MarkdownHeading> &headings, bool available,
                    bool truncated = false);
    void raiseAbovePreview();

  signals:
    void fileActivated(const QString &path);
    void headingActivated(int line);

  private:
    WorkspaceFilesSection *files_;
    MarkdownOutlineSection *outline_;
    SidebarSection *outlineSection_;
};
