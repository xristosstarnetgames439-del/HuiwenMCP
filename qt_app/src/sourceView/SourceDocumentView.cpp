/* 输入区域实现；当前只显示首个 PDF，入口保留文档集合形式。 */
#include "SourceDocumentView.h"
#include "components/FileCard.h"
#include <QVBoxLayout>

SourceDocumentView::SourceDocumentView(QWidget *parent) : QWidget(parent), card_(new FileCard(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 96);
    layout->addWidget(card_, 0, Qt::AlignCenter);
    setMinimumWidth(260);
    setDocuments({});
    connect(card_, &QPushButton::clicked, this, &SourceDocumentView::selectionRequested);
}

void SourceDocumentView::setDocuments(const QVector<DocumentItem> &documents)
{
    if (documents.isEmpty())
        card_->setPrompt(QStringLiteral("请选择 PDF"), true);
    else
        card_->setFile(documents.first().kind, documents.first().name);
}

void SourceDocumentView::setBusy(bool busy) { card_->setEnabled(!busy); }
