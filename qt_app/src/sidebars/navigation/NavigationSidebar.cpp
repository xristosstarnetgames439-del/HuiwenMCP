/* 左侧栏与 PDF／CEF 分开原生承载；区块内容使用 Qt 树控件，不增加浏览器实例。 */
#include "NavigationSidebar.h"
#include "components/SidebarSection.h"
#include "components/SidebarSectionList.h"
#include "sections/markdownOutline/MarkdownOutlineSection.h"
#include "sections/workspaceFiles/WorkspaceFilesSection.h"
#include <QPalette>
#include <QVBoxLayout>
#include <QWindow>

NavigationSidebar::NavigationSidebar(QWidget *parent)
    : QWidget(parent), files_(new WorkspaceFilesSection(this)),
      outline_(new MarkdownOutlineSection(this))
{
#ifdef Q_OS_OPENHARMONY
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
#endif
    setObjectName("navigationSidebar");
    setMinimumWidth(500);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, Qt::white);
    colors.setColor(QPalette::Base, Qt::white);
    colors.setColor(QPalette::Text, QColor("#333333"));
    colors.setColor(QPalette::WindowText, QColor("#333333"));
    colors.setColor(QPalette::Highlight, QColor("#E5E5E5"));
    colors.setColor(QPalette::HighlightedText, QColor("#333333"));
    setPalette(colors);
    setStyleSheet(QStringLiteral(
        "QWidget#navigationSidebar { background: white; }"
        "QLabel { color: #777777; font-size: 18px; }"
        "QToolButton#navigationSectionHeader { background: #F3F3F3; color: #333333; "
        "border: none; padding: 8px 12px; font-size: 20px; text-align: left; }"
        "QToolButton#navigationSectionHeader:hover { background: #EAEAEA; }"
        "QToolButton#navigationSectionHeader[available=\"false\"] { color: #999999; }"
        "QTreeView { background: white; color: #333333; border: none; font-size: 18px; outline: "
        "none; }"
        "QTreeView::item { padding: 4px 0; }"
        "QTreeView::item:selected { background: #E5E5E5; color: #333333; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *sections = new SidebarSectionList(this);
    sections->addSection(new SidebarSection(QStringLiteral("工作区文件"), files_, sections));
    outlineSection_ = new SidebarSection(QStringLiteral("文档目录"), outline_, sections);
    outlineSection_->setAvailable(false);
    sections->addSection(outlineSection_);
    layout->addWidget(sections);
    connect(files_, &WorkspaceFilesSection::fileActivated, this, &NavigationSidebar::fileActivated);
    connect(outline_, &MarkdownOutlineSection::headingActivated, this,
            &NavigationSidebar::headingActivated);
}

QSize NavigationSidebar::sizeHint() const
{
    return QSize(minimumWidth(), 480);
}

void NavigationSidebar::setDocument(const DocumentItem &document)
{
    files_->setWorkspace(document.taskPath);
    setOutline({}, false);
}

void NavigationSidebar::setOutline(const QVector<MarkdownHeading> &headings, bool available,
                                   bool truncated)
{
    outline_->setHeadings(headings, truncated);
    const bool newlyAvailable = available && !outlineSection_->isAvailable();
    outlineSection_->setAvailable(available);
    if (newlyAvailable)
        outlineSection_->setExpanded(true);
}

void NavigationSidebar::raiseAbovePreview()
{
    if (isHidden())
        return;
    raise();
#ifdef Q_OS_OPENHARMONY
    if (isVisible() && windowHandle())
        windowHandle()->raise();
#endif
}
