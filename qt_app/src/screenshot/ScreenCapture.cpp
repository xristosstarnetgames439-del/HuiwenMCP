/* 汇文截图入口与全屏覆盖层：框选、标注、保存 PNG 和系统剪贴板。 */
#include "ScreenCapture.h"
#include "CaptureAmplifier.h"
#include "CaptureToolbar.h"
#include "HarmonyImeInput.h"
#include <QDebug>

#ifdef Q_OS_OPENHARMONY
// 汇文 CEF 预览的 Ctrl+V 读取鸿蒙系统剪贴板；Qt setPixmap 不保证图片同步过去。
// 这里把截图以 file:// URI + pixelmap 两条记录同时写入系统 pasteboard，
// 保证所有 web 窗口 Chromium Ctrl+V 能拿到图片。
#include "database/pasteboard/oh_pasteboard.h"
#include "database/udmf/udmf.h"
#include "database/udmf/udmf_meta.h"
#include "database/udmf/uds.h"
#include "filemanagement/file_uri/oh_file_uri.h"
#include <multimodalinput/oh_input_manager.h>
#include <window_manager/oh_display_manager.h>
#endif

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLineF>
#include <QList>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPair>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QRegion>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cmath>
#include <cstdlib>

namespace
{

#ifdef Q_OS_OPENHARMONY
// OHOS：把 PNG 文件路径转 file:// URI 写入 data。返回 true 表示成功添加 record。
static bool addPngFileUriRecord(OH_UdmfData *data, const QString &pngPath)
{
    const QByteArray pathBytes = pngPath.toUtf8();
    char *uriResult = nullptr;
    if (OH_FileUri_GetUriFromPath(pathBytes.constData(),
                                  static_cast<unsigned int>(pathBytes.size()), &uriResult) != 0 ||
        !uriResult)
    {
        return false;
    }
    OH_UdsFileUri *udsFileUri = OH_UdsFileUri_Create();
    OH_UdmfRecord *recFile = (udsFileUri && OH_UdsFileUri_SetFileUri(udsFileUri, uriResult) == 0 &&
                              OH_UdsFileUri_SetFileType(udsFileUri, "image/png") == 0)
                                 ? OH_UdmfRecord_Create()
                                 : nullptr;
    const bool added = recFile && OH_UdmfRecord_AddFileUri(recFile, udsFileUri) == 0 &&
                       OH_UdmfData_AddRecord(data, recFile) == 0;
    std::free(uriResult);
    if (udsFileUri)
        OH_UdsFileUri_Destroy(udsFileUri);
    if (recFile)
        OH_UdmfRecord_Destroy(recFile);
    return added;
}

// OHOS：把 QImage 转 pixelmap 写入 data。返回 true 表示成功添加 record。
static bool addPngPixelmapRecord(OH_UdmfData *data, const QImage &image)
{
    OH_Pixelmap_InitializationOptions *options = nullptr;
    if (OH_PixelmapInitializationOptions_Create(&options) != IMAGE_SUCCESS || !options)
    {
        return false;
    }
    OH_PixelmapInitializationOptions_SetWidth(options, image.width());
    OH_PixelmapInitializationOptions_SetHeight(options, image.height());
    OH_PixelmapInitializationOptions_SetPixelFormat(options, PIXEL_FORMAT_RGBA_8888);
    OH_PixelmapNative *pixelmap = nullptr;
    if (OH_PixelmapNative_CreatePixelmap(const_cast<uint8_t *>(image.constBits()),
                                         static_cast<size_t>(image.sizeInBytes()), options,
                                         &pixelmap) != IMAGE_SUCCESS ||
        !pixelmap)
    {
        OH_PixelmapInitializationOptions_Release(options);
        return false;
    }
    OH_UdsPixelMap *udsPixel = OH_UdsPixelMap_Create();
    OH_UdmfRecord *recPixel = (udsPixel && OH_UdsPixelMap_SetPixelMap(udsPixel, pixelmap) == 0)
                                  ? OH_UdmfRecord_Create()
                                  : nullptr;
    const bool added = recPixel && OH_UdmfRecord_AddPixelMap(recPixel, udsPixel) == 0 &&
                       OH_UdmfData_AddRecord(data, recPixel) == 0;
    if (udsPixel)
        OH_UdsPixelMap_Destroy(udsPixel);
    if (recPixel)
        OH_UdmfRecord_Destroy(recPixel);
    OH_PixelmapNative_Release(pixelmap);
    OH_PixelmapInitializationOptions_Release(options);
    return added;
}

// 把截图以 file:// URI + pixelmap 两条记录写入 OHOS 系统 pasteboard：
//   - file URI (UDMF_META_GENERAL_FILE_URI): web 窗口 Chromium 粘贴识别。
//   - pixelmap (UDMF_META_OPENHARMONY_PIXEL_MAP): OHOS 原生应用识别。
// pathOrEmpty 为空时只写 pixelmap (saveAndFinish 时序下)。
bool writeScreenshotToSystemPasteboard(const QPixmap &pixmap, const QString &pathOrEmpty)
{
    if (pixmap.isNull())
        return false;
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
        return false;

    OH_UdmfData *data = OH_UdmfData_Create();
    if (!data)
        return false;

    const bool okFileUri = !pathOrEmpty.isEmpty() && QFile::exists(pathOrEmpty) &&
                           addPngFileUriRecord(data, pathOrEmpty);
    const bool okPixelmap = addPngPixelmapRecord(data, image);
    if (!okFileUri && !okPixelmap)
    {
        OH_UdmfData_Destroy(data);
        return false;
    }

    OH_Pasteboard *pasteboard = OH_Pasteboard_Create();
    if (!pasteboard)
    {
        OH_UdmfData_Destroy(data);
        return false;
    }
    OH_Pasteboard_SetData(pasteboard, data);
    OH_Pasteboard_Destroy(pasteboard);
    OH_UdmfData_Destroy(data);
    return okFileUri;
}
#endif

using ScreenSnapshots = QList<QPair<QPointer<QScreen>, QPixmap>>;

class CaptureOverlay final : public QWidget
{
  public:
    enum class Tool
    {
        None,
        Rectangle,
        Ellipse,
        Arrow,
        Text
    };
    // 选区边框/角落拖拽方向
    enum class ResizeDir
    {
        None,
        Left,
        Right,
        Top,
        Bottom,
        LeftTop,
        RightTop,
        LeftBottom,
        RightBottom
    };

    static constexpr int kResizePadding = 6; // 边框命中宽度

