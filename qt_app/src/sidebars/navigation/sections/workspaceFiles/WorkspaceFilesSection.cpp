/* 复用 Qt 文件系统模型的异步加载与目录监听；过滤兄弟工作区及符号链接。 */
#include "WorkspaceFilesSection.h"
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>

class WorkspaceFileFilter : public QSortFilterProxyModel
{
  public:
    explicit WorkspaceFileFilter(QObject *parent) : QSortFilterProxyModel(parent)
    {
    }
    void setWorkspace(const QString &root)
    {
        root_ = root;
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (root_.isEmpty())
            return false;
        const auto *files = static_cast<const QFileSystemModel *>(sourceModel());
        const QString path = files->filePath(files->index(row, 0, parent));
        const QString prefix = path.endsWith('/') ? path : path + '/';
        // 保留定位根索引必需的祖先，但树视图只从当前任务的父目录开始展示。
        return path == root_ || path.startsWith(root_ + '/') || root_.startsWith(prefix);
    }

  private:
    QString root_;
};

WorkspaceFilesSection::WorkspaceFilesSection(QWidget *parent)
    : QWidget(parent), files_(new QFileSystemModel(this)), filter_(new WorkspaceFileFilter(this)),
      tree_(new QTreeView(this)), hint_(new QLabel(QStringLiteral("请选择或打开工作区"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    files_->setReadOnly(true);
    files_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    filter_->setSourceModel(files_);
    tree_->setModel(filter_);
    tree_->setHeaderHidden(true);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setTextElideMode(Qt::ElideMiddle);
    tree_->setUniformRowHeights(true);
    tree_->setIndentation(20);
    tree_->setAccessibleName(QStringLiteral("当前工作区文件树"));
    for (int column = 1; column < files_->columnCount(); ++column)
        tree_->hideColumn(column);
    tree_->header()->setStretchLastSection(true);
    layout->addWidget(hint_);
    layout->addWidget(tree_, 1);
    connect(files_, &QFileSystemModel::directoryLoaded, this, [this](const QString &path) {
        if (path == root_)
            tree_->expand(filter_->mapFromSource(files_->index(root_)));
    });
    connect(tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto info = files_->fileInfo(filter_->mapToSource(index));
        const QString path = info.canonicalFilePath();
        if (!root_.isEmpty() && info.isFile() && !info.isSymLink() && path.startsWith(root_ + '/'))
            emit fileActivated(path);
    });
    setWorkspace({});
}

void WorkspaceFilesSection::setWorkspace(const QString &taskPath)
{
    const QFileInfo info(taskPath);
    const QString root = !taskPath.isEmpty() && info.isDir() && !info.isSymLink()
                             ? info.canonicalFilePath()
                             : QString();
    if (!root.isEmpty() && root == root_)
        return;
    root_ = root;
    filter_->setWorkspace(root_);
    hint_->setVisible(root_.isEmpty());
    tree_->setVisible(!root_.isEmpty());
    if (root_.isEmpty())
        return;
    files_->setRootPath(root_);
    tree_->setRootIndex(filter_->mapFromSource(files_->index(QFileInfo(root_).absolutePath())));
    tree_->expand(filter_->mapFromSource(files_->index(root_)));
}
