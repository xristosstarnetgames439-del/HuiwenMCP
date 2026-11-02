/* DOCX 只读预览，使用 PortOne 离线页面及共用 CEF 承载。 */
#pragma once
#include "documentView/renderers/web/CefDocumentPreviewWidget.h"

class WordPreviewWidget : public CefDocumentPreviewWidget
{
  public:
    explicit WordPreviewWidget(QWidget *parent = nullptr)
        : CefDocumentPreviewWidget(Format::Word, parent)
    {
    }
};
