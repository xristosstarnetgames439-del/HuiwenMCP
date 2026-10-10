/* 通用文件卡片：绘制文件占位图与名称，点击行为由所属区域接入。 */
#pragma once
#include <QPushButton>

class FileCard : public QPushButton
{
  public:
    explicit FileCard(QWidget *parent = nullptr);
    void setFile(const QString &kind, const QString &name);
    void setPrompt(const QString &prompt, bool selectable);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QString kind_;
    QString name_;
    QString prompt_;
};
