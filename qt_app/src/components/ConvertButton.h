/* 独立转化按钮：只提供交互与忙碌状态，转换进程由服务管理。 */
#pragma once
#include <QPushButton>

class ConvertButton : public QPushButton
{
  public:
    explicit ConvertButton(QWidget *parent = nullptr);
    void setState(bool hasDocument, bool busy);
};
