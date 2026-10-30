/* 预览与转化共用文件菜单；仅当前业务模式提供更换文件，模式切换继续保留 TODO。 */
#include "FileMenu.h"
#include <QAction>
#include <QActionGroup>

FileMenu::FileMenu(const QString &mode, QWidget *parent)
    : SelectionMenu(parent), conversion_(mode == "convert")
{
    auto *viewMenu = new SelectionMenu(this);
    viewMenu->setTitle(QStringLiteral("查看模式"));
    addMenu(viewMenu);
    auto *views = new QActionGroup(viewMenu);
    views->setExclusive(true);
    placeholderAction_ = viewMenu->addAction(QStringLiteral("占位图模式"));
    previewAction_ = viewMenu->addAction(QStringLiteral("预览模式"));
    auto *editView = viewMenu->addAction(QStringLiteral("编辑模式"));
    for (auto *action : {placeholderAction_, previewAction_, editView})
    {
        action->setCheckable(true);
        views->addAction(action);
    }
    previewAction_->setChecked(true);
    // TODO: 接入编辑视图；当前仅提供占位图和只读预览。
    editView->setEnabled(false);
    editView->setToolTip(QStringLiteral("编辑视图尚未接入"));
    connect(placeholderAction_, &QAction::triggered, this, [this] { setPreviewMode(false); });
    connect(previewAction_, &QAction::triggered, this, [this] { setPreviewMode(true); });

    auto *directoryMenu = new SelectionMenu(this);
    directoryMenu->setTitle(QStringLiteral("文件目录"));
    addMenu(directoryMenu);
    inputDirectoryAction_ = directoryMenu->addAction(conversion_ ? QStringLiteral("输入目录")
                                                                 : QStringLiteral("所在目录"));
    outputDirectoryAction_ = directoryMenu->addAction(QStringLiteral("输出目录"));
    outputDirectoryAction_->setVisible(conversion_);
    connect(inputDirectoryAction_, &QAction::triggered, this,
            [this] { emit directoryRequested(false); });
    connect(outputDirectoryAction_, &QAction::triggered, this,
            [this] { emit directoryRequested(true); });

    auto *modeMenu = new SelectionMenu(this);
    modeMenu->setTitle(QStringLiteral("切换模式"));
    addMenu(modeMenu);
    auto *modes = new QActionGroup(modeMenu);
    modes->setExclusive(true);
    auto *currentMenu = new SelectionMenu(modeMenu);
    currentMenu->setTitle(conversion_ ? QStringLiteral("转化模式") : QStringLiteral("预览模式"));
    for (const QString &name :
         {QStringLiteral("预览模式"), QStringLiteral("编辑模式"), QStringLiteral("转化模式")})
    {
        const bool current = name == currentMenu->title();
        auto *action = current ? modeMenu->addMenu(currentMenu) : modeMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(current);
        // TODO: 后续接入业务模式切换，当前只由 ArkTS 启动参数决定模式。
        action->setEnabled(current);
        if (!current)
            action->setToolTip(QStringLiteral("业务模式切换尚未接入"));
        modes->addAction(action);
    }
    replacementAction_ = currentMenu->addAction(QStringLiteral("更换文件"));
    connect(replacementAction_, &QAction::triggered, this, &FileMenu::replacementRequested);
    setDocumentState(false, false);
}

// 菜单点击与程序切换共用入口，只改变查看方式，不改变业务模式。
void FileMenu::setPreviewMode(bool preview)
{
    (preview ? previewAction_ : placeholderAction_)->setChecked(true);
    emit previewModeRequested(preview);
}

void FileMenu::setDocumentState(bool hasDocument, bool busy)
{
    placeholderAction_->setEnabled(!busy);
    previewAction_->setEnabled(hasDocument && !busy);
    inputDirectoryAction_->setEnabled(hasDocument);
    outputDirectoryAction_->setEnabled(conversion_ && hasDocument);
    replacementAction_->setEnabled(!busy);
}
