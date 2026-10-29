/* 文档竖向滚动条：仅封装外观，位移、惯性与同步由使用方负责。 */
#pragma once
#include <QScrollBar>

class DocumentScrollBar : public QScrollBar
{
  public:
    explicit DocumentScrollBar(QWidget *parent = nullptr);
};
