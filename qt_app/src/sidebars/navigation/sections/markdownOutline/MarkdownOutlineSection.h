/* 标题树只消费解析结果，不读取文件，也不直接调用浏览器。 */
#pragma once
#include "MarkdownHeading.h"
#include <QVector>
#include <QWidget>
class QLabel;
class QTreeWidget;

class MarkdownOutlineSection : public QWidget
{
    Q_OBJECT
  public:
    explicit MarkdownOutlineSection(QWidget *parent = nullptr);
    void setHeadings(const QVector<MarkdownHeading> &headings, bool truncated = false);

  signals:
    void headingActivated(int line);

  private:
    QTreeWidget *tree_;
    QLabel *hint_;
    QVector<MarkdownHeading> headings_;
    bool truncated_ = false;
};
