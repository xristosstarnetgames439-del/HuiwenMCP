/* Markdown 只读预览，浏览器行为由共用 CEF 承载维护。 */
#pragma once
#include "documentView/renderers/web/CefDocumentPreviewWidget.h"

class MarkdownPreviewWidget : public CefDocumentPreviewWidget
{
    Q_OBJECT
  public:
    explicit MarkdownPreviewWidget(QWidget *parent = nullptr);
};
