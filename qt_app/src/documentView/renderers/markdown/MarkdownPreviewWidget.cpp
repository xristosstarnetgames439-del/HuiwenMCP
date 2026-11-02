/* Markdown 使用共用 CEF 承载，保留现有渲染器入口。 */
#include "MarkdownPreviewWidget.h"

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget *parent)
    : CefDocumentPreviewWidget(Format::Markdown, parent)
{
}
