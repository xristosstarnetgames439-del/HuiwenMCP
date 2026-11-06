/* Markdown 标题数据契约：级别 1～6、纯文本标题及从 1 开始的源行号。 */
#pragma once
#include <QString>

struct MarkdownHeading
{
    QString title;
    int level = 1;
    int line = 1;
};
