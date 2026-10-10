/* 汇文工作区文件模型：记录格式、显示名称和现有沙箱输入/输出路径。 */
#pragma once
#include <QString>

struct DocumentItem
{
    QString kind;
    QString name;
    QString inputPath;
    QString outputPath;
};