    // 根据鼠标位置判定命中的选区边框/角落方向
    ResizeDir getResizeDir(const QPoint &pos) const
    {
        const QRect sel = selection_.normalized();
        const bool nearLeft = pos.x() >= sel.left() && pos.x() <= sel.left() + kResizePadding;
        const bool nearRight = pos.x() <= sel.right() && pos.x() >= sel.right() - kResizePadding;
        const bool nearTop = pos.y() >= sel.top() && pos.y() <= sel.top() + kResizePadding;
        const bool nearBottom = pos.y() <= sel.bottom() && pos.y() >= sel.bottom() - kResizePadding;
        if (nearLeft && nearTop)
            return ResizeDir::LeftTop;
        if (nearRight && nearBottom)
            return ResizeDir::RightBottom;
        if (nearLeft && nearBottom)
            return ResizeDir::LeftBottom;
        if (nearRight && nearTop)
            return ResizeDir::RightTop;
        if (nearLeft)
            return ResizeDir::Left;
        if (nearRight)
            return ResizeDir::Right;
        if (nearTop)
            return ResizeDir::Top;
        if (nearBottom)
            return ResizeDir::Bottom;
        return ResizeDir::None;
    }

    static Qt::CursorShape ResizeCursor(ResizeDir dir)
    {
        switch (dir)
        {
        case ResizeDir::LeftTop:
        case ResizeDir::RightBottom:
            return Qt::SizeFDiagCursor;
        case ResizeDir::LeftBottom:
        case ResizeDir::RightTop:
            return Qt::SizeBDiagCursor;
        case ResizeDir::Left:
        case ResizeDir::Right:
            return Qt::SizeHorCursor;
        case ResizeDir::Top:
        case ResizeDir::Bottom:
            return Qt::SizeVerCursor;
        default:
            return Qt::ArrowCursor;
        }
    }

    // 按拖拽方向调整选区大小（保持拖拽锚点）
    void applyResize(const QPoint &pos)
    {
        const qint64 started = resizeTrace_.clock.nsecsElapsed();
        if (resizeTrace_.input.count)
            resizeTrace_.maxInputGapNs =
                qMax(resizeTrace_.maxInputGapNs, started - resizeTrace_.lastInputNs);
        resizeTrace_.lastInputNs = started;
        const QRect before = selection_.normalized().intersected(rect());
        QRect sel = resizeOrigin_;
        switch (resizeDir_)
        {
        case ResizeDir::Left:
            sel.setLeft(pos.x());
            break;
        case ResizeDir::Right:
            sel.setRight(pos.x());
            break;
        case ResizeDir::Top:
            sel.setTop(pos.y());
            break;
        case ResizeDir::Bottom:
            sel.setBottom(pos.y());
            break;
        case ResizeDir::LeftTop:
            sel.setLeft(pos.x());
            sel.setTop(pos.y());
            break;
        case ResizeDir::LeftBottom:
            sel.setLeft(pos.x());
            sel.setBottom(pos.y());
            break;
        case ResizeDir::RightTop:
            sel.setRight(pos.x());
            sel.setTop(pos.y());
            break;
        case ResizeDir::RightBottom:
            sel.setRight(pos.x());
            sel.setBottom(pos.y());
            break;
        default:
            return;
        }
        selection_ = sel.normalized().intersected(rect());
        const qint64 toolbarStarted = resizeTrace_.clock.nsecsElapsed();
        positionToolbar();
        resizeTrace_.toolbar.add(resizeTrace_.clock.nsecsElapsed() - toolbarStarted);
        // 只重绘明暗发生变化的区域和新旧边框，保留两像素余量清除旧描边。
        QRegion dirty = QRegion(before).xored(QRegion(selection_));
        for (const QRect &edge : {before, selection_})
            dirty += QRegion(edge.adjusted(-2, -2, 2, 2))
                         .subtracted(QRegion(edge.adjusted(2, 2, -2, -2)));
        update(dirty.intersected(QRegion(rect())));
        resizeTrace_.input.add(resizeTrace_.clock.nsecsElapsed() - started);
    }

    // 自动框选目标矩形（如鼠标所在的活动窗口），工具栏延迟到单击左键确认后显示
    void initDefaultSelection(const QRect &target)
    {
        // target 是虚拟桌面坐标；扩展屏原点不一定为 (0, 0)。
        selection_ =
            QRect(mapFromGlobal(target.topLeft()), target.size()).normalized().intersected(rect());
        selectionReady_ = true;
        toolbar_->hide();
        // 进入"自动框选阶段"：后续 mouseMoveEvent 会根据鼠标位置动态切换选区
        autoSelectPhase_ = true;
        // 将 mainWindowRect_ 和 availableRect_（均为屏幕坐标）转换到 overlay 本地坐标系，
        // 使后续 mouseMoveEvent 里 event->pos()（本地坐标）可直接做 contains 判断
        if (!mainWindowRect_.isEmpty())
        {
            mainWindowRect_ = QRect(mapFromGlobal(mainWindowRect_.topLeft()),
                                    mapFromGlobal(mainWindowRect_.bottomRight()));
        }
        if (!availableRect_.isEmpty())
        {
            availableRect_ = QRect(mapFromGlobal(availableRect_.topLeft()),
                                   mapFromGlobal(availableRect_.bottomRight()));
        }
        update();
    }

    struct Mark
    {
        Tool tool = Tool::None;
        QPoint start;
        QPoint end;
        QString text;
        int color = 9;
        int font = 3;
    };

    CaptureOverlay(ScreenSnapshots snapshots, QWidget *hiddenWindow,
                   ScreenCapture::FinishCallback callback)
        : snapshots_(std::move(snapshots)), hiddenWindow_(hiddenWindow),
          callback_(std::move(callback))
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setWindowTitle(QStringLiteral("截图"));
        // 非焦点快捷键先用本次截图窗口取得焦点，查询系统鼠标前不绘制任何内容。
        setAttribute(Qt::WA_TranslucentBackground);
        // QPA 会读取窗口画刷作为原生节点底色，准备阶段也必须保持透明。
        QPalette transparentPalette = palette();
        transparentPalette.setColor(QPalette::Window, Qt::transparent);
        setPalette(transparentPalette);
        setCursor(Qt::CrossCursor);
        // 开启鼠标跟踪：不按鼠标移动也触发 MouseMove，选区外悬停即实时切换指针光标
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        // overlay 必须在截图结束后销毁（close + WA_DeleteOnClose）。
        // 鸿蒙侧 overlay 是一个独立 UIAbility 实例；隐藏它的同时它的 Ability 会进入后台，
        // 而 QOhPlatformAbilityCtrl::startAbility 取的是"窗口列表里最后一个窗口"的 Ability
        // context，若 overlay 常驻复用，之后所有 startAbility（系统图片查看器/文件管理器）
        // 都会以这个后台 Ability 发出，被系统拒绝并返回 201，导致图片与文件夹再也打不开。
        setAttribute(Qt::WA_DeleteOnClose);
        createToolbar();
        createTextEditor();
        // 记录截图启动时的主窗口矩形（屏幕坐标）。
        // hiddenWindow 是 ignoreActiveWindow=true 时被 hide 的主窗口，
        // 此时仍可通过 geometry() 拿到截图前的位置；用于"鼠标在主窗口内 → 框选主窗口"判断。
        // hiddenWindow 为空时（captureIgnoreWindow=false，主窗口未隐藏），回退到 activeWindow。
        QWidget *initialWindow = hiddenWindow ? hiddenWindow : QApplication::activeWindow();
        if (initialWindow)
            mainWindowRect_ = initialWindow->geometry();
    }

