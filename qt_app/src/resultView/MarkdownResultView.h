/* Markdown 输出区域的独立入口：显示结果卡片并发出打开文件位置请求。 */
#pragma once
#include <QWidget>
class FileCard;

class MarkdownResultView : public QWidget
{
    Q_OBJECT
  public:
    explicit MarkdownResultView(QWidget *parent = nullptr);
    void setResultFile(const QString &path);
  signals:
    void locationRequested(const QString &path);

  private:
    FileCard *card_;
    QString path_;
};