    // 返回 false 仅表示还在异步等待原生窗口/焦点；计时器随本窗口销毁。
    bool startOnPointerScreen(bool timedOut)
    {
        if (finished_ || !screen_.isNull())
            return true;
        if (timedOut)
        {
            qWarning().noquote() << (QStringLiteral(
                "HUIWEN_SCREENSHOT: waiting for native pointer/focus timed out"));
            finish(false, QString(), QStringLiteral("截图窗口未取得系统焦点，请重试。"));
            return true;
        }
        if (!windowHandle() || !windowHandle()->isExposed())
            return false;

        QPoint cursorPos;
#ifdef Q_OS_OPENHARMONY
        int32_t displayId = -1;
        double displayX = 0, displayY = 0;
        const auto result = OH_Input_GetPointerLocation(&displayId, &displayX, &displayY);
        if (result == INPUT_APP_NOT_FOCUSED)
            return false;
        int32_t originX = 0, originY = 0;
        const auto positionResult =
            result == INPUT_SUCCESS
                ? OH_NativeDisplayManager_GetDisplayPosition(displayId, &originX, &originY)
                : DISPLAY_MANAGER_ERROR_SYSTEM_ABNORMAL;
        qInfo().noquote() << (QStringLiteral("HUIWEN_SCREENSHOT: pointer ret=%1 display=%2 "
                                             "local=%3,%4 origin=%5,%6 positionRet=%7")
                                  .arg(result)
                                  .arg(displayId)
                                  .arg(displayX)
                                  .arg(displayY)
                                  .arg(originX)
                                  .arg(originY)
                                  .arg(positionResult));
        if (result != INPUT_SUCCESS || positionResult != DISPLAY_MANAGER_OK)
        {
            finish(false, QString(), QStringLiteral("无法获取系统鼠标所在屏幕，请重试。"));
            return true;
        }
        // 系统返回屏内像素坐标，必须加上扩展屏原点后才能用于 Qt 虚拟桌面选屏。
        cursorPos = QPoint(originX + qRound(displayX), originY + qRound(displayY));
#else
        cursorPos = QCursor::pos();
#endif
        QPointer<QScreen> screen = QGuiApplication::screenAt(cursorPos);
        if (!screen)
        {
            finish(false, QString(), QStringLiteral("未找到鼠标所在的截图屏幕。"));
            return true;
        }
        const QRect screenGeometry = screen->geometry();
        qInfo().noquote()
            << (QStringLiteral(
                    "HUIWEN_SCREENSHOT: screen=%1 origin=%2,%3 size=%4x%5 cursor=%6,%7 screens=%8")
                    .arg(screen->name())
                    .arg(screenGeometry.x())
                    .arg(screenGeometry.y())
                    .arg(screenGeometry.width())
                    .arg(screenGeometry.height())
                    .arg(cursorPos.x())
                    .arg(cursorPos.y())
                    .arg(QGuiApplication::screens().size()));
        // 只取起窗前的画面，不能在系统启动页尚未退场时重新抓图。
        for (const auto &snapshot : snapshots_)
        {
            if (snapshot.first == screen)
            {
                screen_ = snapshot.second;
                break;
            }
        }
        snapshots_.clear();
        if (screen_.isNull())
        {
            finish(false, QString(), QStringLiteral("未能获取启动前的屏幕截图，请重试。"));
            return true;
        }
        // 暗色蒙层只合成一次；保存仍使用未经调暗的 screen_。
        dimmedScreen_ = screen_.copy();
        {
            QPainter dimmer(&dimmedScreen_);
            dimmer.fillRect(dimmedScreen_.rect(), QColor(0, 0, 0, 110));
        }
        availableRect_ = screen->availableGeometry();
        amplifier_ = new CaptureAmplifier(std::make_shared<QPixmap>(screen_), this);
        windowHandle()->setScreen(screen.data());
        setGeometry(screenGeometry);
        showFullScreen();
        raise();
        activateWindow();
        // 保留截图文字输入所需的系统焦点与 Qt 焦点。
        windowHandle()->requestActivate();
        setFocus();
        QRect defaultSel = mainWindowRect_.contains(cursorPos)
                               ? mainWindowRect_.intersected(screenGeometry)
                               : QRect();
        if (defaultSel.width() < 2 || defaultSel.height() < 2)
        {
            const int w = screenGeometry.width() * 3 / 5;
            const int h = screenGeometry.height() * 3 / 5;
            defaultSel = QRect(cursorPos.x() - w / 2, cursorPos.y() - h / 2, w, h)
                             .intersected(screenGeometry);
        }
        initDefaultSelection(defaultSel);
        return true;
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        const qint64 started = resizing_ ? resizeTrace_.clock.nsecsElapsed() : 0;
        QPainter painter(this);
        if (screen_.isNull())
        {
            // 首帧尚无截图时清空缓冲，避免未初始化像素闪现。
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect(), Qt::transparent);
            return;
        }
        // 底图直接覆盖；Qt 自动按脏区域裁剪，不再逐帧整屏混合半透明蒙层。
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(rect(), dimmedScreen_);

        const QRect selected = selection_.normalized().intersected(rect());
        if (!selected.isEmpty())
        {
            painter.drawPixmap(selected, screen_, sourceRect(selected));
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(45, 140, 255), 2));
            painter.drawRect(selected.adjusted(0, 0, -1, -1));

            painter.save();
            painter.setClipRect(selected);
            for (const Mark &mark : marks_)
                drawMark(painter, mark);
            if (drawingMark_)
                drawMark(painter, currentMark_);
            painter.restore();
        }
        painter.end();
        if (resizing_)
        {
            if (resizeTrace_.paint.count)
                resizeTrace_.maxPaintGapNs =
                    qMax(resizeTrace_.maxPaintGapNs, started - resizeTrace_.lastPaintNs);
            resizeTrace_.lastPaintNs = started;
            resizeTrace_.paint.add(resizeTrace_.clock.nsecsElapsed() - started);
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::RightButton)
        {
            dismiss();
            return;
        }
        if (screen_.isNull() || event->button() != Qt::LeftButton)
            return;

        // 用户按下左键 → 退出"自动框选阶段"，进入待确认/拖拽/编辑流程
        autoSelectPhase_ = false;

        if (textEditor_->isVisible() && !textEditor_->geometry().contains(event->pos()))
        {
            commitTextEdit();
        }

        // 自动框选完成后：不立即显示工具栏，进入待确认状态
        // - 若拖动（移动超过阈值）→ 重新框选
        // - 若仅点击（press+release 无明显移动）→ 显示工具栏确认
        if (selectionReady_ && !toolbar_->isVisible())
        {
            pendingConfirm_ = true;
            confirmStartPos_ = event->pos();
            return;
        }

        const QRect selected = selection_.normalized().intersected(rect());
        if (selectionReady_ && currentTool_ != Tool::None && selected.contains(event->pos()))
        {
            drawingMark_ = true;
            currentMark_ = {currentTool_, event->pos(), event->pos(), QString(), color_, font_};
            return;
        }
        // 已选工具但点击在选区外 → 不触发重新框选，仅忽略；
        // 工具激活时选区外点击保持选区不变，避免误触重新截图。
        if (selectionReady_ && currentTool_ != Tool::None && !selected.contains(event->pos()))
        {
            return;
        }
        if (selectionReady_ && currentTool_ == Tool::None)
        {
            // 未选工具时：先判定边框/角落（拉伸），再判定内部（移动），外部忽略
            const ResizeDir dir = getResizeDir(event->pos());
            if (dir != ResizeDir::None)
            {
                resizing_ = true;
                resizeDir_ = dir;
                resizeOrigin_ = selection_.normalized();
                // 只在边缘拉伸期间统计，松手统一输出，避免逐帧日志干扰性能。
                resizeTrace_ = {};
                resizeTrace_.clock.start();
                return;
            }
            if (selected.contains(event->pos()))
            {
                movingSelection_ = true;
                lastMovePos_ = event->pos();
                setCursor(Qt::SizeAllCursor);
                return;
            }
            // 已有选区时，点击选区外 → 清除当前选区，开始重新框选
        }
        // 无选区或已清除旧选区：开始框选
        draggingSelection_ = true;
        selectionReady_ = false;
        marks_.clear();
        toolbar_->hide();
        selection_ = QRect(event->pos(), QSize());
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        // 放大镜：仅在框选阶段（自动框选/拖拽框选）跟随鼠标显示，选区确认后隐藏
        // 汇文截图选区创建完成后隐藏放大镜
        if (amplifier_)
        {
            const bool inSelecting = (autoSelectPhase_ || draggingSelection_) && !pendingConfirm_;
            if (inSelecting)
            {
                amplifier_->onPostionChange(event->pos().x(), event->pos().y());
                amplifier_->show();
                amplifier_->raise();
            }
            else if (amplifier_->isVisible())
            {
                amplifier_->hide();
            }
        }
        // ====== 自动框选阶段：鼠标移动时按所在区域切换选区 ======
        // 汇文截图自动框选规则：
        //   主窗口内 → 框主窗口；桌面（availableRect 内）→ 框桌面（不含状态栏）；
        //   状态栏（availableRect 外）→ 框整个屏幕（含状态栏）。
        // 用户按下左键（pendingConfirm_/draggingSelection_）即退出此阶段。
        if (autoSelectPhase_ && !pendingConfirm_ && !draggingSelection_)
        {
            const QPoint localPos = event->pos();
            const QRect screenRect = rect();
            QRect newSel;
            // 1. 鼠标在主窗口内 → 框选主窗口
            if (!mainWindowRect_.isEmpty() && mainWindowRect_.contains(localPos))
            {
                newSel = mainWindowRect_.intersected(screenRect);
            }
            // 2. 鼠标在主窗口外的桌面（不含状态栏）→ 框选 availableRect（桌面区域）
            //    availableRect_ 是 QScreen::availableGeometry() 映射到本地坐标的结果，
            //    等于整个屏幕减去系统状态栏/导航栏后的"工作区"。
            else if (!availableRect_.isEmpty() && availableRect_.contains(localPos))
            {
                newSel = availableRect_;
            }
            // 3. 鼠标在状态栏（或其他系统占用区域）→ 框选整个屏幕（含状态栏）
            else
            {
                newSel = screenRect;
            }
            // 仅当选区确实变化时才更新，避免每帧重绘
            if (newSel != selection_)
            {
                selection_ = newSel;
                update();
            }
            // 自动框选阶段保持十字光标，不切换为箭头
            setCursor(Qt::CrossCursor);
            return;
        }
        if (pendingConfirm_)
        {
            // 拖动超过阈值 → 视为重新框选，清除旧选区开始新拖拽
            if ((event->pos() - confirmStartPos_).manhattanLength() > 3)
            {
                pendingConfirm_ = false;
                draggingSelection_ = true;
                selectionReady_ = false;
                marks_.clear();
                toolbar_->hide();
                selection_ = QRect(confirmStartPos_, event->pos());
                update();
            }
            return;
        }
        if (draggingSelection_)
        {
            selection_.setBottomRight(event->pos());
            const QRect sel = selection_.normalized();
            if (amplifier_)
                amplifier_->onSizeChange(sel.width(), sel.height());
            update();
        }
        else if (drawingMark_)
        {
            currentMark_.end = clampToSelection(event->pos());
            update();
        }
        else if (resizing_)
        {
            applyResize(event->pos());
        }
        else if (movingSelection_)
        {
            const QRect before = selection_.normalized();
            QRect moved = before.translated(event->pos() - lastMovePos_);
            moved.moveLeft(qBound(0, moved.left(), qMax(0, width() - moved.width())));
            moved.moveTop(qBound(0, moved.top(), qMax(0, height() - moved.height())));
            const QPoint delta = moved.topLeft() - before.topLeft();
            selection_ = moved;
            for (Mark &mark : marks_)
            {
                mark.start += delta;
                mark.end += delta;
            }
            lastMovePos_ = event->pos();
            positionToolbar();
            update();
        }
        else if (selectionReady_ && currentTool_ == Tool::None)
        {
            // 未选工具时：边框/角落 → 尺寸光标（可拖拽调整），内部 → 移动光标，外部 → 指针
            const ResizeDir dir = getResizeDir(event->pos());
            resizeDir_ = dir;
            if (dir != ResizeDir::None)
            {
                setCursor(ResizeCursor(dir));
            }
            else if (selection_.normalized().contains(event->pos()))
            {
                setCursor(Qt::SizeAllCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
        }
        else
        {
            // 已有选区时，区域外显示指针（不再显示框选十字线）；初始框选阶段保持十字线
            setCursor(selectionReady_ ? Qt::ArrowCursor : Qt::CrossCursor);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        // 待确认状态下松开（未拖动）→ 显示工具栏确认
        if (pendingConfirm_)
        {
            pendingConfirm_ = false;
            positionToolbar();
            toolbar_->show();
            toolbar_->raise();
            return;
        }
        if (resizing_)
        {
            // paintCalls 是 Qt paintEvent 次数，不代表屏幕最终呈现帧率。
            // inputMs 包含 toolbarMs；绘制耗时不包含 QPA 后续提交与系统合成。
            qInfo().noquote() << QStringLiteral("HUIWEN_SCREENSHOT_RESIZE: dragMs=%1 moves=%2 "
                                                "inputMs(avg/max)=%3 toolbarMs(avg/max)=%4 "
                                                "paintCalls=%5 paintMs(avg/max)=%6 "
                                                "inputGapMaxMs=%7 paintGapMaxMs=%8 "
                                                "window=%9x%10 image=%11x%12 dpr=%13")
                                     .arg(resizeTrace_.clock.elapsed())
                                     .arg(resizeTrace_.input.count)
                                     .arg(resizeTrace_.input.milliseconds())
                                     .arg(resizeTrace_.toolbar.milliseconds())
                                     .arg(resizeTrace_.paint.count)
                                     .arg(resizeTrace_.paint.milliseconds())
                                     .arg(resizeTrace_.maxInputGapNs / 1e6, 0, 'f', 2)
                                     .arg(resizeTrace_.maxPaintGapNs / 1e6, 0, 'f', 2)
                                     .arg(width())
                                     .arg(height())
                                     .arg(screen_.width())
                                     .arg(screen_.height())
                                     .arg(devicePixelRatioF(), 0, 'f', 2);
            // 拉伸结束：选区可能被拖动过小/翻转，保证至少 2x2 并重定位工具栏
            resizing_ = false;
            resizeDir_ = ResizeDir::None;
            const QRect sel = selection_.normalized();
            if (sel.width() < 2 || sel.height() < 2)
            {
                selection_ = resizeOrigin_;
            }
            positionToolbar();
            toolbar_->show();
            toolbar_->raise();
            update();
            return;
        }
        if (movingSelection_)
        {
            movingSelection_ = false;
            positionToolbar();
            toolbar_->show();
            toolbar_->raise();
            setCursor(Qt::CrossCursor);
            return;
        }
        if (drawingMark_)
        {
            drawingMark_ = false;
            currentMark_.end = clampToSelection(event->pos());
            if (currentMark_.tool == Tool::Text)
            {
                beginTextEdit(clampToSelection(event->pos()));
            }
            else if ((currentMark_.end - currentMark_.start).manhattanLength() > 2)
            {
                marks_.append(currentMark_);
            }
            update();
            return;
        }
        if (!draggingSelection_)
            return;

        draggingSelection_ = false;
        selection_.setBottomRight(event->pos());
        const QRect selected = selection_.normalized().intersected(rect());
        selectionReady_ = selected.width() >= 2 && selected.height() >= 2;
        if (selectionReady_)
        {
            positionToolbar();
            toolbar_->show();
            toolbar_->raise();
        }
        else
        {
            selection_ = QRect();
        }
        update();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && selectionReady_)
            saveAndFinish();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            dismiss();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == textEditor_)
        {
            if (event->type() == QEvent::KeyPress &&
                static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
            {
                cancelTextEdit();
                return true;
            }
            if (event->type() == QEvent::FocusOut)
            {
                // 焦点转移到工具栏或其弹窗（字号/颜色选择）时，不提交，保持编辑状态
                QWidget *fw = QApplication::focusWidget();
                if (fw)
                {
                    if (fw == toolbar_ || (toolbar_ && toolbar_->isAncestorOf(fw)) ||
                        fw->window() == (toolbar_ ? toolbar_->window() : nullptr))
                    {
                        return false;
                    }
                }
                if (QApplication::activePopupWidget())
                    return false;
                // 其他失焦场景延迟提交，确保 changeFont/changeColor 先设置 suppressCommit_
                QTimer::singleShot(0, this, [this]() { commitTextEdit(true); });
                return false;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

  private:
    void createToolbar()
    {
        toolbar_ = new CaptureToolbar(this);
        connect(toolbar_, &CaptureToolbar::finishScreen, this, [this]() { saveAndFinish(); });
        connect(toolbar_, &CaptureToolbar::quitScreen, this, [this]() { dismiss(); });
        connect(toolbar_, &CaptureToolbar::saveScreen, this, [this]() { saveAs(); });
        connect(toolbar_, &CaptureToolbar::changeEditState, this,
                [this](CaptureToolbar::CaptureEditState state) {
                    if (state == CaptureToolbar::CES_UNDO)
                    {
                        cancelTextEdit();
                        if (!marks_.isEmpty())
                            marks_.removeLast();
                        currentTool_ = Tool::None;
                        update();
                        return;
                    }
                    commitTextEdit();
                    switch (state)
                    {
                    case CaptureToolbar::CES_RECTANGLE:
                        currentTool_ = Tool::Rectangle;
                        break;
                    case CaptureToolbar::CES_ELLIPSE:
                        currentTool_ = Tool::Ellipse;
                        break;
                    case CaptureToolbar::CES_ARROW:
                        currentTool_ = Tool::Arrow;
                        break;
                    case CaptureToolbar::CES_TEXT:
                        currentTool_ = Tool::Text;
                        break;
                    default:
                        currentTool_ = Tool::None;
                        break;
                    }
                });
        connect(toolbar_, &CaptureToolbar::changeColor, this, [this](int color) {
            color_ = qBound(0, color, 13);
            if (textEditor_ && textEditor_->isVisible())
            {
                suppressCommitBriefly();
                textEditor_->setStyleSheet(
                    QStringLiteral(
                        "QLineEdit{border:1px solid #dadada;background:rgba(255,255,255,20);"
                        "padding:2px;color:%1;}")
                        .arg(vecColor[color_]));
                textEditor_->setFocus();
            }
        });
        connect(toolbar_, &CaptureToolbar::changeFont, this, [this](int font) {
            font_ = qBound(0, font, 7);
            if (textEditor_ && textEditor_->isVisible())
            {
                // 正在编辑文字：只更新编辑器字体/颜色，不触发提交
                suppressCommitBriefly();
                textEditor_->setFont(QFont(QStringLiteral("Arial"), vecFont[font_].toInt()));
                // setFont 不触发 textChanged，必须显式按新字体重算编辑器尺寸
                updateTextEditorSize();
                textEditor_->setFocus();
            }
            else
            {
                for (int i = marks_.size() - 1; i >= 0; --i)
                {
                    if (marks_[i].tool == Tool::Text)
                    {
                        marks_[i].font = font_;
                        break;
                    }
                }
                update();
            }
        });
    }

    void createTextEditor()
    {
        textEditor_ = new QLineEdit(this);
        textEditor_->hide();
        textEditor_->setFixedHeight(40);
        // 不使用 Qt 平台插件的输入法：该插件与 CEF 争抢进程内唯一的 IME 客户端，
        // 被 CEF 抢占后永远无法重新 attach（只能输入英文）。改为由 HarmonyImeInput 直接
        // 使用 IME Kit C API 接管输入法，因此这里关闭 Qt 侧的输入法查询。
        textEditor_->setAttribute(Qt::WA_InputMethodEnabled, false);
        textEditor_->setStyleSheet(
            QStringLiteral("QLineEdit{border:1px solid #dadada;background:rgba(255,255,255,20);"
                           "padding:2px;color:#ff0000;}"));
        textEditor_->installEventFilter(this);
        connect(textEditor_, &QLineEdit::returnPressed, this, [this]() { commitTextEdit(); });
        connect(textEditor_, &QLineEdit::textChanged, this,
                [this](const QString &) { updateTextEditorSize(); });
    }

    // 按当前字体与文本重算编辑器尺寸，并把位置重新约束回截图选区内。
    // 注意：字号变更是 setFont，不会触发 textChanged；若不在该路径调用本函数，
    // 编辑器会停留在旧字号算出的宽高上（表现为"改了字号输入框大小不变"）。
    void updateTextEditorSize()
    {
        if (!textEditor_)
            return;
        const QFontMetrics fm = textEditor_->fontMetrics();
        textEditor_->setFixedWidth(
            qBound(120, fm.horizontalAdvance(textEditor_->text()) + 24, 400));
        textEditor_->setFixedHeight(qBound(40, fm.height() + 12, 120));
        // 尺寸变化后重新限制编辑器位置在截图选区内
        const QRect sel = selection_.normalized().intersected(rect());
        if (sel.isEmpty())
            return;
        const int maxX = qMax(sel.left(), sel.right() - textEditor_->width());
        const int maxY = qMax(sel.top(), sel.bottom() - textEditor_->height());
        textEditor_->move(qBound(sel.left(), textEditor_->x(), maxX),
                          qBound(sel.top(), textEditor_->y(), maxY));
    }

    void beginTextEdit(const QPoint &point)
    {
        suppressCommit_ = false;
        commitTextEdit();
        pendingTextMark_ = currentMark_;
        textEditor_->setFont(QFont(QStringLiteral("Arial"), vecFont[qBound(0, font_, 7)].toInt()));
        textEditor_->setStyleSheet(
            QStringLiteral("QLineEdit{border:1px solid #dadada;background:rgba(255,255,255,20);"
                           "padding:2px;color:%1;}")
                .arg(vecColor[qBound(0, color_, 13)]));
        textEditor_->setText(QString());
        textEditor_->setFixedWidth(120);
        // 限制文字编辑器位置在截图选区内
        const QRect sel = selection_.normalized().intersected(rect());
        const int maxX = qMax(sel.left(), sel.right() - textEditor_->width());
        const int maxY = qMax(sel.top(), sel.bottom() - textEditor_->height());
        textEditor_->move(qBound(sel.left(), point.x(), maxX), qBound(sel.top(), point.y(), maxY));
        textEditor_->show();
        textEditor_->raise();
        textEditor_->setFocus();
        // 鸿蒙平台：接管输入法。Qt 平台插件的 IME 客户端会被 CEF 抢占且无法恢复
        // （log: "show text input failed ... Input method client detached"），
        // 这里改用 HarmonyImeInput 直接 attach，支持中英文输入法切换。
        HarmonyImeInput::instance().setEnterCallback([this]() { commitTextEdit(); });
        HarmonyImeInput::instance().start(textEditor_);
    }

    // 字号/颜色弹窗引起的失焦只需短暂抑制提交；加兜底超时，避免标志残留
    // 导致后续"完成/保存"时文字被吞掉
    void suppressCommitBriefly()
    {
        suppressCommit_ = true;
        QTimer::singleShot(500, this, [this]() { suppressCommit_ = false; });
    }

    // 提交文字编辑内容。
    // fromFocusLoss=true：编辑器失焦触发的提交，允许被 suppressCommit_ 抑制，
    //   避免"点击字号/颜色弹窗"引起的失焦把文字提前提交掉。
    // fromFocusLoss=false：完成/保存/回车/点击选区等显式提交，必须强制生效——
    //   否则 suppressCommit_ 残留会让最终提交被吞掉，导致截图上的文字丢失。
    void commitTextEdit(bool fromFocusLoss = false)
    {
        if (!textEditor_->isVisible())
            return;
        if (fromFocusLoss)
        {
            // 字号/颜色弹窗打开时不提交，保持编辑状态
            if (QApplication::activePopupWidget())
                return;
            // 字号/颜色变更导致的失焦：跳过提交，保持编辑器继续编辑
            if (suppressCommit_)
            {
                suppressCommit_ = false;
                return;
            }
        }
        else
        {
            suppressCommit_ = false;
            if (QWidget *popup = QApplication::activePopupWidget())
                popup->close();
        }
        const QString text = textEditor_->text();
        textEditor_->hide();
        HarmonyImeInput::instance().stop();
        if (!text.isEmpty())
        {
            pendingTextMark_.start =
                textEditor_->pos() + QPoint(4, textEditor_->fontMetrics().ascent() + 4);
            pendingTextMark_.end = textEditor_->geometry().bottomRight();
            pendingTextMark_.text = text;
            pendingTextMark_.font = font_;
            pendingTextMark_.color = color_;
            marks_.append(pendingTextMark_);
        }
        textEditor_->clear();
        setFocus();
        update();
    }

    void cancelTextEdit()
    {
        if (!textEditor_->isVisible())
            return;
        textEditor_->hide();
        textEditor_->clear();
        HarmonyImeInput::instance().stop();
        setFocus();
    }

    void positionToolbar()
    {
        const QRect selected = selection_.normalized().intersected(rect());
        toolbar_->setClipRect(selected);
    }

    // 将鼠标坐标限制在截图选区内，防止箭头/矩形/椭圆绘制超出选区边界
    QPoint clampToSelection(const QPoint &pos) const
    {
        const QRect sel = selection_.normalized().intersected(rect());
        if (sel.isEmpty())
            return pos;
        return QPoint(qBound(sel.left(), pos.x(), sel.right()),
                      qBound(sel.top(), pos.y(), sel.bottom()));
    }

    void drawMark(QPainter &painter, const Mark &mark) const
    {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(vecColor[qBound(0, mark.color, 13)]), 3));
        // 默认不填充（矩形／椭圆只画边框）；
        // 箭头分支内部自行 setBrush 实心填充，避免状态泄漏到后续图形
        painter.setBrush(Qt::NoBrush);
        const QRect markRect(mark.start, mark.end);
        switch (mark.tool)
        {
        case Tool::Rectangle:
            painter.drawRect(markRect.normalized());
            break;
        case Tool::Ellipse:
            painter.drawEllipse(markRect.normalized());
            break;
        case Tool::Arrow: {
            // 汇文截图箭头绘制：
            // 实心锥形箭头（箭头头三角形 + 锥形箭身，头粗尾细）
            painter.setPen(QPen(QColor(vecColor[qBound(0, mark.color, 13)]), 1));
            painter.setBrush(QColor(vecColor[qBound(0, mark.color, 13)]));
            const double par = 15.0;
            const double slopy = std::atan2(double(mark.end.y() - mark.start.y()),
                                            double(mark.end.x() - mark.start.x()));
            const double cos_y = std::cos(slopy);
            const double sin_y = std::sin(slopy);
            const QPoint head_point1(mark.end.x() + int(-par * cos_y - (par / 2.0 * sin_y)),
                                     mark.end.y() + int(-par * sin_y + (par / 2.0 * cos_y)));
            const QPoint head_point2(mark.end.x() + int(-par * cos_y + (par / 2.0 * sin_y)),
                                     mark.end.y() - int(par / 2.0 * cos_y + par * sin_y));
            const QPoint head_points[3] = {mark.end, head_point1, head_point2};
            painter.drawPolygon(head_points, 3);

            const int offset_x = int(par * sin_y / 3);
            const int offset_y = int(par * cos_y / 3);
            const QPoint body_point1(
                mark.end.x() + int(-par * cos_y - (par / 2.0 * sin_y)) + offset_x,
                mark.end.y() + int(-par * sin_y + (par / 2.0 * cos_y)) - offset_y);
            const QPoint body_point2(
                mark.end.x() + int(-par * cos_y + (par / 2.0 * sin_y) - offset_x),
                mark.end.y() - int(par / 2.0 * cos_y + par * sin_y) + offset_y);
            const QPoint body_points[3] = {mark.start, body_point1, body_point2};
            painter.drawPolygon(body_points, 3);
            break;
        }
        case Tool::Text:
            painter.setFont(
                QFont(QStringLiteral("Arial"), vecFont[qBound(0, mark.font, 7)].toInt()));
            painter.drawText(mark.start, mark.text);
            break;
        case Tool::None:
            break;
        }
    }

    QPixmap capturePixmap() const
    {
        const QRect selected = selection_.normalized().intersected(rect());
        QPixmap result = screen_.copy(sourceRect(selected));
        QPainter painter(&result);
        const qreal sx = selected.width() > 0 ? qreal(result.width()) / selected.width() : 1.0;
        const qreal sy = selected.height() > 0 ? qreal(result.height()) / selected.height() : 1.0;
        painter.scale(sx, sy);
        painter.translate(-selected.left(), -selected.top());
        for (const Mark &mark : marks_)
            drawMark(painter, mark);
        return result;
    }

    void saveAs()
    {
        if (!selectionReady_)
            return;
        commitTextEdit();
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存图片"),
                                                          QStringLiteral("截图.png"),
                                                          QStringLiteral("PNG Files (*.png)"));
        if (!path.isEmpty())
        {
            if (capturePixmap().save(path, "PNG"))
                finish(true, path, QString());
            else
                qWarning() << "HUIWEN_SCREENSHOT: 另存为失败" << path;
        }
    }

    void dismiss()
    {
        finish(false, QString(), QString());
    }

    void saveAndFinish()
    {
        if (!selectionReady_)
            return;
        commitTextEdit();
        const QPixmap captured = capturePixmap();
        // 截图完成后同步写入汇文 Qt 剪贴板。
        QApplication::clipboard()->setPixmap(captured, QClipboard::Clipboard);

        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                 QStringLiteral("/screenshots"));
        if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        {
            finish(false, QString(), QStringLiteral("无法创建截图缓存目录。"));
            return;
        }
        const QString path = dir.filePath(
            QStringLiteral("huiwen_capture_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
        if (!captured.save(path, "PNG"))
        {
            finish(false, QString(), QStringLiteral("截图保存失败。"));
            return;
        }

#ifdef Q_OS_OPENHARMONY
        // 汇文 CEF 窗口的 Ctrl+V 由 Chromium 直接读取 OHOS
        // 系统 pasteboard；把截图 PNG 路径以 file:// URI（UDMF_META_GENERAL_FILE_URI）
        // 同时附加 pixelmap 记录写入 pasteboard，确保 web 窗口可粘贴。
        writeScreenshotToSystemPasteboard(captured, path);
#endif
        finish(true, path, QString());
    }

    QRect sourceRect(const QRect &logical) const
    {
        const qreal sx = width() > 0 ? qreal(screen_.width()) / width() : 1.0;
        const qreal sy = height() > 0 ? qreal(screen_.height()) / height() : 1.0;
        return QRect(qRound(logical.x() * sx), qRound(logical.y() * sy),
                     qRound(logical.width() * sx), qRound(logical.height() * sy));
    }

    void finish(bool ok, const QString &path, const QString &error)
    {
        if (finished_)
            return;
        finished_ = true;
        snapshots_.clear();
        // 结束截图前释放输入法客户端，避免影响后续 CEF 输入。
        if (textEditor_)
        {
            textEditor_->clearFocus();
            textEditor_->hide();
        }
        HarmonyImeInput::instance().stop();
        if (hiddenWindow_)
        {
            hiddenWindow_->show();
            hiddenWindow_->raise();
            hiddenWindow_->activateWindow();
        }
        const auto callback = std::move(callback_);
        qInfo().noquote() << "HUIWEN_SCREENSHOT_FINISHED:" << ok << path << error;
        // 销毁 overlay 窗口，避免它继续作为 startAbility 使用的 Ability（详见构造函数注释）
        if (callback)
            callback(ok, path, error);
        close();
    }

    // 拉伸诊断仅累加计数与单调时钟，结束时才格式化日志。
    struct ResizeSample
    {
        qint64 count = 0;
        qint64 totalNs = 0;
        qint64 maxNs = 0;
        void add(qint64 ns)
        {
            ++count;
            totalNs += ns;
            maxNs = qMax(maxNs, ns);
        }
        QString milliseconds() const
        {
            return QStringLiteral("%1/%2")
                .arg(count ? totalNs / (1e6 * count) : 0, 0, 'f', 2)
                .arg(maxNs / 1e6, 0, 'f', 2);
        }
    };
    struct ResizeTrace
    {
        QElapsedTimer clock;
        ResizeSample input, toolbar, paint;
        qint64 lastInputNs = 0, lastPaintNs = 0;
        qint64 maxInputGapNs = 0, maxPaintGapNs = 0;
    } resizeTrace_;

    QPixmap screen_;
    QPixmap dimmedScreen_;
    ScreenSnapshots snapshots_;
    QPointer<QWidget> hiddenWindow_;
    ScreenCapture::FinishCallback callback_;
    CaptureToolbar *toolbar_ = nullptr;
    QLineEdit *textEditor_ = nullptr;
    QList<Mark> marks_;
    QRect selection_;
    Mark currentMark_;
    Mark pendingTextMark_;
    Tool currentTool_ = Tool::None;
    int color_ = 9;
    int font_ = 3;
    bool suppressCommit_ = false;
    bool draggingSelection_ = false;
    bool selectionReady_ = false;
    bool drawingMark_ = false;
    bool movingSelection_ = false;
    bool resizing_ = false;
    ResizeDir resizeDir_ = ResizeDir::None;
    QRect resizeOrigin_;
    QPoint lastMovePos_;
    bool finished_ = false;
    // 自动框选后的待确认状态：按下不松开 → 拖动重新框选；仅点击 → 显示工具栏确认
    bool pendingConfirm_ = false;
    QPoint confirmStartPos_;
    // 截图启动时的主窗口矩形（用于"鼠标在主窗口内 → 框选主窗口"判断）
    // 存 geometry 而非 QWidget*，因为主窗口可能被 hide()，但仍可拿 geometry
    QRect mainWindowRect_;
    // 屏幕工作区矩形（可用桌面，不含状态栏），对应 QScreen::availableGeometry()，
    // 用于"鼠标在主窗口外桌面 → 框选桌面（不含状态栏）"
    QRect availableRect_;
    // 是否处于"自动框选阶段"（鼠标移动可动态切换选区；按下/拖拽/确认后退出）
    bool autoSelectPhase_ = false;
    // 放大取色器（鼠标位置局部放大 + RGB 取值）
    CaptureAmplifier *amplifier_ = nullptr;
};

void BeginCapture(bool ignoreActiveWindow, ScreenCapture::FinishCallback callback)
{
    if (!QGuiApplication::primaryScreen())
    {
        callback(false, QString(), QStringLiteral("未找到可截图的屏幕。"));
        return;
    }

    QPointer<QWidget> activeWindow = QApplication::activeWindow();
    if (ignoreActiveWindow && activeWindow)
        activeWindow->hide();

    QTimer::singleShot(ignoreActiveWindow ? 180 : 0, [activeWindow,
                                                      callback = std::move(callback)]() mutable {
        // 先抓取所有当前屏幕，再起窗获取焦点；避免启动页/Logo 混入截图。
        ScreenSnapshots snapshots;
        for (QScreen *screen : QGuiApplication::screens())
        {
            snapshots.append(qMakePair(QPointer<QScreen>(screen), QPixmap()));
        }
        bool hasSnapshot = false;
        for (auto &snapshot : snapshots)
        {
            if (!snapshot.first)
                continue;
            const QString name = snapshot.first->name();
            snapshot.second = snapshot.first->grabWindow(0);
            hasSnapshot = hasSnapshot || !snapshot.second.isNull();
            qInfo().noquote() << (QStringLiteral("HUIWEN_SCREENSHOT: pregrab screen=%1 size=%2x%3")
                                      .arg(name)
                                      .arg(snapshot.second.width())
                                      .arg(snapshot.second.height()));
        }
        if (!hasSnapshot)
        {
            if (activeWindow)
                activeWindow->show();
            callback(false, QString(), QStringLiteral("系统屏幕捕获失败。"));
            return;
        }
        // 每次截图新建 overlay，截图结束时 close 销毁（见 CaptureOverlay 构造函数注释：
        // 常驻复用的 overlay 会把 startAbility 锁死在一个已进入后台的 Ability 上）
        auto *overlay = new CaptureOverlay(
            std::move(snapshots),
            activeWindow && !activeWindow->isVisible() ? activeWindow.data() : nullptr,
            std::move(callback));
        // 这里只创建透明窗口取得原生焦点；查询成功后选用已有截图，不再抓图。
        overlay->show();
        overlay->raise();
        overlay->activateWindow();
        if (overlay->windowHandle())
            overlay->windowHandle()->requestActivate();
        auto *timer = new QTimer(overlay);
        timer->setInterval(100);
        QObject::connect(timer, &QTimer::timeout, overlay,
                         [overlay, timer, deadline = QDeadlineTimer(5000)] {
                             // 失败回调可能销毁窗口，先停表；只有仍在等待时才继续。
                             timer->stop();
                             if (!overlay->startOnPointerScreen(deadline.hasExpired()))
                                 timer->start();
                         });
        timer->start();
    });
}

} // namespace

ScreenCapture *ScreenCapture::getInstance()
{
    static ScreenCapture instance;
    return &instance;
}

void ScreenCapture::startCapture(bool ignoreActiveWindow, FinishCallback callback)
{
    // ArkTS 入口已取得截图权限；这里只执行 Qt GUI 线程上的截图业务。
    BeginCapture(ignoreActiveWindow, std::move(callback));
}
