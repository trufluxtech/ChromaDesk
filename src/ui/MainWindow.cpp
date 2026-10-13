#include "ui/MainWindow.h"

#include "core/ImageLoader.h"
#include "core/ImageProcessor.h"
#include "core/PassportSheet.h"
#include "core/PresetRepository.h"
#include "core/SmartMaskEngine.h"
#include "ui/AdjustmentControl.h"
#include "ui/Branding.h"
#include "ui/ExportDialog.h"
#include "ui/GenerativeFillClient.h"
#include "ui/HistogramWidget.h"
#include "ui/TextOverlayDialog.h"
#include "ui/ThemeManager.h"
#include "ui/WatermarkDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDirIterator>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageWriter>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace chroma {
namespace {

QString fileDialogFilter()
{
    return QStringLiteral("Supported photos (%1);;All files (*)")
        .arg(ImageLoader::supportedNameFilters().join(QLatin1Char(' ')));
}

QString captionForEntry(const CatalogEntry &entry)
{
    QString caption = QFileInfo(entry.path).completeBaseName();
    if (entry.picked)
        caption.prepend(QStringLiteral("● "));
    if (entry.rating > 0)
        caption += QStringLiteral("  %1").arg(QString(entry.rating, QChar(0x2605)));
    return caption;
}

QIcon libraryViewIcon(bool grid)
{
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(190, 198, 210), 1.6));
    painter.setBrush(QColor(112, 130, 154, 90));
    if (grid) {
        for (int row = 0; row < 2; ++row) {
            for (int column = 0; column < 2; ++column)
                painter.drawRoundedRect(QRectF(3.0 + column * 9.0,
                    3.0 + row * 9.0, 7.0, 7.0), 1.0, 1.0);
        }
    } else {
        for (int row = 0; row < 3; ++row) {
            const qreal y = 3.0 + row * 6.5;
            painter.drawRoundedRect(QRectF(3.0, y, 4.0, 4.0), 0.8, 0.8);
            painter.drawLine(QPointF(9.5, y + 2.0), QPointF(19.0, y + 2.0));
        }
    }
    return QIcon(pixmap);
}

QIcon libraryVisibilityIcon()
{
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(190, 198, 210), 1.6));
    painter.drawEllipse(QRectF(7.5, 7.5, 7.0, 7.0));
    QPainterPath eye;
    eye.moveTo(2.0, 11.0);
    eye.cubicTo(6.0, 4.0, 16.0, 4.0, 20.0, 11.0);
    eye.cubicTo(16.0, 18.0, 6.0, 18.0, 2.0, 11.0);
    painter.drawPath(eye);
    return QIcon(pixmap);
}

struct InnerImageMapping {
    QPointF origin;
    QSizeF scale = QSizeF(1.0, 1.0);
    QSize pixelSize;
};

InnerImageMapping innerImageMapping(const QSize &outerSize, double borderFraction)
{
    InnerImageMapping mapping;
    mapping.pixelSize = outerSize;
    if (!outerSize.isValid() || borderFraction <= 0.0001)
        return mapping;
    const double bounded = qBound(0.0, borderFraction, 0.20);
    const double borderPixels = bounded * qMin(outerSize.width(), outerSize.height())
        / (1.0 + bounded * 2.0);
    mapping.origin = QPointF(borderPixels / outerSize.width(),
                             borderPixels / outerSize.height());
    mapping.scale = QSizeF(1.0 - mapping.origin.x() * 2.0,
                           1.0 - mapping.origin.y() * 2.0);
    mapping.pixelSize = QSize(qMax(1, qRound(outerSize.width() - borderPixels * 2.0)),
                              qMax(1, qRound(outerSize.height() - borderPixels * 2.0)));
    return mapping;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_canvas(new ImageCanvas(this)),
      m_catalogSaveTimer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("ChromaDesk %1")
        .arg(QString::fromLatin1(CHROMADESK_VERSION)));
    setWindowIcon(branding::applicationIcon());
    resize(1500, 920);
    setMinimumSize(1040, 680);
    setAcceptDrops(true);
    setDockNestingEnabled(true);
    setCentralWidget(m_canvas);
    m_generativeFillClient = new GenerativeFillClient(this);

    createActions();
    createMenus();
    createToolbar();
    createLibraryDock();
    createDevelopDock();

    m_catalogSaveTimer->setSingleShot(true);
    m_catalogSaveTimer->setInterval(500);

    connect(&m_document, &ImageDocument::previewStarted, this, [this] {
        statusBar()->showMessage(QStringLiteral("Rendering preview…"));
    });
    connect(&m_document, &ImageDocument::previewReady, this, [this](const QImage &) {
        updateDocumentPresentation();
        statusBar()->showMessage(QStringLiteral("Ready"), 1800);
    });
    connect(&m_document, &ImageDocument::layerStackChanged, this,
            [this](const LayerStack &) {
        refreshLayers();
        syncActiveLayer();
        m_catalogSaveTimer->start();
    });
    connect(&m_document, &ImageDocument::activeLayerChanged, this,
            [this](const QString &) { syncActiveLayer(); });
    connect(m_catalogSaveTimer, &QTimer::timeout, this, [this] {
        if (!m_document.isOpen())
            return;
        m_catalog.updateLayers(m_document.path(), m_document.layerStack());
        updateItemCaption(m_document.path());
    });
    connect(&m_document, &ImageDocument::errorOccurred, this,
            [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("ChromaDesk"), message);
    });
    connect(m_canvas, &ImageCanvas::cropRequested, this, [this](const QRectF &relative) {
        LayerStack stack = m_document.layerStack();
        EditState &state = stack.baseLayer()->edits;
        const QRectF current = state.crop;
        state.crop = QRectF(current.x() + relative.x() * current.width(),
                            current.y() + relative.y() * current.height(),
                            relative.width() * current.width(),
                            relative.height() * current.height())
                         .intersected(QRectF(0.0, 0.0, 1.0, 1.0));
        m_document.setLayerStack(stack, QStringLiteral("Crop"));
        m_cropAction->setChecked(false);
    });
    connect(m_canvas, &ImageCanvas::shapeMaskRequested,
            this, &MainWindow::completeShapeMask);
    connect(m_canvas, &ImageCanvas::watermarkMoveRequested,
            this, &MainWindow::moveSelectedWatermark);
    connect(m_canvas, &ImageCanvas::dustRemovalRequested,
            this, &MainWindow::addDustRemoval);
    connect(m_canvas, &ImageCanvas::cloneSourceSelected, this,
            [this](const QPointF &) {
        statusBar()->showMessage(QStringLiteral(
            "Clone source selected. Now click the destination."));
    });
    connect(m_canvas, &ImageCanvas::cloneRequested,
            this, &MainWindow::addCloneStamp);
    connect(m_generativeFillClient, &GenerativeFillClient::completed,
            this, &MainWindow::finishGenerativeFill);
    connect(m_canvas, &ImageCanvas::zoomChanged, this, [this](double percent) {
        statusBar()->showMessage(percent <= 0.0
            ? QStringLiteral("Fit")
            : QStringLiteral("Zoom: %1%").arg(qRound(percent)), 1200);
    });

    QString catalogError;
    if (!m_catalog.load(&catalogError)) {
        QMessageBox::warning(this, QStringLiteral("Catalog"),
                             QStringLiteral("The catalog could not be loaded: %1")
                                 .arg(catalogError));
    }
    refreshLibrary();

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    if (m_libraryDock && m_libraryVisibilityAction) {
        const QSignalBlocker blocker(m_libraryVisibilityAction);
        m_libraryVisibilityAction->setChecked(m_libraryDock->isVisible());
    }
    statusBar()->showMessage(ImageLoader::rawSupportAvailable()
        ? QStringLiteral("Ready · Camera RAW enabled")
        : QStringLiteral("Ready · Install LibRaw to enable camera RAW"));
}

void MainWindow::createActions()
{
    m_saveEditsAction = new QAction(QStringLiteral("Save Edits"), this);
    m_saveEditsAction->setShortcut(QKeySequence::Save);
    m_saveEditsAction->setEnabled(false);
    connect(m_saveEditsAction, &QAction::triggered, this, &MainWindow::saveEdits);

    m_saveCopyAction = new QAction(QStringLiteral("Save File…"), this);
    m_saveCopyAction->setShortcut(QKeySequence::SaveAs);
    m_saveCopyAction->setEnabled(false);
    connect(m_saveCopyAction, &QAction::triggered, this, &MainWindow::saveEditedCopy);

    m_exportAction = new QAction(QStringLiteral("Export…"), this);
    m_exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    m_exportAction->setEnabled(false);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportPhoto);

    m_convertAction = new QAction(QStringLiteral("Convert Format…"), this);
    m_convertAction->setEnabled(false);
    connect(m_convertAction, &QAction::triggered, this, &MainWindow::convertPhoto);

    m_cropAction = new QAction(QStringLiteral("Crop"), this);
    m_cropAction->setCheckable(true);
    m_cropAction->setShortcut(QKeySequence(Qt::Key_R));
    connect(m_cropAction, &QAction::toggled, this, &MainWindow::toggleCrop);

    m_originalAction = new QAction(QStringLiteral("Original"), this);
    m_originalAction->setCheckable(true);
    connect(m_originalAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) m_canvas->setCompareMode(ImageCanvas::CompareMode::Original);
        else if (!m_sideBySideAction->isChecked() && !m_splitAction->isChecked())
            m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    });
    m_originalAction->setShortcut(QKeySequence(Qt::Key_Backslash));

    m_sideBySideAction = new QAction(QStringLiteral("Side by Side"), this);
    m_sideBySideAction->setCheckable(true);
    connect(m_sideBySideAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) m_canvas->setCompareMode(ImageCanvas::CompareMode::SideBySide);
        else if (!m_originalAction->isChecked() && !m_splitAction->isChecked())
            m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    });

    m_splitAction = new QAction(QStringLiteral("Split"), this);
    m_splitAction->setCheckable(true);
    connect(m_splitAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) m_canvas->setCompareMode(ImageCanvas::CompareMode::Split);
        else if (!m_originalAction->isChecked() && !m_sideBySideAction->isChecked())
            m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    });

    auto *compareGroup = new QActionGroup(this);
    compareGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    compareGroup->addAction(m_originalAction);
    compareGroup->addAction(m_sideBySideAction);
    compareGroup->addAction(m_splitAction);

    m_pickAction = new QAction(QStringLiteral("Pick"), this);
    m_pickAction->setCheckable(true);
    m_pickAction->setShortcut(QKeySequence(Qt::Key_P));
    connect(m_pickAction, &QAction::triggered, this, &MainWindow::togglePicked);

    m_libraryVisibilityAction = new QAction(libraryVisibilityIcon(),
        QStringLiteral("Photo Library"), this);
    m_libraryVisibilityAction->setCheckable(true);
    m_libraryVisibilityAction->setChecked(true);
    m_libraryVisibilityAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    connect(m_libraryVisibilityAction, &QAction::toggled, this, [this](bool visible) {
        if (m_libraryDock)
            m_libraryDock->setVisible(visible);
    });
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto *addAction = fileMenu->addAction(QStringLiteral("Add Photos…"),
                                           this, &MainWindow::importPhotos,
                                           QKeySequence::Open);
    addAction->setShortcutContext(Qt::WindowShortcut);
    fileMenu->addAction(QStringLiteral("Import Folder…"), this, &MainWindow::importFolder,
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveEditsAction);
    fileMenu->addAction(m_saveCopyAction);
    fileMenu->addAction(m_exportAction);
    fileMenu->addAction(m_convertAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit,
                        QKeySequence::Quit);

    auto *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(m_document.undoStack()->createUndoAction(this, QStringLiteral("Undo")));
    editMenu->addAction(m_document.undoStack()->createRedoAction(this, QStringLiteral("Redo")));
    editMenu->addSeparator();
    auto *preferencesAction = editMenu->addAction(
        QStringLiteral("Preferences…"), this, &MainWindow::showPreferences,
        QKeySequence(Qt::CTRL | Qt::Key_Comma));
    preferencesAction->setMenuRole(QAction::PreferencesRole);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Reset Selected Layer"), this, &MainWindow::resetEdits,
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    auto *photoMenu = menuBar()->addMenu(QStringLiteral("&Photo"));
    photoMenu->addAction(QStringLiteral("Rotate Clockwise"), this,
                         &MainWindow::rotateClockwise, QKeySequence(Qt::CTRL | Qt::Key_R));
    photoMenu->addAction(m_cropAction);
    photoMenu->addAction(m_pickAction);
    for (int rating = 0; rating <= 5; ++rating) {
        auto *action = photoMenu->addAction(rating == 0
            ? QStringLiteral("Clear Rating")
            : QStringLiteral("%1 Star%2").arg(rating).arg(rating == 1 ? QString() : QStringLiteral("s")));
        action->setShortcut(QKeySequence(QString::number(rating)));
        connect(action, &QAction::triggered, this, [this, rating] { setRating(rating); });
    }

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(QStringLiteral("Fit to Window"), m_canvas,
                        &ImageCanvas::fitToWindow, QKeySequence(Qt::Key_F));
    viewMenu->addAction(QStringLiteral("100%"), m_canvas,
                        &ImageCanvas::actualPixels, QKeySequence(Qt::Key_Z));
    viewMenu->addAction(QStringLiteral("Zoom In"), m_canvas,
                        &ImageCanvas::zoomIn, QKeySequence::ZoomIn);
    viewMenu->addAction(QStringLiteral("Zoom Out"), m_canvas,
                        &ImageCanvas::zoomOut, QKeySequence::ZoomOut);
    viewMenu->addAction(QStringLiteral("Clear Custom Guides"), m_canvas,
                        &ImageCanvas::clearGuides);
    viewMenu->addAction(m_libraryVisibilityAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_originalAction);
    viewMenu->addAction(m_sideBySideAction);
    viewMenu->addAction(m_splitAction);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("About ChromaDesk"), this, &MainWindow::showAbout);
}

void MainWindow::createToolbar()
{
    auto *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(QStringLiteral("＋ Add Photos"), this, &MainWindow::importPhotos);
    toolbar->addAction(QStringLiteral("Import Folder"), this, &MainWindow::importFolder);
    toolbar->addAction(m_saveEditsAction);
    toolbar->addSeparator();
    toolbar->addAction(m_document.undoStack()->createUndoAction(this, QStringLiteral("Undo")));
    toolbar->addAction(m_document.undoStack()->createRedoAction(this, QStringLiteral("Redo")));
    toolbar->addSeparator();
    toolbar->addAction(m_cropAction);
    toolbar->addAction(QStringLiteral("Rotate ↻"), this, &MainWindow::rotateClockwise);
    toolbar->addSeparator();
    toolbar->addAction(m_originalAction);
    toolbar->addAction(m_sideBySideAction);
    toolbar->addAction(m_splitAction);
    toolbar->addAction(m_libraryVisibilityAction);
    toolbar->addSeparator();
    auto *fitAction = toolbar->addAction(QStringLiteral("Fit"),
                                          m_canvas, &ImageCanvas::fitToWindow);
    fitAction->setToolTip(QStringLiteral("Fit photo to window"));
    auto *zoomOutAction = toolbar->addAction(QStringLiteral("−"),
                                             m_canvas, &ImageCanvas::zoomOut);
    zoomOutAction->setToolTip(QStringLiteral("Reduce zoom"));
    auto *actualPixelsAction = toolbar->addAction(QStringLiteral("100%"),
                                                   m_canvas, &ImageCanvas::actualPixels);
    actualPixelsAction->setToolTip(QStringLiteral("Show actual pixels at 100%"));
    auto *zoomInAction = toolbar->addAction(QStringLiteral("＋"),
                                            m_canvas, &ImageCanvas::zoomIn);
    zoomInAction->setToolTip(QStringLiteral("Increase zoom"));

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    toolbar->addAction(m_exportAction);
}

void MainWindow::createLibraryDock()
{
    m_libraryDock = new QDockWidget(QStringLiteral("PHOTO LIBRARY"), this);
    m_libraryDock->setObjectName(QStringLiteral("libraryDock"));
    m_libraryDock->setFeatures(QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    m_libraryDock->setMinimumWidth(220);

    auto *panel = new QWidget(m_libraryDock);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);

    const QSettings librarySettings;
    auto *viewRow = new QHBoxLayout;
    const QString savedView = librarySettings.value(
        QStringLiteral("library/view"), QStringLiteral("icons")).toString();
    m_libraryIconViewButton = new QToolButton(panel);
    m_libraryIconViewButton->setIcon(libraryViewIcon(true));
    m_libraryIconViewButton->setToolTip(QStringLiteral("Icon view"));
    m_libraryIconViewButton->setCheckable(true);
    m_libraryIconViewButton->setAutoExclusive(true);
    m_libraryIconViewButton->setChecked(savedView != QStringLiteral("list"));
    m_libraryListViewButton = new QToolButton(panel);
    m_libraryListViewButton->setIcon(libraryViewIcon(false));
    m_libraryListViewButton->setToolTip(QStringLiteral("List view"));
    m_libraryListViewButton->setCheckable(true);
    m_libraryListViewButton->setAutoExclusive(true);
    m_libraryListViewButton->setChecked(savedView == QStringLiteral("list"));
    auto *hideLibraryButton = new QToolButton(panel);
    hideLibraryButton->setIcon(libraryVisibilityIcon());
    hideLibraryButton->setToolTip(QStringLiteral("Hide photo library"));
    viewRow->addWidget(m_libraryIconViewButton);
    viewRow->addWidget(m_libraryListViewButton);
    viewRow->addStretch(1);
    viewRow->addWidget(hideLibraryButton);

    auto *iconSizeRow = new QHBoxLayout;
    iconSizeRow->addWidget(new QLabel(QStringLiteral("Image size"), panel));
    m_libraryIconSize = new QSlider(Qt::Horizontal, panel);
    m_libraryIconSize->setRange(72, 200);
    m_libraryIconSize->setValue(librarySettings.value(
        QStringLiteral("library/iconSize"), 182).toInt());
    m_libraryIconSizeValue = new QLabel(panel);
    m_libraryIconSizeValue->setMinimumWidth(42);
    m_libraryIconSizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    iconSizeRow->addWidget(m_libraryIconSize, 1);
    iconSizeRow->addWidget(m_libraryIconSizeValue);

    m_libraryList = new QListWidget(panel);
    m_libraryList->setResizeMode(QListView::Adjust);
    m_libraryList->setMovement(QListView::Static);
    connect(m_libraryList, &QListWidget::itemClicked,
            this, &MainWindow::openLibraryItem);
    connect(m_libraryIconViewButton, &QToolButton::clicked,
            this, &MainWindow::applyLibraryViewSettings);
    connect(m_libraryListViewButton, &QToolButton::clicked,
            this, &MainWindow::applyLibraryViewSettings);
    connect(hideLibraryButton, &QToolButton::clicked,
            m_libraryVisibilityAction, [this] {
        m_libraryVisibilityAction->setChecked(false);
    });
    connect(m_libraryIconSize, &QSlider::valueChanged,
            this, &MainWindow::applyLibraryViewSettings);
    applyLibraryViewSettings();

    auto *classificationRow = new QHBoxLayout;
    auto *pickButton = new QPushButton(QStringLiteral("Pick"), panel);
    pickButton->setCheckable(true);
    connect(pickButton, &QPushButton::clicked, m_pickAction, &QAction::trigger);
    connect(m_pickAction, &QAction::toggled, pickButton, &QPushButton::setChecked);
    classificationRow->addWidget(pickButton);
    for (int rating = 1; rating <= 5; ++rating) {
        auto *star = new QPushButton(QString::number(rating), panel);
        star->setToolTip(QStringLiteral("Set %1-star rating").arg(rating));
        star->setFixedWidth(30);
        connect(star, &QPushButton::clicked, this, [this, rating] { setRating(rating); });
        classificationRow->addWidget(star);
    }

    layout->addLayout(viewRow);
    layout->addLayout(iconSizeRow);
    layout->addWidget(m_libraryList, 1);
    layout->addLayout(classificationRow);
    connect(m_libraryDock, &QDockWidget::visibilityChanged,
            this, [this](bool visible) {
        const QSignalBlocker blocker(m_libraryVisibilityAction);
        m_libraryVisibilityAction->setChecked(visible);
    });
    m_libraryDock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, m_libraryDock);
}

void MainWindow::createDevelopDock()
{
    auto *dock = new QDockWidget(QStringLiteral("DEVELOP"), this);
    dock->setObjectName(QStringLiteral("developDock"));
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setMinimumWidth(310);

    auto *scroll = new QScrollArea(dock);
    scroll->setWidgetResizable(true);
    auto *panel = new QWidget(scroll);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 16);
    layout->setSpacing(6);

    auto *brandRow = new QHBoxLayout;
    auto *brandLogo = new QLabel(panel);
    brandLogo->setPixmap(branding::applicationIcon().pixmap(38, 38));
    brandLogo->setFixedSize(42, 42);
    brandLogo->setAlignment(Qt::AlignCenter);
    brandLogo->setToolTip(QStringLiteral("ChromaDesk application logo"));
    auto *brandVersion = new QLabel(QStringLiteral(
        "<b>ChromaDesk</b><br><span style='color:#9aa1af'>Version %1</span>")
        .arg(QString::fromLatin1(CHROMADESK_VERSION)), panel);
    brandVersion->setTextFormat(Qt::RichText);
    brandVersion->setAccessibleName(QStringLiteral("ChromaDesk version %1")
        .arg(QString::fromLatin1(CHROMADESK_VERSION)));
    brandRow->addWidget(brandLogo);
    brandRow->addWidget(brandVersion, 1);
    layout->addLayout(brandRow);

    m_histogram = new HistogramWidget(panel);
    layout->addWidget(m_histogram);

    m_fileInfo = new QLabel(QStringLiteral("No photo selected"), panel);
    m_fileInfo->setWordWrap(true);
    m_fileInfo->setObjectName(QStringLiteral("mutedLabel"));
    m_editSummary = new QLabel(QStringLiteral("Original"), panel);
    m_editSummary->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(m_fileInfo);
    layout->addWidget(m_editSummary);

    auto *tabs = new QTabWidget(panel);
    tabs->setDocumentMode(true);
    auto *layersTab = new QWidget(tabs);
    auto *layersTabLayout = new QVBoxLayout(layersTab);
    layersTabLayout->setContentsMargins(4, 8, 4, 8);
    auto *masksTab = new QWidget(tabs);
    auto *masksTabLayout = new QVBoxLayout(masksTab);
    masksTabLayout->setContentsMargins(4, 8, 4, 8);
    auto *adjustTab = new QWidget(tabs);
    auto *adjustTabLayout = new QVBoxLayout(adjustTab);
    adjustTabLayout->setContentsMargins(4, 8, 4, 8);
    auto *toolsTab = new QWidget(tabs);
    auto *toolsTabLayout = new QVBoxLayout(toolsTab);
    toolsTabLayout->setContentsMargins(4, 8, 4, 8);
    auto *filtersTab = new QWidget(tabs);
    auto *filtersTabLayout = new QVBoxLayout(filtersTab);
    filtersTabLayout->setContentsMargins(4, 8, 4, 8);
    auto *fileTab = new QWidget(tabs);
    auto *fileTabLayout = new QVBoxLayout(fileTab);
    fileTabLayout->setContentsMargins(4, 8, 4, 8);
    tabs->setUsesScrollButtons(true);
    tabs->tabBar()->setExpanding(false);
    tabs->addTab(layersTab, QStringLiteral("Layers"));
    tabs->addTab(masksTab, QStringLiteral("AI Masks"));
    tabs->addTab(adjustTab, QStringLiteral("Adjust"));
    tabs->addTab(toolsTab, QStringLiteral("Tools"));
    tabs->addTab(filtersTab, QStringLiteral("Filters"));
    tabs->addTab(fileTab, QStringLiteral("File & Export"));

    auto *layerGroup = new QGroupBox(QStringLiteral("Layers · top to bottom"), panel);
    auto *layerLayout = new QVBoxLayout(layerGroup);
    layerLayout->setContentsMargins(0, 5, 0, 0);
    m_layerList = new QListWidget(layerGroup);
    m_layerList->setObjectName(QStringLiteral("layerList"));
    m_layerList->setMinimumHeight(118);
    m_layerList->setMaximumHeight(190);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_layerList, &QListWidget::currentItemChanged,
            this, &MainWindow::selectLayer);
    connect(m_layerList, &QListWidget::itemChanged,
            this, &MainWindow::updateLayerItem);
    layerLayout->addWidget(m_layerList);

    auto *layerButtons = new QHBoxLayout;
    auto *addLayerButton = new QPushButton(QStringLiteral("＋"), layerGroup);
    addLayerButton->setToolTip(QStringLiteral("Add adjustment layer"));
    auto *duplicateLayerButton = new QPushButton(QStringLiteral("Duplicate"), layerGroup);
    auto *removeLayerButton = new QPushButton(QStringLiteral("−"), layerGroup);
    removeLayerButton->setToolTip(QStringLiteral("Delete selected layer"));
    auto *moveUpButton = new QPushButton(QStringLiteral("↑"), layerGroup);
    moveUpButton->setToolTip(QStringLiteral("Move layer up"));
    auto *moveDownButton = new QPushButton(QStringLiteral("↓"), layerGroup);
    moveDownButton->setToolTip(QStringLiteral("Move layer down"));
    connect(addLayerButton, &QPushButton::clicked, this, &MainWindow::addLayer);
    connect(duplicateLayerButton, &QPushButton::clicked, this, &MainWindow::duplicateLayer);
    connect(removeLayerButton, &QPushButton::clicked, this, &MainWindow::removeLayer);
    connect(moveUpButton, &QPushButton::clicked, this, &MainWindow::moveLayerUp);
    connect(moveDownButton, &QPushButton::clicked, this, &MainWindow::moveLayerDown);
    layerButtons->addWidget(addLayerButton);
    layerButtons->addWidget(duplicateLayerButton, 1);
    layerButtons->addWidget(removeLayerButton);
    layerButtons->addWidget(moveUpButton);
    layerButtons->addWidget(moveDownButton);
    layerLayout->addLayout(layerButtons);

    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(QStringLiteral("Opacity"), layerGroup));
    m_layerOpacity = new QSlider(Qt::Horizontal, layerGroup);
    m_layerOpacity->setRange(0, 100);
    m_layerOpacity->setValue(100);
    m_layerOpacityValue = new QLabel(QStringLiteral("100%"), layerGroup);
    m_layerOpacityValue->setMinimumWidth(38);
    m_layerOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_layerOpacity, &QSlider::valueChanged,
            this, &MainWindow::setLayerOpacity);
    opacityRow->addWidget(m_layerOpacity, 1);
    opacityRow->addWidget(m_layerOpacityValue);
    layerLayout->addLayout(opacityRow);
    layersTabLayout->addWidget(layerGroup);

    auto *maskGroup = new QGroupBox(QStringLiteral("AI & Range Masks"), panel);
    auto *maskLayout = new QVBoxLayout(maskGroup);
    maskLayout->setContentsMargins(0, 5, 0, 0);
    auto *maskButtons = new QGridLayout;
    auto *subjectButton = new QPushButton(QStringLiteral("AI Subject"), maskGroup);
    auto *backgroundButton = new QPushButton(QStringLiteral("AI Background"), maskGroup);
    auto *skyButton = new QPushButton(QStringLiteral("AI Sky"), maskGroup);
    auto *colorMaskButton = new QPushButton(QStringLiteral("Color Range…"), maskGroup);
    auto *luminanceMaskButton = new QPushButton(QStringLiteral("Luminance…"), maskGroup);
    auto *rectangleMaskButton = new QPushButton(QStringLiteral("Rectangle"), maskGroup);
    auto *ellipseMaskButton = new QPushButton(QStringLiteral("Ellipse"), maskGroup);
    connect(subjectButton, &QPushButton::clicked, this, &MainWindow::createAiSubjectMask);
    connect(backgroundButton, &QPushButton::clicked, this, &MainWindow::createAiBackgroundMask);
    connect(skyButton, &QPushButton::clicked, this, &MainWindow::createAiSkyMask);
    connect(colorMaskButton, &QPushButton::clicked, this, &MainWindow::createColorRangeMask);
    connect(luminanceMaskButton, &QPushButton::clicked, this, &MainWindow::createLuminanceRangeMask);
    connect(rectangleMaskButton, &QPushButton::clicked, this, &MainWindow::beginRectangleMask);
    connect(ellipseMaskButton, &QPushButton::clicked, this, &MainWindow::beginEllipseMask);
    maskButtons->addWidget(subjectButton, 0, 0);
    maskButtons->addWidget(backgroundButton, 0, 1);
    maskButtons->addWidget(skyButton, 1, 0, 1, 2);
    maskButtons->addWidget(colorMaskButton, 2, 0);
    maskButtons->addWidget(luminanceMaskButton, 2, 1);
    maskButtons->addWidget(rectangleMaskButton, 3, 0);
    maskButtons->addWidget(ellipseMaskButton, 3, 1);
    maskLayout->addLayout(maskButtons);

    m_aiEngineStatus = new QLabel(QStringLiteral("Engine: %1")
        .arg(SmartMaskEngine::engineName(LayerMaskType::AiSubject)), maskGroup);
    m_aiEngineStatus->setWordWrap(true);
    m_aiEngineStatus->setObjectName(QStringLiteral("mutedLabel"));
    m_aiEngineStatus->setToolTip(QStringLiteral(
        "Subject and Background use native Apple Vision on macOS 14 or later. "
        "All processing remains on this computer."));
    maskLayout->addWidget(m_aiEngineStatus);

    m_maskSummary = new QLabel(QStringLiteral("Entire Image"), maskGroup);
    m_maskSummary->setWordWrap(true);
    m_maskSummary->setObjectName(QStringLiteral("mutedLabel"));
    maskLayout->addWidget(m_maskSummary);
    auto *editMaskButton = new QPushButton(QStringLiteral("Edit Selected Mask…"), maskGroup);
    connect(editMaskButton, &QPushButton::clicked, this, &MainWindow::editSelectedMask);
    maskLayout->addWidget(editMaskButton);
    auto *featherRow = new QHBoxLayout;
    featherRow->addWidget(new QLabel(QStringLiteral("Feather"), maskGroup));
    m_maskFeather = new QSlider(Qt::Horizontal, maskGroup);
    m_maskFeather->setRange(0, 50);
    m_maskFeather->setValue(8);
    m_maskFeatherValue = new QLabel(QStringLiteral("8%"), maskGroup);
    m_maskFeatherValue->setMinimumWidth(36);
    connect(m_maskFeather, &QSlider::valueChanged,
            this, &MainWindow::setMaskFeather);
    featherRow->addWidget(m_maskFeather, 1);
    featherRow->addWidget(m_maskFeatherValue);
    maskLayout->addLayout(featherRow);
    masksTabLayout->addWidget(maskGroup);

    auto *aiFilterGroup = new QGroupBox(QStringLiteral("AI Filters"), panel);
    auto *aiFilterLayout = new QGridLayout(aiFilterGroup);
    auto *autoEnhanceButton = new QPushButton(QStringLiteral("Auto Enhance"), aiFilterGroup);
    auto *subjectPopButton = new QPushButton(QStringLiteral("Subject Pop"), aiFilterGroup);
    auto *backgroundBlurButton = new QPushButton(QStringLiteral("Background Blur"), aiFilterGroup);
    auto *monoBackgroundButton = new QPushButton(QStringLiteral("Color Subject"), aiFilterGroup);
    auto *enhanceSkyButton = new QPushButton(QStringLiteral("Enhance Sky"), aiFilterGroup);
    auto *replaceColorButton = new QPushButton(QStringLiteral("Replace Color…"), aiFilterGroup);
    connect(autoEnhanceButton, &QPushButton::clicked, this, &MainWindow::applyAiAutoEnhance);
    connect(subjectPopButton, &QPushButton::clicked, this, &MainWindow::applyAiSubjectPop);
    connect(backgroundBlurButton, &QPushButton::clicked, this, &MainWindow::applyAiBackgroundBlur);
    connect(monoBackgroundButton, &QPushButton::clicked,
            this, &MainWindow::applyAiMonochromeBackground);
    connect(enhanceSkyButton, &QPushButton::clicked,
            this, &MainWindow::applyAiEnhanceSky);
    connect(replaceColorButton, &QPushButton::clicked,
            this, &MainWindow::applyAiReplaceColor);
    aiFilterLayout->addWidget(autoEnhanceButton, 0, 0);
    aiFilterLayout->addWidget(subjectPopButton, 0, 1);
    aiFilterLayout->addWidget(backgroundBlurButton, 1, 0);
    aiFilterLayout->addWidget(monoBackgroundButton, 1, 1);
    aiFilterLayout->addWidget(enhanceSkyButton, 2, 0);
    aiFilterLayout->addWidget(replaceColorButton, 2, 1);
    masksTabLayout->addWidget(aiFilterGroup);

    auto *presetGroup = new QGroupBox(QStringLiteral("Presets"), panel);
    auto *presetLayout = new QHBoxLayout(presetGroup);
    m_presetCombo = new QComboBox(presetGroup);
    for (const auto &preset : PresetRepository::builtIns())
        m_presetCombo->addItem(preset.name, preset.description);
    auto *applyButton = new QPushButton(QStringLiteral("Apply"), presetGroup);
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyPreset);
    presetLayout->addWidget(m_presetCombo, 1);
    presetLayout->addWidget(applyButton);
    adjustTabLayout->addWidget(presetGroup);

    auto *creativeFilterGroup = new QGroupBox(QStringLiteral("50 Creative Filters"), panel);
    auto *creativeFilterLayout = new QVBoxLayout(creativeFilterGroup);
    creativeFilterLayout->setContentsMargins(0, 5, 0, 0);
    m_filterCategory = new QComboBox(creativeFilterGroup);
    m_filterCategory->addItem(QStringLiteral("All Filters"), QString());
    m_filterCategory->addItem(QStringLiteral("Cinematic"), QStringLiteral("Cinematic"));
    m_filterCategory->addItem(QStringLiteral("Film"), QStringLiteral("Film"));
    m_filterCategory->addItem(QStringLiteral("Portrait"), QStringLiteral("Portrait"));
    m_filterCategory->addItem(QStringLiteral("Landscape"), QStringLiteral("Landscape"));
    m_filterCategory->addItem(QStringLiteral("Black & White"),
                              QStringLiteral("Black & White"));
    connect(m_filterCategory, &QComboBox::currentIndexChanged,
            this, &MainWindow::refreshCreativeFilters);
    creativeFilterLayout->addWidget(m_filterCategory);

    m_filterList = new QListWidget(creativeFilterGroup);
    m_filterList->setMinimumHeight(330);
    m_filterList->setAlternatingRowColors(true);
    connect(m_filterList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { applyCreativeFilter(); });
    creativeFilterLayout->addWidget(m_filterList, 1);
    auto *creativeFilterNote = new QLabel(QStringLiteral(
        "Double-click a look or select it and press Apply. Each filter is added as an editable layer."),
        creativeFilterGroup);
    creativeFilterNote->setWordWrap(true);
    creativeFilterNote->setObjectName(QStringLiteral("mutedLabel"));
    creativeFilterLayout->addWidget(creativeFilterNote);
    auto *applyCreativeFilterButton = new QPushButton(
        QStringLiteral("Apply as New Layer"), creativeFilterGroup);
    connect(applyCreativeFilterButton, &QPushButton::clicked,
            this, &MainWindow::applyCreativeFilter);
    creativeFilterLayout->addWidget(applyCreativeFilterButton);
    filtersTabLayout->addWidget(creativeFilterGroup);
    refreshCreativeFilters();

    auto *lightGroup = new QGroupBox(QStringLiteral("Light"), panel);
    auto *lightLayout = new QVBoxLayout(lightGroup);
    lightLayout->setContentsMargins(0, 4, 0, 0);
    addAdjustment(lightLayout, QStringLiteral("Exposure"), -5.0, 5.0, 0.1, 1, Exposure);
    addAdjustment(lightLayout, QStringLiteral("Contrast"), -100, 100, 1, 0, Contrast);
    addAdjustment(lightLayout, QStringLiteral("Highlights"), -100, 100, 1, 0, Highlights);
    addAdjustment(lightLayout, QStringLiteral("Shadows"), -100, 100, 1, 0, Shadows);
    addAdjustment(lightLayout, QStringLiteral("Whites"), -100, 100, 1, 0, Whites);
    addAdjustment(lightLayout, QStringLiteral("Blacks"), -100, 100, 1, 0, Blacks);
    adjustTabLayout->addWidget(lightGroup);

    auto *colorGroup = new QGroupBox(QStringLiteral("Color"), panel);
    auto *colorLayout = new QVBoxLayout(colorGroup);
    colorLayout->setContentsMargins(0, 4, 0, 0);
    addAdjustment(colorLayout, QStringLiteral("Temp"), -100, 100, 1, 0, Temperature);
    addAdjustment(colorLayout, QStringLiteral("Tint"), -100, 100, 1, 0, Tint);
    addAdjustment(colorLayout, QStringLiteral("Vibrance"), -100, 100, 1, 0, Vibrance);
    addAdjustment(colorLayout, QStringLiteral("Saturation"), -100, 100, 1, 0, Saturation);
    addAdjustment(colorLayout, QStringLiteral("Replace Amount"), 0, 100, 1, 0,
                  ReplaceAmount);
    adjustTabLayout->addWidget(colorGroup);

    auto *presenceGroup = new QGroupBox(QStringLiteral("Presence"), panel);
    auto *presenceLayout = new QVBoxLayout(presenceGroup);
    presenceLayout->setContentsMargins(0, 4, 0, 0);
    addAdjustment(presenceLayout, QStringLiteral("Clarity"), -100, 100, 1, 0, Clarity);
    addAdjustment(presenceLayout, QStringLiteral("Blur"), 0, 100, 1, 0, Blur);
    adjustTabLayout->addWidget(presenceGroup);

    auto *transformGroup = new QGroupBox(QStringLiteral("Crop & Transform"), panel);
    auto *transformLayout = new QVBoxLayout(transformGroup);
    auto *cropPresetRow = new QHBoxLayout;
    cropPresetRow->addWidget(new QLabel(QStringLiteral("Crop ratio"), transformGroup));
    auto *cropPreset = new QComboBox(transformGroup);
    cropPreset->addItem(QStringLiteral("Free"), 0.0);
    cropPreset->addItem(QStringLiteral("Original image"), -1.0);
    cropPreset->addItem(QStringLiteral("Square · 1:1"), 1.0);
    cropPreset->addItem(QStringLiteral("Portrait · 4:5"), 4.0 / 5.0);
    cropPreset->addItem(QStringLiteral("Passport · 35:45"), 35.0 / 45.0);
    cropPreset->addItem(QStringLiteral("Photo · 3:2"), 3.0 / 2.0);
    cropPreset->addItem(QStringLiteral("Screen · 16:9"), 16.0 / 9.0);
    cropPreset->addItem(QStringLiteral("A4 portrait"), 210.0 / 297.0);
    connect(cropPreset, &QComboBox::currentIndexChanged, this,
            [this, cropPreset](int) {
        double ratio = cropPreset->currentData().toDouble();
        if (ratio < 0.0 && m_document.isOpen() && !m_document.preview().isNull()) {
            ratio = static_cast<double>(m_document.preview().width())
                / m_document.preview().height();
        }
        m_canvas->setCropAspectRatio(qMax(0.0, ratio));
    });
    cropPresetRow->addWidget(cropPreset, 1);
    transformLayout->addLayout(cropPresetRow);

    auto *transformButtons = new QHBoxLayout;
    auto *cropButton = new QPushButton(QStringLiteral("Crop"), transformGroup);
    cropButton->setCheckable(true);
    connect(cropButton, &QPushButton::toggled, m_cropAction, &QAction::setChecked);
    connect(m_cropAction, &QAction::toggled, cropButton, &QPushButton::setChecked);
    auto *rotateButton = new QPushButton(QStringLiteral("Rotate 90°"), transformGroup);
    connect(rotateButton, &QPushButton::clicked, this, &MainWindow::rotateClockwise);
    auto *resetCropButton = new QPushButton(QStringLiteral("Reset Crop"), transformGroup);
    connect(resetCropButton, &QPushButton::clicked, this, [this] {
        if (!m_document.isOpen()) return;
        LayerStack stack = m_document.layerStack();
        stack.baseLayer()->edits.crop = QRectF(0.0, 0.0, 1.0, 1.0);
        m_document.setLayerStack(stack, QStringLiteral("Reset Crop"));
    });
    transformButtons->addWidget(cropButton);
    transformButtons->addWidget(rotateButton);
    transformButtons->addWidget(resetCropButton);
    transformLayout->addLayout(transformButtons);
    toolsTabLayout->addWidget(transformGroup);

    auto *retouchGroup = new QGroupBox(QStringLiteral("Retouch & Generative Fill"), panel);
    auto *retouchLayout = new QVBoxLayout(retouchGroup);
    auto *retouchButtons = new QHBoxLayout;
    m_dustRemovalButton = new QPushButton(QStringLiteral("Dust / Spot Heal"),
                                          retouchGroup);
    m_dustRemovalButton->setCheckable(true);
    m_dustRemovalButton->setToolTip(QStringLiteral(
        "Click dust spots or small blemishes; ChromaDesk chooses nearby texture locally."));
    m_cloneStampButton = new QPushButton(QStringLiteral("Clone Stamp"), retouchGroup);
    m_cloneStampButton->setCheckable(true);
    m_cloneStampButton->setToolTip(QStringLiteral(
        "Click once for the source and again for the destination."));
    connect(m_dustRemovalButton, &QPushButton::toggled,
            this, &MainWindow::toggleDustRemoval);
    connect(m_cloneStampButton, &QPushButton::toggled,
            this, &MainWindow::toggleCloneStamp);
    retouchButtons->addWidget(m_dustRemovalButton, 1);
    retouchButtons->addWidget(m_cloneStampButton, 1);
    retouchLayout->addLayout(retouchButtons);

    auto *retouchSizeRow = new QHBoxLayout;
    retouchSizeRow->addWidget(new QLabel(QStringLiteral("Brush size"), retouchGroup));
    m_retouchSize = new QSlider(Qt::Horizontal, retouchGroup);
    m_retouchSize->setRange(5, 120);
    m_retouchSize->setValue(24);
    m_retouchSizeValue = new QLabel(QStringLiteral("24 px"), retouchGroup);
    m_retouchSizeValue->setMinimumWidth(48);
    connect(m_retouchSize, &QSlider::valueChanged, this, [this](int value) {
        m_retouchSizeValue->setText(QStringLiteral("%1 px").arg(value));
        if (m_syncingLayers || !m_document.isOpen())
            return;
        LayerStack stack = m_document.layerStack();
        AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
        if (!layer || (layer->retouchOperation.type != RetouchType::HealSpot
            && layer->retouchOperation.type != RetouchType::CloneStamp))
            return;
        layer->retouchOperation.radius = retouchRadius();
        m_document.setLayerStack(stack,
            QStringLiteral("Resize %1").arg(layer->name), 9401);
    });
    retouchSizeRow->addWidget(m_retouchSize, 1);
    retouchSizeRow->addWidget(m_retouchSizeValue);
    retouchLayout->addLayout(retouchSizeRow);

    m_generativeFillButton = new QPushButton(
        QStringLiteral("Mark Area & Generative Fill…"), retouchGroup);
    connect(m_generativeFillButton, &QPushButton::clicked,
            this, &MainWindow::beginGenerativeFill);
    retouchLayout->addWidget(m_generativeFillButton);
    auto *retouchNote = new QLabel(QStringLiteral(
        "Dust Heal and Clone Stamp run entirely on this computer. Generative Fill "
        "sends only a resized photo, transparent area mask, and your prompt to "
        "OpenAI after explicit consent; API usage may be billed."), retouchGroup);
    retouchNote->setWordWrap(true);
    retouchNote->setObjectName(QStringLiteral("mutedLabel"));
    retouchLayout->addWidget(retouchNote);
    toolsTabLayout->addWidget(retouchGroup);

    auto *textGroup = new QGroupBox(QStringLiteral("Text on Photo"), panel);
    auto *textLayout = new QHBoxLayout(textGroup);
    auto *addTextButton = new QPushButton(QStringLiteral("＋ Add Text…"), textGroup);
    auto *editTextButton = new QPushButton(QStringLiteral("Edit Selected…"), textGroup);
    connect(addTextButton, &QPushButton::clicked, this, &MainWindow::addTextLayer);
    connect(editTextButton, &QPushButton::clicked, this, &MainWindow::editTextLayer);
    textLayout->addWidget(addTextButton, 1);
    textLayout->addWidget(editTextButton, 1);
    toolsTabLayout->addWidget(textGroup);

    auto *watermarkGroup = new QGroupBox(QStringLiteral("Watermark Layer"),
                                          toolsTab);
    auto *watermarkLayout = new QVBoxLayout(watermarkGroup);
    auto *watermarkButtons = new QHBoxLayout;
    auto *addWatermarkButton = new QPushButton(QStringLiteral("＋ Add Watermark…"),
                                                watermarkGroup);
    auto *editWatermarkButton = new QPushButton(QStringLiteral("Edit Selected…"),
                                                 watermarkGroup);
    connect(addWatermarkButton, &QPushButton::clicked,
            this, &MainWindow::addWatermark);
    connect(editWatermarkButton, &QPushButton::clicked,
            this, &MainWindow::editWatermark);
    watermarkButtons->addWidget(addWatermarkButton, 1);
    watermarkButtons->addWidget(editWatermarkButton, 1);
    watermarkLayout->addLayout(watermarkButtons);
    auto *watermarkNote = new QLabel(QStringLiteral(
        "Create a text or image watermark, control its size and transparency, "
        "then select its layer and drag the broken outline anywhere on the photo."),
        watermarkGroup);
    watermarkNote->setWordWrap(true);
    watermarkNote->setObjectName(QStringLiteral("mutedLabel"));
    watermarkLayout->addWidget(watermarkNote);
    toolsTabLayout->addWidget(watermarkGroup);

    auto *borderGroup = new QGroupBox(QStringLiteral("Image Border"), panel);
    auto *borderLayout = new QVBoxLayout(borderGroup);
    auto *borderSizeRow = new QHBoxLayout;
    borderSizeRow->addWidget(new QLabel(QStringLiteral("Width"), borderGroup));
    m_borderSize = new QSlider(Qt::Horizontal, borderGroup);
    m_borderSize->setRange(0, 20);
    m_borderSizeValue = new QLabel(QStringLiteral("0%"), borderGroup);
    m_borderSizeValue->setMinimumWidth(34);
    connect(m_borderSize, &QSlider::valueChanged,
            this, &MainWindow::setBorderSize);
    borderSizeRow->addWidget(m_borderSize, 1);
    borderSizeRow->addWidget(m_borderSizeValue);
    auto *borderColorRow = new QHBoxLayout;
    borderColorRow->addWidget(new QLabel(QStringLiteral("Colour"), borderGroup));
    m_borderColorButton = new QPushButton(borderGroup);
    connect(m_borderColorButton, &QPushButton::clicked,
            this, &MainWindow::chooseBorderColor);
    updateBorderButton(Qt::white);
    auto *removeBorderButton = new QPushButton(QStringLiteral("Remove"), borderGroup);
    connect(removeBorderButton, &QPushButton::clicked, this, [this] {
        m_borderSize->setValue(0);
    });
    borderColorRow->addWidget(m_borderColorButton, 1);
    borderColorRow->addWidget(removeBorderButton);
    auto *borderNote = new QLabel(QStringLiteral(
        "Borders are added outside the edited photo. White is the default colour."),
        borderGroup);
    borderNote->setWordWrap(true);
    borderNote->setObjectName(QStringLiteral("mutedLabel"));
    borderLayout->addLayout(borderSizeRow);
    borderLayout->addLayout(borderColorRow);
    borderLayout->addWidget(borderNote);
    toolsTabLayout->addWidget(borderGroup);

    auto *guideGroup = new QGroupBox(QStringLiteral("Rulers & Grids"), panel);
    auto *guideLayout = new QVBoxLayout(guideGroup);
    auto *rulerCheck = new QCheckBox(QStringLiteral("Show percentage rulers"), guideGroup);
    connect(rulerCheck, &QCheckBox::toggled,
            m_canvas, &ImageCanvas::setRulersVisible);
    connect(rulerCheck, &QCheckBox::toggled, this, [](bool visible) {
        QSettings().setValue(QStringLiteral("guides/rulers"), visible);
    });
    auto *gridRow = new QHBoxLayout;
    gridRow->addWidget(new QLabel(QStringLiteral("Composition grid"), guideGroup));
    auto *gridCombo = new QComboBox(guideGroup);
    gridCombo->addItem(QStringLiteral("Off"),
                       static_cast<int>(ImageCanvas::GridMode::None));
    gridCombo->addItem(QStringLiteral("Rule of thirds"),
                       static_cast<int>(ImageCanvas::GridMode::Thirds));
    gridCombo->addItem(QStringLiteral("4 × 4"),
                       static_cast<int>(ImageCanvas::GridMode::Quarters));
    gridCombo->addItem(QStringLiteral("Golden ratio"),
                       static_cast<int>(ImageCanvas::GridMode::GoldenRatio));
    connect(gridCombo, &QComboBox::currentIndexChanged, this,
            [this, gridCombo](int) {
        const int mode = gridCombo->currentData().toInt();
        m_canvas->setGridMode(static_cast<ImageCanvas::GridMode>(mode));
        QSettings().setValue(QStringLiteral("guides/gridMode"), mode);
    });
    const QSettings guideSettings;
    rulerCheck->setChecked(guideSettings.value(
        QStringLiteral("guides/rulers"), false).toBool());
    const int savedGridMode = guideSettings.value(QStringLiteral("guides/gridMode"),
        static_cast<int>(ImageCanvas::GridMode::None)).toInt();
    gridCombo->setCurrentIndex(qMax(0, gridCombo->findData(savedGridMode)));
    gridRow->addWidget(gridCombo, 1);
    guideLayout->addWidget(rulerCheck);
    guideLayout->addLayout(gridRow);
    auto *guideNote = new QLabel(QStringLiteral(
        "Drag from the top ruler to create a horizontal guide, or from the left ruler for a vertical guide. Guides never appear in saved or exported images."),
        guideGroup);
    guideNote->setWordWrap(true);
    guideNote->setObjectName(QStringLiteral("mutedLabel"));
    guideLayout->addWidget(guideNote);
    auto *clearGuidesButton = new QPushButton(QStringLiteral("Clear Custom Guides"), guideGroup);
    connect(clearGuidesButton, &QPushButton::clicked,
            m_canvas, &ImageCanvas::clearGuides);
    guideLayout->addWidget(clearGuidesButton);
    toolsTabLayout->addWidget(guideGroup);

    auto *passportGroup = new QGroupBox(QStringLiteral("Passport Photo Grid"), panel);
    auto *passportLayout = new QVBoxLayout(passportGroup);
    m_passportSheetSize = new QComboBox(passportGroup);
    m_passportSheetSize->addItem(QStringLiteral("4 × 6 inch · 6 photos"), 0);
    m_passportSheetSize->addItem(QStringLiteral("A4 · 25 photos"), 1);
    auto *passportButton = new QPushButton(
        QStringLiteral("Create 35 × 45 mm Passport Grid…"), passportGroup);
    connect(passportButton, &QPushButton::clicked,
            this, &MainWindow::preparePassportSheet);
    auto *passportNote = new QLabel(QStringLiteral(
        "Creates a 300 DPI sheet from the current edited image using a centred 35 × 45 mm crop. Verify current authority requirements before printing."),
        passportGroup);
    passportNote->setWordWrap(true);
    passportNote->setObjectName(QStringLiteral("mutedLabel"));
    passportLayout->addWidget(m_passportSheetSize);
    passportLayout->addWidget(passportButton);
    passportLayout->addWidget(passportNote);
    fileTabLayout->addWidget(passportGroup);

    auto actionButton = [](const QString &label, QAction *action, QWidget *parent) {
        auto *button = new QPushButton(label, parent);
        button->setEnabled(action->isEnabled());
        QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
        QObject::connect(action, &QAction::changed, button, [button, action] {
            button->setEnabled(action->isEnabled());
        });
        return button;
    };

    auto *fileGroup = new QGroupBox(QStringLiteral("Open & Save"), panel);
    auto *fileLayout = new QGridLayout(fileGroup);
    auto *addPhotosButton = new QPushButton(QStringLiteral("Add Photos…"), fileGroup);
    auto *importFolderButton = new QPushButton(QStringLiteral("Import Folder…"), fileGroup);
    connect(addPhotosButton, &QPushButton::clicked, this, &MainWindow::importPhotos);
    connect(importFolderButton, &QPushButton::clicked, this, &MainWindow::importFolder);
    fileLayout->addWidget(addPhotosButton, 0, 0);
    fileLayout->addWidget(importFolderButton, 0, 1);
    fileLayout->addWidget(actionButton(QStringLiteral("Save Edits"),
                                       m_saveEditsAction, fileGroup), 1, 0);
    fileLayout->addWidget(actionButton(QStringLiteral("Save File…"),
                                       m_saveCopyAction, fileGroup), 1, 1);
    fileTabLayout->insertWidget(0, fileGroup);

    auto *exportGroup = new QGroupBox(QStringLiteral("Export & Convert"), panel);
    auto *exportLayout = new QVBoxLayout(exportGroup);
    exportLayout->addWidget(actionButton(QStringLiteral("Export…"),
                                         m_exportAction, exportGroup));
    exportLayout->addWidget(actionButton(QStringLiteral("Convert Format…"),
                                         m_convertAction, exportGroup));
    auto *exportNote = new QLabel(QStringLiteral(
        "Save File and Export render the complete layer stack. Your source photo is never overwritten."),
        exportGroup);
    exportNote->setWordWrap(true);
    exportNote->setObjectName(QStringLiteral("mutedLabel"));
    exportLayout->addWidget(exportNote);
    fileTabLayout->insertWidget(1, exportGroup);

    auto *resetButton = new QPushButton(QStringLiteral("Reset Selected Layer"), panel);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetEdits);
    adjustTabLayout->addWidget(resetButton);
    layersTabLayout->addStretch();
    masksTabLayout->addStretch();
    adjustTabLayout->addStretch();
    toolsTabLayout->addStretch();
    filtersTabLayout->addStretch();
    fileTabLayout->addStretch();
    layout->addWidget(tabs, 1);

    scroll->setWidget(panel);
    dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::applyLibraryViewSettings()
{
    if (!m_libraryList || !m_libraryIconViewButton || !m_libraryIconSize)
        return;
    const bool iconView = m_libraryIconViewButton->isChecked();
    const int size = m_libraryIconSize->value();
    const int imageHeight = qRound(size * 0.67);
    m_libraryList->setViewMode(iconView ? QListView::IconMode : QListView::ListMode);
    m_libraryList->setFlow(iconView ? QListView::LeftToRight : QListView::TopToBottom);
    m_libraryList->setWrapping(iconView);
    m_libraryList->setWordWrap(iconView);
    m_libraryList->setIconSize(QSize(size, imageHeight));
    m_libraryList->setGridSize(iconView
        ? QSize(size + 24, imageHeight + 48) : QSize());
    m_libraryList->setSpacing(iconView ? 3 : 2);
    m_libraryIconSizeValue->setText(QStringLiteral("%1 px").arg(size));
    m_libraryList->doItemsLayout();

    QSettings settings;
    settings.setValue(QStringLiteral("library/view"),
                      iconView ? QStringLiteral("icons") : QStringLiteral("list"));
    settings.setValue(QStringLiteral("library/iconSize"), size);
}

AdjustmentControl *MainWindow::addAdjustment(QVBoxLayout *layout, const QString &label,
                                              double minimum, double maximum,
                                              double step, int decimals,
                                              AdjustmentId id)
{
    auto *control = new AdjustmentControl(label, minimum, maximum, step, decimals,
                                          static_cast<int>(id), this);
    m_controls.insert(static_cast<int>(id), control);
    connect(control, &AdjustmentControl::valueChanged, this,
            [this, id](double value, int mergeId) {
        applyAdjustment(id, value, mergeId);
    });
    layout->addWidget(control);
    return control;
}

void MainWindow::importPhotos()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add Photos"), {}, fileDialogFilter());
    addPaths(files);
}

void MainWindow::importFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Import Folder"));
    if (folder.isEmpty())
        return;
    QStringList files;
    QDirIterator iterator(folder, ImageLoader::supportedNameFilters(), QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
        files << iterator.next();
    addPaths(files);
}

void MainWindow::addPaths(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const int added = m_catalog.addFiles(paths);
    refreshLibrary();
    statusBar()->showMessage(QStringLiteral("Added %1 photo%2")
                                 .arg(added)
                                 .arg(added == 1 ? QString() : QStringLiteral("s")), 2500);
    if (added > 0 && !m_document.isOpen() && m_libraryList->count() > 0) {
        m_libraryList->setCurrentRow(0);
        openLibraryItem(m_libraryList->item(0));
    }
}

void MainWindow::refreshLibrary()
{
    const QString selectedPath = m_document.path();
    m_libraryList->clear();
    for (const auto &entry : m_catalog.entries()) {
        auto *item = new QListWidgetItem(captionForEntry(entry), m_libraryList);
        item->setData(Qt::UserRole, entry.path);
        item->setToolTip(entry.path);
        if (!QFileInfo::exists(entry.path))
            item->setForeground(QColor(QStringLiteral("#e87979")));
        if (entry.path == selectedPath)
            m_libraryList->setCurrentItem(item);
        queueThumbnail(entry.path);
    }
}

void MainWindow::queueThumbnail(const QString &path)
{
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path] {
        const QImage thumbnail = watcher->result();
        watcher->deleteLater();
        if (thumbnail.isNull())
            return;
        for (int i = 0; i < m_libraryList->count(); ++i) {
            auto *item = m_libraryList->item(i);
            if (item->data(Qt::UserRole).toString() == path) {
                item->setIcon(QPixmap::fromImage(thumbnail));
                break;
            }
        }
    });
    watcher->setFuture(QtConcurrent::run([path] { return ImageLoader::thumbnail(path); }));
}

void MainWindow::openLibraryItem(QListWidgetItem *item)
{
    if (item)
        openPath(item->data(Qt::UserRole).toString());
}

void MainWindow::openPath(const QString &path)
{
    deactivateRetouchTools();
    if (m_catalogSaveTimer->isActive() && m_document.isOpen()) {
        m_catalogSaveTimer->stop();
        m_catalog.updateLayers(m_document.path(), m_document.layerStack());
        updateItemCaption(m_document.path());
    }
    const auto *entry = m_catalog.find(path);
    if (!entry)
        return;
    QString error;
    if (!m_document.open(path, entry->layers, &error)) {
        QMessageBox::warning(this, QStringLiteral("Open Photo"),
                             QStringLiteral("Could not open %1.\n\n%2")
                                 .arg(QFileInfo(path).fileName(), error));
        return;
    }
    m_canvas->clearGuides();
    m_saveEditsAction->setEnabled(true);
    m_saveCopyAction->setEnabled(true);
    m_exportAction->setEnabled(true);
    m_convertAction->setEnabled(true);
    m_pickAction->setChecked(entry->picked);
    setWindowTitle(QStringLiteral("%1 — ChromaDesk %2")
        .arg(QFileInfo(path).fileName(),
             QString::fromLatin1(CHROMADESK_VERSION)));
}

void MainWindow::applyAdjustment(AdjustmentId id, double value, int mergeId)
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer)
        return;
    EditState &state = layer->edits;
    switch (id) {
    case Exposure: state.exposure = value; break;
    case Contrast: state.contrast = value; break;
    case Highlights: state.highlights = value; break;
    case Shadows: state.shadows = value; break;
    case Whites: state.whites = value; break;
    case Blacks: state.blacks = value; break;
    case Temperature: state.temperature = value; break;
    case Tint: state.tint = value; break;
    case Saturation: state.saturation = value; break;
    case Vibrance: state.vibrance = value; break;
    case ReplaceAmount: state.replacementAmount = value; break;
    case Clarity: state.clarity = value; break;
    case Blur: state.blur = value; break;
    }
    const QString description = m_controls.value(static_cast<int>(id))->findChild<QLabel *>()->text();
    const int layerMergeId = mergeId ^ static_cast<int>(qHash(layer->id) & 0x3fffffffU);
    m_document.setLayerStack(stack,
        QStringLiteral("%1 · %2").arg(layer->name, description), layerMergeId);
}

void MainWindow::syncControls(const EditState &state)
{
    m_controls.value(Exposure)->setValue(state.exposure);
    m_controls.value(Contrast)->setValue(state.contrast);
    m_controls.value(Highlights)->setValue(state.highlights);
    m_controls.value(Shadows)->setValue(state.shadows);
    m_controls.value(Whites)->setValue(state.whites);
    m_controls.value(Blacks)->setValue(state.blacks);
    m_controls.value(Temperature)->setValue(state.temperature);
    m_controls.value(Tint)->setValue(state.tint);
    m_controls.value(Saturation)->setValue(state.saturation);
    m_controls.value(Vibrance)->setValue(state.vibrance);
    m_controls.value(ReplaceAmount)->setValue(state.replacementAmount);
    m_controls.value(Clarity)->setValue(state.clarity);
    m_controls.value(Blur)->setValue(state.blur);
}

void MainWindow::refreshLayers()
{
    if (!m_layerList)
        return;
    m_syncingLayers = true;
    const QSignalBlocker listBlocker(m_layerList);
    const QSignalBlocker opacityBlocker(m_layerOpacity);
    m_layerList->clear();

    if (!m_document.isOpen()) {
        m_layerOpacity->setValue(100);
        m_layerOpacity->setEnabled(false);
        m_layerOpacityValue->setText(QStringLiteral("100%"));
        m_maskFeather->setValue(8);
        m_maskFeather->setEnabled(false);
        m_maskFeatherValue->setText(QStringLiteral("8%"));
        m_maskSummary->setText(QStringLiteral("No mask selected"));
        m_syncingLayers = false;
        return;
    }

    const auto &layers = m_document.layerStack().layers();
    for (int index = layers.size() - 1; index >= 0; --index) {
        const AdjustmentLayer &layer = layers.at(index);
        auto *item = new QListWidgetItem(layer.name, m_layerList);
        item->setData(Qt::UserRole, layer.id);
        item->setData(Qt::UserRole + 1, index);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
        if (index == 0) {
            item->setToolTip(QStringLiteral(
                "Base image adjustments and global crop/rotation"));
        } else if (layer.retouchOperation.isEnabled()) {
            item->setToolTip(QStringLiteral("%1 · non-destructive layer")
                .arg(retouchTypeName(layer.retouchOperation.type)));
        } else if (layer.textOverlay.isEnabled()) {
            item->setToolTip(QStringLiteral("Text overlay: %1 · double-click to rename")
                .arg(layer.textOverlay.text.left(80)));
        } else if (layer.watermarkOverlay.isEnabled()) {
            item->setToolTip(QStringLiteral("%1 watermark · drag on canvas to reposition")
                .arg(layer.watermarkOverlay.type == WatermarkType::Image
                    ? QStringLiteral("Image") : QStringLiteral("Text")));
        } else {
            item->setToolTip(QStringLiteral("%1 mask · double-click to rename")
                .arg(layerMaskTypeName(layer.mask.type)));
        }
        if (layer.id == m_document.activeLayerId())
            m_layerList->setCurrentItem(item);
    }

    const AdjustmentLayer *active = m_document.activeLayer();
    const int opacity = active ? qRound(active->opacity * 100.0) : 100;
    m_layerOpacity->setEnabled(active != nullptr);
    m_layerOpacity->setValue(opacity);
    m_layerOpacityValue->setText(QStringLiteral("%1%").arg(opacity));
    m_syncingLayers = false;
}

void MainWindow::syncActiveLayer()
{
    if (!m_document.isOpen()) {
        syncControls(EditState{});
        m_editSummary->setText(QStringLiteral("No layer selected"));
        m_maskSummary->setText(QStringLiteral("No mask selected"));
        m_canvas->setMovableWatermark({}, {}, false);
        updateActiveLayerOverlay(true);
        return;
    }
    const AdjustmentLayer *layer = m_document.activeLayer();
    if (!layer) {
        m_canvas->setActiveLayerOverlay({}, false);
        return;
    }
    syncControls(layer->edits);
    const QString layerSummary = layer->retouchOperation.isEnabled()
        ? retouchTypeName(layer->retouchOperation.type)
        : layer->watermarkOverlay.isEnabled()
        ? QStringLiteral("%1 watermark · %2% transparent")
              .arg(layer->watermarkOverlay.type == WatermarkType::Image
                       ? QStringLiteral("Image") : QStringLiteral("Text"))
              .arg(qRound((1.0 - layer->watermarkOverlay.opacity) * 100.0))
        : layer->textOverlay.isEnabled()
        ? QStringLiteral("Text · %1% size · %2")
              .arg(qRound(layer->textOverlay.size * 100.0))
              .arg(layer->textOverlay.fontFamily)
        : editStateSummary(layer->edits);
    m_editSummary->setText(QStringLiteral("%1 · %2")
        .arg(layer->name, layerSummary));

    m_syncingLayers = true;
    const QSignalBlocker blocker(m_layerOpacity);
    const int opacity = qRound(layer->opacity * 100.0);
    m_layerOpacity->setValue(opacity);
    m_layerOpacityValue->setText(QStringLiteral("%1%").arg(opacity));
    if (layer->retouchOperation.type == RetouchType::HealSpot
        || layer->retouchOperation.type == RetouchType::CloneStamp) {
        const QImage &reference = m_document.preview().isNull()
            ? m_document.original() : m_document.preview();
        const int shortEdge = qMax(1, qMin(reference.width(), reference.height()));
        const int brushPixels = qBound(m_retouchSize->minimum(),
            qRound(layer->retouchOperation.radius * shortEdge),
            m_retouchSize->maximum());
        const QSignalBlocker brushBlocker(m_retouchSize);
        m_retouchSize->setValue(brushPixels);
        m_retouchSizeValue->setText(QStringLiteral("%1 px").arg(brushPixels));
    }

    QString maskDescription = layer->retouchOperation.isEnabled()
        ? layer->retouchOperation.type == RetouchType::GeneratedImage
            ? QStringLiteral("Generative Fill · marked rectangle · cloud result")
            : QStringLiteral("%1 · processed locally")
                .arg(retouchTypeName(layer->retouchOperation.type))
        : layer->watermarkOverlay.isEnabled()
        ? QStringLiteral("Watermark · drag directly on the photo to reposition")
        : layer->textOverlay.isEnabled()
        ? QStringLiteral("Text overlay · use Tools → Edit Selected")
        : layerMaskTypeName(layer->mask.type);
    if (layer->mask.type == LayerMaskType::ColorRange) {
        maskDescription += QStringLiteral(" · %1 · tolerance %2%")
            .arg(layer->mask.targetColor.name(QColor::HexRgb).toUpper())
            .arg(qRound(layer->mask.colorTolerance * 100.0));
        if (layer->edits.replacementColor.isValid()
            && layer->edits.replacementAmount > 0.0) {
            maskDescription += QStringLiteral(" · replace with %1 at %2%")
                .arg(layer->edits.replacementColor.name(QColor::HexRgb).toUpper())
                .arg(qRound(layer->edits.replacementAmount));
        }
    } else if (layer->mask.type == LayerMaskType::LuminanceRange) {
        maskDescription += QStringLiteral(" · %1–%2%")
            .arg(qRound(layer->mask.luminanceMinimum * 100.0))
            .arg(qRound(layer->mask.luminanceMaximum * 100.0));
    } else if (layer->mask.type == LayerMaskType::AiSubject
               || layer->mask.type == LayerMaskType::AiBackground
               || layer->mask.type == LayerMaskType::AiSky) {
        maskDescription += QStringLiteral(" · %1")
            .arg(SmartMaskEngine::engineName(layer->mask.type));
    }
    m_maskSummary->setText(maskDescription);
    const QSignalBlocker featherBlocker(m_maskFeather);
    const int feather = qRound(layer->mask.feather * 100.0);
    m_maskFeather->setEnabled(!layer->mask.isGlobal());
    m_maskFeather->setValue(feather);
    m_maskFeatherValue->setText(QStringLiteral("%1%").arg(feather));
    for (int row = 0; row < m_layerList->count(); ++row) {
        QListWidgetItem *item = m_layerList->item(row);
        if (item->data(Qt::UserRole).toString() == layer->id) {
            const QSignalBlocker listBlocker(m_layerList);
            m_layerList->setCurrentItem(item);
            break;
        }
    }
    m_syncingLayers = false;
    if (layer->watermarkOverlay.isEnabled()) {
        const QSize imageSize = m_document.preview().isNull()
            ? m_document.original().size() : m_document.preview().size();
        const AdjustmentLayer *baseLayer = m_document.layerStack().baseLayer();
        const InnerImageMapping mapping = innerImageMapping(imageSize,
            baseLayer ? baseLayer->edits.borderSize : 0.0);
        const QPointF position(
            mapping.origin.x() + layer->watermarkOverlay.position.x()
                * mapping.scale.width(),
            mapping.origin.y() + layer->watermarkOverlay.position.y()
                * mapping.scale.height());
        const QSizeF innerSize = layer->watermarkOverlay.normalizedSize(
            mapping.pixelSize);
        m_canvas->setMovableWatermark(position,
            QSizeF(innerSize.width() * mapping.scale.width(),
                   innerSize.height() * mapping.scale.height()), layer->visible);
    } else {
        m_canvas->setMovableWatermark({}, {}, false);
    }

    if (const AdjustmentLayer *baseLayer = m_document.layerStack().baseLayer()) {
        const QSignalBlocker borderBlocker(m_borderSize);
        const int borderPercent = qRound(baseLayer->edits.borderSize * 100.0);
        m_borderSize->setValue(borderPercent);
        m_borderSizeValue->setText(QStringLiteral("%1%").arg(borderPercent));
        updateBorderButton(baseLayer->edits.borderColor);
    }
    updateActiveLayerOverlay();
}

void MainWindow::updateActiveLayerOverlay(bool force)
{
    const AdjustmentLayer *layer = m_document.activeLayer();
    const AdjustmentLayer *base = m_document.layerStack().baseLayer();
    if (!m_document.isOpen() || !layer || !base) {
        ++m_overlayGeneration;
        m_overlayInitialized = false;
        m_overlayLayerId.clear();
        m_canvas->setActiveLayerOverlay({}, false);
        return;
    }

    const EditState baseEdits = base->edits;
    if (!force && m_overlayInitialized && m_overlayLayerId == layer->id
        && m_overlayMask == layer->mask
        && m_overlayRetouchOperation == layer->retouchOperation
        && m_overlayBaseEdits == baseEdits) {
        m_canvas->setActiveLayerOverlayVisible(layer->visible);
        return;
    }

    ++m_overlayGeneration;
    const quint64 generation = m_overlayGeneration;
    m_overlayInitialized = true;
    m_overlayLayerId = layer->id;
    m_overlayMask = layer->mask;
    m_overlayRetouchOperation = layer->retouchOperation;
    m_overlayBaseEdits = baseEdits;

    if (layer->retouchOperation.type == RetouchType::HealSpot
        || layer->retouchOperation.type == RetouchType::CloneStamp) {
        EditState overlayEdits = baseEdits;
        overlayEdits.borderSize = 0.0;
        const QImage reference = ImageProcessor::render(
            m_document.original(), overlayEdits, 1000);
        QImage spot(reference.size(), QImage::Format_ARGB32);
        spot.fill(Qt::transparent);
        if (!spot.isNull()) {
            const QPointF target(
                layer->retouchOperation.target.x() * spot.width(),
                layer->retouchOperation.target.y() * spot.height());
            const double radius = qMax(2.0, qMin(spot.width(), spot.height())
                * layer->retouchOperation.radius);
            QPainter painter(&spot);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawEllipse(target, radius, radius);
        }
        m_canvas->setActiveLayerOverlay(spot, layer->visible);
        return;
    }

    if (layer->mask.isGlobal()) {
        m_canvas->setActiveLayerOverlay({}, layer->visible);
        return;
    }

    EditState overlayEdits = baseEdits;
    overlayEdits.borderSize = 0.0;
    const QImage reference = ImageProcessor::render(
        m_document.original(), overlayEdits, 1000);
    const QString layerId = layer->id;
    const LayerMask mask = layer->mask;
    m_canvas->setActiveLayerOverlay({}, false);
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, generation, layerId, mask] {
        const QImage overlay = watcher->result();
        watcher->deleteLater();
        const AdjustmentLayer *current = m_document.activeLayer();
        if (generation != m_overlayGeneration || !current
            || current->id != layerId || current->mask != mask)
            return;
        m_canvas->setActiveLayerOverlay(overlay, current->visible);
    });
    watcher->setFuture(QtConcurrent::run([reference, mask] {
        return SmartMaskEngine::generate(reference, mask);
    }));
}

void MainWindow::selectLayer(QListWidgetItem *current, QListWidgetItem *)
{
    if (m_syncingLayers || !current || !m_document.isOpen())
        return;
    m_document.setActiveLayer(current->data(Qt::UserRole).toString());
}

void MainWindow::updateLayerItem(QListWidgetItem *item)
{
    if (m_syncingLayers || !item || !m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(item->data(Qt::UserRole).toString());
    if (!layer)
        return;
    const QString previousName = layer->name;
    layer->name = item->text().trimmed().isEmpty()
        ? QStringLiteral("Untitled Layer") : item->text().trimmed();
    layer->visible = item->checkState() == Qt::Checked;
    const QString action = previousName == layer->name
        ? QStringLiteral("Toggle Layer: %1").arg(layer->name)
        : QStringLiteral("Rename Layer: %1").arg(layer->name);
    m_document.setLayerStack(stack, action);
}

void MainWindow::addLayer()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer();
    m_document.setLayerStack(stack, QStringLiteral("Add Adjustment Layer"));
    m_document.setActiveLayer(id);
}

void MainWindow::duplicateLayer()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = stack.duplicateLayer(m_document.activeLayerId());
    if (id.isEmpty())
        return;
    if (AdjustmentLayer *copy = stack.find(id)) {
        copy->edits.rotation = 0;
        copy->edits.flipHorizontal = false;
        copy->edits.flipVertical = false;
        copy->edits.crop = QRectF(0.0, 0.0, 1.0, 1.0);
    }
    m_document.setLayerStack(stack, QStringLiteral("Duplicate Adjustment Layer"));
    m_document.setActiveLayer(id);
}

void MainWindow::removeLayer()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = m_document.activeLayerId();
    const AdjustmentLayer *layer = stack.find(id);
    if (!layer)
        return;
    const QString name = layer->name;
    if (!stack.removeLayer(id)) {
        statusBar()->showMessage(QStringLiteral("The Base Adjustments layer cannot be deleted."),
                                 2500);
        return;
    }
    m_document.setLayerStack(stack, QStringLiteral("Delete Layer: %1").arg(name));
}

void MainWindow::moveLayerUp()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    if (stack.moveLayer(m_document.activeLayerId(), 1))
        m_document.setLayerStack(stack, QStringLiteral("Move Layer Up"));
}

void MainWindow::moveLayerDown()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    if (stack.moveLayer(m_document.activeLayerId(), -1))
        m_document.setLayerStack(stack, QStringLiteral("Move Layer Down"));
}

void MainWindow::setLayerOpacity(int value)
{
    m_layerOpacityValue->setText(QStringLiteral("%1%").arg(value));
    if (m_syncingLayers || !m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer)
        return;
    layer->opacity = static_cast<double>(value) / 100.0;
    const int mergeId = 9000 ^ static_cast<int>(qHash(layer->id) & 0x3fffffffU);
    m_document.setLayerStack(stack,
        QStringLiteral("%1 · Opacity").arg(layer->name), mergeId);
}

void MainWindow::setMaskFeather(int value)
{
    m_maskFeatherValue->setText(QStringLiteral("%1%").arg(value));
    if (m_syncingLayers || !m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer || layer->mask.isGlobal())
        return;
    layer->mask.feather = static_cast<double>(value) / 100.0;
    const int mergeId = 9200 ^ static_cast<int>(qHash(layer->id) & 0x3fffffffU);
    m_document.setLayerStack(stack,
        QStringLiteral("%1 · Mask Feather").arg(layer->name), mergeId);
}

void MainWindow::addMaskedLayer(const QString &name, const LayerMask &mask,
                                const EditState &edits)
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer(name);
    AdjustmentLayer *layer = stack.find(id);
    if (!layer)
        return;
    layer->mask = mask;
    layer->edits = edits;
    m_document.setLayerStack(stack, QStringLiteral("Add %1").arg(name));
    m_document.setActiveLayer(id);
}

void MainWindow::createAiSubjectMask()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiSubject;
    mask.feather = 0.08;
    addMaskedLayer(QStringLiteral("AI Subject"), mask);
    statusBar()->showMessage(QStringLiteral("AI Subject · %1")
        .arg(SmartMaskEngine::engineName(mask.type)), 4000);
}

void MainWindow::createAiBackgroundMask()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiBackground;
    mask.feather = 0.08;
    addMaskedLayer(QStringLiteral("AI Background"), mask);
    statusBar()->showMessage(QStringLiteral("AI Background · %1")
        .arg(SmartMaskEngine::engineName(mask.type)), 4000);
}

void MainWindow::createAiSkyMask()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiSky;
    mask.feather = 0.07;
    addMaskedLayer(QStringLiteral("AI Sky"), mask);
    statusBar()->showMessage(QStringLiteral("AI Sky · %1")
        .arg(SmartMaskEngine::engineName(mask.type)), 4000);
}

void MainWindow::createColorRangeMask()
{
    if (!m_document.isOpen())
        return;
    QColor initial(Qt::white);
    if (!m_document.preview().isNull()) {
        initial = m_document.preview().pixelColor(
            m_document.preview().width() / 2, m_document.preview().height() / 2);
    }
    const QColor selected = QColorDialog::getColor(initial, this,
        QStringLiteral("Select the color to isolate"), QColorDialog::ShowAlphaChannel);
    if (!selected.isValid())
        return;
    bool accepted = false;
    const int tolerance = QInputDialog::getInt(this, QStringLiteral("Color Range"),
        QStringLiteral("Color tolerance (%)"), 18, 1, 100, 1, &accepted);
    if (!accepted)
        return;
    LayerMask mask;
    mask.type = LayerMaskType::ColorRange;
    mask.targetColor = selected;
    mask.colorTolerance = static_cast<double>(tolerance) / 100.0;
    mask.feather = 0.06;
    addMaskedLayer(QStringLiteral("Color %1")
        .arg(selected.name(QColor::HexRgb).toUpper()), mask);
}

void MainWindow::createLuminanceRangeMask()
{
    if (!m_document.isOpen())
        return;
    bool accepted = false;
    const int minimum = QInputDialog::getInt(this, QStringLiteral("Luminance Range"),
        QStringLiteral("Minimum luminance (%)"), 25, 0, 100, 1, &accepted);
    if (!accepted)
        return;
    const int maximum = QInputDialog::getInt(this, QStringLiteral("Luminance Range"),
        QStringLiteral("Maximum luminance (%)"), 75, minimum, 100, 1, &accepted);
    if (!accepted)
        return;
    LayerMask mask;
    mask.type = LayerMaskType::LuminanceRange;
    mask.luminanceMinimum = static_cast<double>(minimum) / 100.0;
    mask.luminanceMaximum = static_cast<double>(maximum) / 100.0;
    mask.feather = 0.06;
    addMaskedLayer(QStringLiteral("Luminance %1–%2").arg(minimum).arg(maximum), mask);
}

void MainWindow::editSelectedMask()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer)
        return;

    if (layer->mask.type == LayerMaskType::ColorRange) {
        const QColor selected = QColorDialog::getColor(layer->mask.targetColor, this,
            QStringLiteral("Edit isolated color"));
        if (!selected.isValid())
            return;
        bool accepted = false;
        const int tolerance = QInputDialog::getInt(this, QStringLiteral("Color Range"),
            QStringLiteral("Color tolerance (%)"),
            qRound(layer->mask.colorTolerance * 100.0), 1, 100, 1, &accepted);
        if (!accepted)
            return;
        QColor replacement = layer->edits.replacementColor;
        int replacementAmount = qRound(layer->edits.replacementAmount);
        if (replacement.isValid()) {
            replacement = QColorDialog::getColor(replacement, this,
                QStringLiteral("Edit replacement color"));
            if (!replacement.isValid())
                return;
            replacementAmount = QInputDialog::getInt(this,
                QStringLiteral("Replace Color"), QStringLiteral("Replacement strength (%)"),
                replacementAmount, 1, 100, 1, &accepted);
            if (!accepted)
                return;
        }
        layer->mask.targetColor = selected;
        layer->mask.colorTolerance = static_cast<double>(tolerance) / 100.0;
        if (replacement.isValid()) {
            layer->edits.replacementColor = replacement;
            layer->edits.replacementAmount = replacementAmount;
        }
        m_document.setLayerStack(stack,
            QStringLiteral("Edit Color Mask: %1").arg(layer->name));
        return;
    }

    if (layer->mask.type == LayerMaskType::LuminanceRange) {
        bool accepted = false;
        const int minimum = QInputDialog::getInt(this, QStringLiteral("Luminance Range"),
            QStringLiteral("Minimum luminance (%)"),
            qRound(layer->mask.luminanceMinimum * 100.0), 0, 100, 1, &accepted);
        if (!accepted)
            return;
        const int maximum = QInputDialog::getInt(this, QStringLiteral("Luminance Range"),
            QStringLiteral("Maximum luminance (%)"),
            qMax(minimum, qRound(layer->mask.luminanceMaximum * 100.0)),
            minimum, 100, 1, &accepted);
        if (!accepted)
            return;
        layer->mask.luminanceMinimum = static_cast<double>(minimum) / 100.0;
        layer->mask.luminanceMaximum = static_cast<double>(maximum) / 100.0;
        m_document.setLayerStack(stack,
            QStringLiteral("Edit Luminance Mask: %1").arg(layer->name));
        return;
    }

    if (layer->mask.type == LayerMaskType::Rectangle
        || layer->mask.type == LayerMaskType::Ellipse) {
        deactivateRetouchTools();
        m_pendingShapeLayerId = layer->id;
        m_cropAction->setChecked(false);
        m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
        m_canvas->setShapeMaskMode(layer->mask.type == LayerMaskType::Ellipse
            ? ImageCanvas::ShapeMaskMode::Ellipse
            : ImageCanvas::ShapeMaskMode::Rectangle);
        statusBar()->showMessage(QStringLiteral("Drag to replace the selected shape mask."));
        return;
    }

    if (layer->mask.type == LayerMaskType::AiSubject
        || layer->mask.type == LayerMaskType::AiBackground
        || layer->mask.type == LayerMaskType::AiSky) {
        statusBar()->showMessage(
            QStringLiteral("Use Feather to refine the AI mask edge."), 3000);
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("This layer affects the entire image. Add a mask first."), 3000);
}

void MainWindow::beginRectangleMask()
{
    if (!m_document.isOpen())
        return;
    deactivateRetouchTools();
    m_cropAction->setChecked(false);
    m_pendingShapeLayerId.clear();
    m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    m_canvas->setShapeMaskMode(ImageCanvas::ShapeMaskMode::Rectangle);
    statusBar()->showMessage(QStringLiteral("Drag a rectangle over the area to mask."));
}

void MainWindow::deactivateRetouchTools()
{
    if (m_dustRemovalButton) {
        const QSignalBlocker blocker(m_dustRemovalButton);
        m_dustRemovalButton->setChecked(false);
    }
    if (m_cloneStampButton) {
        const QSignalBlocker blocker(m_cloneStampButton);
        m_cloneStampButton->setChecked(false);
    }
    m_canvas->setRetouchMode(ImageCanvas::RetouchMode::None);
}

double MainWindow::retouchRadius() const
{
    const QImage &reference = m_document.preview().isNull()
        ? m_document.original() : m_document.preview();
    const int shortEdge = qMax(1, qMin(reference.width(), reference.height()));
    const int pixels = m_retouchSize ? m_retouchSize->value() : 24;
    return qBound(0.002, static_cast<double>(pixels) / shortEdge, 0.35);
}

void MainWindow::toggleDustRemoval(bool enabled)
{
    if (enabled && !m_document.isOpen()) {
        const QSignalBlocker blocker(m_dustRemovalButton);
        m_dustRemovalButton->setChecked(false);
        return;
    }
    if (enabled) {
        const QSignalBlocker blocker(m_cloneStampButton);
        m_cloneStampButton->setChecked(false);
        m_cropAction->setChecked(false);
        m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
        m_canvas->setRetouchMode(ImageCanvas::RetouchMode::DustHeal);
        statusBar()->showMessage(QStringLiteral(
            "Click each dust spot or small blemish. Every repair becomes a layer."));
    } else if (!m_cloneStampButton->isChecked()) {
        m_canvas->setRetouchMode(ImageCanvas::RetouchMode::None);
    }
}

void MainWindow::toggleCloneStamp(bool enabled)
{
    if (enabled && !m_document.isOpen()) {
        const QSignalBlocker blocker(m_cloneStampButton);
        m_cloneStampButton->setChecked(false);
        return;
    }
    if (enabled) {
        const QSignalBlocker blocker(m_dustRemovalButton);
        m_dustRemovalButton->setChecked(false);
        m_cropAction->setChecked(false);
        m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
        m_canvas->setRetouchMode(ImageCanvas::RetouchMode::CloneStamp);
        statusBar()->showMessage(QStringLiteral(
            "Click the source texture, then click where it should be cloned."));
    } else if (!m_dustRemovalButton->isChecked()) {
        m_canvas->setRetouchMode(ImageCanvas::RetouchMode::None);
    }
}

void MainWindow::addDustRemoval(const QPointF &target)
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer(QStringLiteral("Dust Heal"));
    AdjustmentLayer *layer = stack.find(id);
    layer->retouchOperation.type = RetouchType::HealSpot;
    layer->retouchOperation.target = target;
    layer->retouchOperation.radius = retouchRadius();
    m_document.setLayerStack(stack, QStringLiteral("Remove Dust Spot"));
    m_document.setActiveLayer(id);
    statusBar()->showMessage(QStringLiteral(
        "Dust spot healed locally. Click another spot or turn the tool off."), 2600);
}

void MainWindow::addCloneStamp(const QPointF &source, const QPointF &target)
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer(QStringLiteral("Clone Stamp"));
    AdjustmentLayer *layer = stack.find(id);
    layer->retouchOperation.type = RetouchType::CloneStamp;
    layer->retouchOperation.source = source;
    layer->retouchOperation.target = target;
    layer->retouchOperation.radius = retouchRadius();
    m_document.setLayerStack(stack, QStringLiteral("Clone Image Area"));
    m_document.setActiveLayer(id);
    statusBar()->showMessage(QStringLiteral(
        "Clone stamp added. Click a new source to continue."), 2600);
}

void MainWindow::beginGenerativeFill()
{
    if (!m_document.isOpen())
        return;
    deactivateRetouchTools();
    m_cropAction->setChecked(false);
    m_pendingShapeLayerId.clear();
    m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    m_canvas->setShapeMaskMode(ImageCanvas::ShapeMaskMode::GenerativeFill);
    statusBar()->showMessage(QStringLiteral(
        "Drag a rectangle around the area that Generative Fill may replace."));
}

void MainWindow::requestGenerativeFill(const QRectF &bounds)
{
    if (!m_document.isOpen())
        return;

    if (!m_generativeFillConsent) {
        QMessageBox notice(this);
        notice.setIcon(QMessageBox::Information);
        notice.setWindowTitle(QStringLiteral("Generative Fill privacy notice"));
        notice.setText(QStringLiteral("Send this edit to OpenAI?"));
        notice.setInformativeText(QStringLiteral(
            "ChromaDesk will send a resized copy of the current edited photo, a "
            "transparent mask for the rectangle you marked, and the prompt you enter "
            "to OpenAI's gpt-image-2 model. OpenAI API usage may be billed. Your API "
            "key is kept in memory for this ChromaDesk session and is not written to "
            "the catalog or preferences."));
        notice.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        notice.setDefaultButton(QMessageBox::Cancel);
        if (notice.exec() != QMessageBox::Ok) {
            statusBar()->showMessage(QStringLiteral("Generative Fill cancelled."), 2200);
            return;
        }
        m_generativeFillConsent = true;
    }

    bool accepted = false;
    const QString prompt = QInputDialog::getMultiLineText(this,
        QStringLiteral("Generative Fill"),
        QStringLiteral("Describe what should appear inside the marked area:"),
        {}, &accepted).trimmed();
    if (!accepted || prompt.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Generative Fill cancelled."), 2200);
        return;
    }

    if (m_openAiApiKey.isEmpty())
        m_openAiApiKey = qEnvironmentVariable("OPENAI_API_KEY").trimmed();
    if (m_openAiApiKey.isEmpty()) {
        m_openAiApiKey = QInputDialog::getText(this,
            QStringLiteral("OpenAI API key"),
            QStringLiteral("Enter an OpenAI API key (kept only for this session):"),
            QLineEdit::Password, {}, &accepted).trimmed();
        if (!accepted || m_openAiApiKey.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Generative Fill cancelled."), 2200);
            return;
        }
    }

    LayerStack requestStack = m_document.layerStack();
    if (AdjustmentLayer *base = requestStack.baseLayer())
        base->edits.borderSize = 0.0;
    const QImage requestImage = ImageProcessor::renderLayers(
        m_document.original(), requestStack, 1536)
        .convertToFormat(QImage::Format_RGBA8888);
    if (requestImage.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Generative Fill"),
                             QStringLiteral("The current photo could not be prepared."));
        return;
    }

    QImage mask(requestImage.size(), QImage::Format_ARGB32);
    mask.fill(qRgba(255, 255, 255, 255));
    const QRectF normalized = bounds.normalized().intersected(
        QRectF(0.0, 0.0, 1.0, 1.0));
    const QRect editRect(
        qBound(0, qFloor(normalized.left() * requestImage.width()),
               requestImage.width() - 1),
        qBound(0, qFloor(normalized.top() * requestImage.height()),
               requestImage.height() - 1),
        qMax(1, qCeil(normalized.width() * requestImage.width())),
        qMax(1, qCeil(normalized.height() * requestImage.height())));
    {
        QPainter painter(&mask);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(editRect.intersected(mask.rect()), Qt::transparent);
    }

    m_pendingGenerativeBounds = normalized;
    m_pendingGenerativePrompt = prompt;
    m_generativeFillButton->setEnabled(false);
    m_generativeFillProgress = new QProgressDialog(
        QStringLiteral("Creating Generative Fill…"), {}, 0, 0, this);
    m_generativeFillProgress->setWindowTitle(QStringLiteral("ChromaDesk"));
    m_generativeFillProgress->setWindowModality(Qt::WindowModal);
    m_generativeFillProgress->setCancelButton(nullptr);
    m_generativeFillProgress->setMinimumDuration(0);
    m_generativeFillProgress->show();

    const QString protectedPrompt = QStringLiteral(
        "%1\n\nEdit only the transparent masked area. Preserve the composition, "
        "identity, lighting, texture, and pixels outside the mask.").arg(prompt);
    m_generativeFillClient->requestFill(m_openAiApiKey, requestImage, mask,
                                         protectedPrompt);
}

void MainWindow::finishGenerativeFill(const QImage &image,
                                      const QString &errorMessage)
{
    if (m_generativeFillProgress) {
        m_generativeFillProgress->close();
        m_generativeFillProgress->deleteLater();
        m_generativeFillProgress = nullptr;
    }
    if (m_generativeFillButton)
        m_generativeFillButton->setEnabled(true);
    if (!errorMessage.isEmpty() || image.isNull()) {
        if (!m_openAiApiKey.isEmpty()) {
            m_openAiApiKey.fill(QChar(u'\0'));
            m_openAiApiKey.clear();
        }
        QMessageBox::warning(this, QStringLiteral("Generative Fill"),
            errorMessage.isEmpty()
                ? QStringLiteral("No generated image was returned.") : errorMessage);
        return;
    }

    QDir outputDirectory(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/generative-fill"));
    if (!outputDirectory.exists() && !outputDirectory.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, QStringLiteral("Generative Fill"),
            QStringLiteral("The generated result could not be stored."));
        return;
    }
    const QString path = outputDirectory.filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".png"));
    QImageWriter writer(path, "png");
    if (!writer.write(image)) {
        QMessageBox::warning(this, QStringLiteral("Generative Fill"),
            QStringLiteral("The generated result could not be stored: %1")
                .arg(writer.errorString()));
        return;
    }

    LayerStack stack = m_document.layerStack();
    const QString name = QStringLiteral("Generative Fill · %1")
        .arg(m_pendingGenerativePrompt.simplified().left(36));
    const QString id = stack.addLayer(name);
    AdjustmentLayer *layer = stack.find(id);
    layer->mask.type = LayerMaskType::Rectangle;
    layer->mask.bounds = m_pendingGenerativeBounds;
    layer->mask.feather = 0.04;
    layer->retouchOperation.type = RetouchType::GeneratedImage;
    layer->retouchOperation.generatedImagePath = path;
    m_document.setLayerStack(stack, QStringLiteral("Add Generative Fill"));
    m_document.setActiveLayer(id);
    statusBar()->showMessage(QStringLiteral(
        "Generative Fill added as a non-destructive layer."), 3500);
}

void MainWindow::beginEllipseMask()
{
    if (!m_document.isOpen())
        return;
    deactivateRetouchTools();
    m_cropAction->setChecked(false);
    m_pendingShapeLayerId.clear();
    m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    m_canvas->setShapeMaskMode(ImageCanvas::ShapeMaskMode::Ellipse);
    statusBar()->showMessage(QStringLiteral("Drag an ellipse over the area to mask."));
}

void MainWindow::completeShapeMask(const QRectF &bounds,
                                   ImageCanvas::ShapeMaskMode mode)
{
    if (mode == ImageCanvas::ShapeMaskMode::GenerativeFill) {
        requestGenerativeFill(bounds);
        return;
    }
    LayerMask mask;
    mask.type = mode == ImageCanvas::ShapeMaskMode::Ellipse
        ? LayerMaskType::Ellipse : LayerMaskType::Rectangle;
    mask.bounds = bounds;
    mask.feather = 0.08;
    if (!m_pendingShapeLayerId.isEmpty() && m_document.isOpen()) {
        LayerStack stack = m_document.layerStack();
        if (AdjustmentLayer *layer = stack.find(m_pendingShapeLayerId)) {
            mask.feather = layer->mask.feather;
            layer->mask = mask;
            m_document.setLayerStack(stack,
                QStringLiteral("Edit Shape Mask: %1").arg(layer->name));
            m_document.setActiveLayer(layer->id);
        }
        m_pendingShapeLayerId.clear();
        statusBar()->showMessage(QStringLiteral("Shape mask updated."), 2500);
    } else {
        addMaskedLayer(layerMaskTypeName(mask.type), mask);
        statusBar()->showMessage(QStringLiteral("Shape mask added. Adjust the layer values."),
                                 2500);
    }
}

void MainWindow::applyAiAutoEnhance()
{
    if (!m_document.isOpen())
        return;
    const AdjustmentLayer *base = m_document.layerStack().baseLayer();
    const QImage analysisImage = ImageProcessor::orientedAndCropped(
        m_document.original(), base ? base->edits : EditState{}, 640);
    addMaskedLayer(QStringLiteral("AI Auto Enhance"), {},
                   ImageProcessor::suggestAutoEnhance(analysisImage));
}

void MainWindow::applyAiSubjectPop()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiSubject;
    mask.feather = 0.09;
    EditState edits;
    edits.exposure = 0.35;
    edits.shadows = 18.0;
    edits.vibrance = 20.0;
    edits.clarity = 14.0;
    addMaskedLayer(QStringLiteral("AI Subject Pop"), mask, edits);
}

void MainWindow::applyAiBackgroundBlur()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiBackground;
    mask.feather = 0.10;
    EditState edits;
    edits.blur = 34.0;
    edits.saturation = -8.0;
    addMaskedLayer(QStringLiteral("AI Background Blur"), mask, edits);
}

void MainWindow::applyAiMonochromeBackground()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiBackground;
    mask.feather = 0.09;
    EditState edits;
    edits.saturation = -100.0;
    edits.contrast = -6.0;
    addMaskedLayer(QStringLiteral("AI Color Subject"), mask, edits);
}

void MainWindow::applyAiEnhanceSky()
{
    LayerMask mask;
    mask.type = LayerMaskType::AiSky;
    mask.feather = 0.08;
    EditState edits;
    edits.highlights = -22.0;
    edits.whites = 10.0;
    edits.contrast = 16.0;
    edits.vibrance = 28.0;
    edits.saturation = 8.0;
    edits.clarity = 12.0;
    edits.temperature = -5.0;
    addMaskedLayer(QStringLiteral("AI Enhance Sky"), mask, edits);
}

void MainWindow::applyAiReplaceColor()
{
    if (!m_document.isOpen())
        return;
    QColor initial(Qt::white);
    if (!m_document.preview().isNull()) {
        initial = m_document.preview().pixelColor(
            m_document.preview().width() / 2, m_document.preview().height() / 2);
    }
    const QColor sourceColor = QColorDialog::getColor(initial, this,
        QStringLiteral("Select the color to replace"));
    if (!sourceColor.isValid())
        return;

    bool accepted = false;
    const int tolerance = QInputDialog::getInt(this, QStringLiteral("Replace Color"),
        QStringLiteral("Source color tolerance (%)"), 18, 1, 100, 1, &accepted);
    if (!accepted)
        return;

    const QColor replacementColor = QColorDialog::getColor(sourceColor, this,
        QStringLiteral("Select the replacement color"));
    if (!replacementColor.isValid())
        return;
    const int strength = QInputDialog::getInt(this, QStringLiteral("Replace Color"),
        QStringLiteral("Replacement strength (%)"), 100, 1, 100, 1, &accepted);
    if (!accepted)
        return;

    LayerMask mask;
    mask.type = LayerMaskType::ColorRange;
    mask.targetColor = sourceColor;
    mask.colorTolerance = static_cast<double>(tolerance) / 100.0;
    mask.feather = 0.06;
    EditState edits;
    edits.replacementColor = replacementColor;
    edits.replacementAmount = strength;
    addMaskedLayer(QStringLiteral("Replace %1 → %2")
        .arg(sourceColor.name(QColor::HexRgb).toUpper(),
             replacementColor.name(QColor::HexRgb).toUpper()), mask, edits);
}

void MainWindow::addTextLayer()
{
    if (!m_document.isOpen())
        return;
    TextOverlayDialog dialog(TextOverlay{}, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const TextOverlay overlay = dialog.overlay();
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer(QStringLiteral("Text · %1")
        .arg(overlay.text.left(24)));
    if (AdjustmentLayer *layer = stack.find(id))
        layer->textOverlay = overlay;
    m_document.setLayerStack(stack, QStringLiteral("Add Text Layer"));
    m_document.setActiveLayer(id);
}

void MainWindow::editTextLayer()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer || !layer->textOverlay.isEnabled()) {
        statusBar()->showMessage(
            QStringLiteral("Select a text layer in the Layers tab first."), 3000);
        return;
    }
    TextOverlayDialog dialog(layer->textOverlay, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    layer->textOverlay = dialog.overlay();
    m_document.setLayerStack(stack,
        QStringLiteral("Edit Text Layer: %1").arg(layer->name));
}

void MainWindow::addWatermark()
{
    if (!m_document.isOpen())
        return;
    WatermarkDialog dialog(WatermarkOverlay{}, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const WatermarkOverlay overlay = dialog.overlay();
    const QString label = overlay.type == WatermarkType::Image
        ? QFileInfo(overlay.imagePath).completeBaseName()
        : overlay.text.left(24);
    LayerStack stack = m_document.layerStack();
    const QString id = stack.addLayer(QStringLiteral("Watermark · %1").arg(label));
    if (AdjustmentLayer *layer = stack.find(id))
        layer->watermarkOverlay = overlay;
    m_document.setLayerStack(stack, QStringLiteral("Add Watermark Layer"));
    m_document.setActiveLayer(id);
}

void MainWindow::editWatermark()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer || !layer->watermarkOverlay.isEnabled()) {
        statusBar()->showMessage(
            QStringLiteral("Select a watermark layer in the Layers tab first."), 3000);
        return;
    }
    WatermarkDialog dialog(layer->watermarkOverlay, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    layer->watermarkOverlay = dialog.overlay();
    m_document.setLayerStack(stack,
        QStringLiteral("Edit Watermark: %1").arg(layer->name));
}

void MainWindow::moveSelectedWatermark(const QPointF &position)
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer || !layer->watermarkOverlay.isEnabled())
        return;
    const QSize imageSize = m_document.preview().isNull()
        ? m_document.original().size() : m_document.preview().size();
    const AdjustmentLayer *base = stack.baseLayer();
    const InnerImageMapping mapping = innerImageMapping(imageSize,
        base ? base->edits.borderSize : 0.0);
    layer->watermarkOverlay.position = QPointF(
        qBound(0.0, (position.x() - mapping.origin.x())
            / qMax(0.0001, mapping.scale.width()), 1.0),
        qBound(0.0, (position.y() - mapping.origin.y())
            / qMax(0.0001, mapping.scale.height()), 1.0));
    m_document.setLayerStack(stack,
        QStringLiteral("Move Watermark: %1").arg(layer->name));
}

void MainWindow::setBorderSize(int value)
{
    m_borderSizeValue->setText(QStringLiteral("%1%").arg(value));
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *base = stack.baseLayer();
    if (!base)
        return;
    base->edits.borderSize = static_cast<double>(value) / 100.0;
    m_document.setLayerStack(stack, QStringLiteral("Image Border Width"), 9801);
}

void MainWindow::chooseBorderColor()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *base = stack.baseLayer();
    if (!base)
        return;
    const QColor selected = QColorDialog::getColor(base->edits.borderColor,
        this, QStringLiteral("Choose Image Border Color"));
    if (!selected.isValid())
        return;
    base->edits.borderColor = selected;
    updateBorderButton(selected);
    m_document.setLayerStack(stack, QStringLiteral("Image Border Color"));
}

void MainWindow::updateBorderButton(const QColor &color)
{
    if (!m_borderColorButton)
        return;
    const QColor safeColor = color.isValid() ? color : QColor(Qt::white);
    const QColor textColor = safeColor.lightnessF() > 0.58
        ? QColor(QStringLiteral("#111318")) : QColor(Qt::white);
    m_borderColorButton->setText(safeColor.name(QColor::HexRgb).toUpper());
    m_borderColorButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid #777; }")
        .arg(safeColor.name(QColor::HexRgb), textColor.name(QColor::HexRgb)));
}

void MainWindow::preparePassportSheet()
{
    if (!m_document.isOpen())
        return;
    const bool useA4 = m_passportSheetSize && m_passportSheetSize->currentData().toInt() == 1;
    const PassportSheetOptions sheetOptions = useA4
        ? PassportSheetOptions::a4() : PassportSheetOptions::fourBySix();
    const QFileInfo sourceInfo(m_document.path());
    const QString suggested = sourceInfo.dir().filePath(
        sourceInfo.completeBaseName() + (useA4
            ? QStringLiteral("-passport-A4.png")
            : QStringLiteral("-passport-4x6.png")));
    const QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("Save Passport Photo Grid"), suggested,
        QStringLiteral("PNG image (*.png)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).absoluteFilePath() == sourceInfo.absoluteFilePath()) {
        QMessageBox::warning(this, QStringLiteral("Protect Source Photo"),
            QStringLiteral("Choose a different file name. ChromaDesk never overwrites the source photo."));
        return;
    }
    if (QFileInfo::exists(path)) {
        const auto answer = QMessageBox::question(this, QStringLiteral("Replace File?"),
            QStringLiteral("%1 already exists. Replace it?").arg(QFileInfo(path).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QImage source = m_document.original();
    const LayerStack layers = m_document.layerStack();
    const int expectedCopies = useA4 ? 25 : 6;
    auto *progress = new QProgressDialog(
        QStringLiteral("Preparing 300 DPI passport photo grid…"), {}, 0, 0, this);
    progress->setWindowTitle(QStringLiteral("Passport Photo Grid"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setCancelButton(nullptr);
    progress->setMinimumDuration(0);
    progress->show();

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, progress, path, expectedCopies] {
        const QString error = watcher->result();
        watcher->deleteLater();
        progress->close();
        progress->deleteLater();
        if (!error.isEmpty()) {
            QMessageBox::critical(this, QStringLiteral("Passport Grid Failed"), error);
            return;
        }
        statusBar()->showMessage(QStringLiteral("Created %1 passport photos in %2")
            .arg(expectedCopies).arg(QFileInfo(path).fileName()), 5000);
    });
    watcher->setFuture(QtConcurrent::run([source, layers, sheetOptions, path] {
        const QImage rendered = ImageProcessor::renderLayers(source, layers, 0);
        if (rendered.isNull())
            return QStringLiteral("The edited image could not be rendered.");
        int copies = 0;
        const QImage sheet = PassportSheet::create(rendered, sheetOptions, &copies);
        if (sheet.isNull() || copies <= 0)
            return QStringLiteral("The selected paper size could not hold a passport photo grid.");
        QImageWriter writer(path, QByteArray("png"));
        if (!writer.write(sheet))
            return writer.errorString();
        return QString{};
    }));
}

void MainWindow::updateDocumentPresentation()
{
    if (!m_document.isOpen())
        return;
    const AdjustmentLayer *base = m_document.layerStack().baseLayer();
    QImage baseline = ImageProcessor::orientedAndCropped(
        m_document.original(), base ? base->edits : EditState{}, 2200);
    if (base)
        baseline = ImageProcessor::withBorder(baseline, base->edits, 2200);
    m_canvas->setImages(baseline, m_document.preview());
    m_histogram->setImage(m_document.preview());
    const QFileInfo info(m_document.path());
    m_fileInfo->setText(QStringLiteral("%1\n%2 × %3 px · %4 MB")
        .arg(info.fileName())
        .arg(m_document.original().width())
        .arg(m_document.original().height())
        .arg(static_cast<double>(info.size()) / (1024.0 * 1024.0), 0, 'f', 1));
    syncActiveLayer();
}

void MainWindow::resetEdits()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer)
        return;
    EditState reset;
    if (stack.indexOf(layer->id) == 0) {
        reset.rotation = layer->edits.rotation;
        reset.flipHorizontal = layer->edits.flipHorizontal;
        reset.flipVertical = layer->edits.flipVertical;
        reset.crop = layer->edits.crop;
    }
    layer->edits = reset;
    m_document.setLayerStack(stack,
        QStringLiteral("Reset Layer: %1").arg(layer->name));
}

void MainWindow::rotateClockwise()
{
    if (!m_document.isOpen())
        return;
    LayerStack stack = m_document.layerStack();
    EditState &state = stack.baseLayer()->edits;
    state.rotation = (state.rotation + 90) % 360;
    state.crop = QRectF(0.0, 0.0, 1.0, 1.0);
    m_document.setLayerStack(stack, QStringLiteral("Rotate Clockwise"));
}

void MainWindow::toggleCrop(bool enabled)
{
    if (enabled && !m_document.isOpen()) {
        m_cropAction->setChecked(false);
        return;
    }
    if (enabled) {
        deactivateRetouchTools();
        m_originalAction->setChecked(false);
        m_sideBySideAction->setChecked(false);
        m_splitAction->setChecked(false);
        m_canvas->setCompareMode(ImageCanvas::CompareMode::Edited);
    }
    m_canvas->setCropMode(enabled);
}

void MainWindow::applyPreset()
{
    if (!m_document.isOpen())
        return;
    const int index = m_presetCombo->currentIndex();
    if (index < 0 || index >= PresetRepository::builtIns().size())
        return;
    LayerStack stack = m_document.layerStack();
    AdjustmentLayer *layer = stack.find(m_document.activeLayerId());
    if (!layer)
        return;
    EditState preset = PresetRepository::builtIns().at(index).edits;
    if (stack.indexOf(layer->id) == 0) {
        preset.rotation = layer->edits.rotation;
        preset.flipHorizontal = layer->edits.flipHorizontal;
        preset.flipVertical = layer->edits.flipVertical;
        preset.crop = layer->edits.crop;
    }
    layer->edits = preset;
    m_document.setLayerStack(stack,
        QStringLiteral("%1 · Preset: %2")
            .arg(layer->name, PresetRepository::builtIns().at(index).name));
}

void MainWindow::refreshCreativeFilters()
{
    if (!m_filterCategory || !m_filterList)
        return;
    const QString category = m_filterCategory->currentData().toString();
    m_filterList->clear();
    const auto &filters = PresetRepository::creativeFilters();
    for (int index = 0; index < filters.size(); ++index) {
        const auto &filter = filters.at(index);
        if (!category.isEmpty() && filter.category != category)
            continue;
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(filter.name, filter.description), m_filterList);
        item->setData(Qt::UserRole, index);
        item->setToolTip(QStringLiteral("%1 · %2").arg(filter.category, filter.description));
    }
    if (m_filterList->count() > 0)
        m_filterList->setCurrentRow(0);
}

void MainWindow::applyCreativeFilter()
{
    if (!m_document.isOpen()) {
        statusBar()->showMessage(QStringLiteral("Open a photo before applying a filter"), 2500);
        return;
    }
    const QListWidgetItem *item = m_filterList ? m_filterList->currentItem() : nullptr;
    if (!item)
        return;
    const int index = item->data(Qt::UserRole).toInt();
    const auto &filters = PresetRepository::creativeFilters();
    if (index < 0 || index >= filters.size())
        return;
    const auto &filter = filters.at(index);
    addMaskedLayer(QStringLiteral("Filter · %1").arg(filter.name), {}, filter.edits);
    statusBar()->showMessage(
        QStringLiteral("Applied %1 as an editable layer").arg(filter.name), 2500);
}

void MainWindow::setRating(int rating)
{
    if (!m_document.isOpen())
        return;
    m_catalog.setRating(m_document.path(), rating);
    updateItemCaption(m_document.path());
}

void MainWindow::togglePicked()
{
    if (!m_document.isOpen())
        return;
    const auto *entry = m_catalog.find(m_document.path());
    if (!entry)
        return;
    const bool picked = !entry->picked;
    m_catalog.setPicked(m_document.path(), picked);
    m_pickAction->setChecked(picked);
    updateItemCaption(m_document.path());
}

void MainWindow::updateItemCaption(const QString &path)
{
    const auto *entry = m_catalog.find(path);
    if (!entry)
        return;
    for (int i = 0; i < m_libraryList->count(); ++i) {
        auto *item = m_libraryList->item(i);
        if (item->data(Qt::UserRole).toString() == path) {
            item->setText(captionForEntry(*entry));
            break;
        }
    }
}

void MainWindow::saveEdits()
{
    if (!m_document.isOpen())
        return;
    m_catalogSaveTimer->stop();
    m_catalog.updateLayers(m_document.path(), m_document.layerStack());
    m_document.undoStack()->setClean();
    updateItemCaption(m_document.path());
    statusBar()->showMessage(QStringLiteral("Edits saved to the ChromaDesk catalog."), 3000);
}

void MainWindow::saveEditedCopy()
{
    writePhoto(ExportMode::SaveCopy);
}

void MainWindow::exportPhoto()
{
    writePhoto(ExportMode::Export);
}

void MainWindow::convertPhoto()
{
    writePhoto(ExportMode::Convert);
}

void MainWindow::writePhoto(ExportMode mode)
{
    if (!m_document.isOpen())
        return;
    ExportDialog dialog(m_document.path(), mode, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const ExportOptions options = dialog.options();
    const QImage source = m_document.original();
    const LayerStack layers = m_document.layerStack();

    const QString operationName = mode == ExportMode::SaveCopy
        ? QStringLiteral("Save File")
        : mode == ExportMode::Convert ? QStringLiteral("Convert Format")
                                      : QStringLiteral("Export Photo");
    auto *progress = new QProgressDialog(QStringLiteral("Rendering full-resolution image…"),
                                         {}, 0, 0, this);
    progress->setWindowTitle(operationName);
    progress->setWindowModality(Qt::WindowModal);
    progress->setCancelButton(nullptr);
    progress->setMinimumDuration(0);
    progress->show();

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, progress, path = options.path, mode] {
        const QString error = watcher->result();
        watcher->deleteLater();
        progress->close();
        progress->deleteLater();
        if (!error.isEmpty()) {
            QMessageBox::critical(this, QStringLiteral("Image Write Failed"), error);
        } else {
            const QString verb = mode == ExportMode::Convert
                ? QStringLiteral("Converted") : QStringLiteral("Saved");
            statusBar()->showMessage(QStringLiteral("%1 %2")
                .arg(verb, QFileInfo(path).fileName()), 4000);
        }
    });
    watcher->setFuture(QtConcurrent::run([source, layers, options] {
        const QImage output = ImageProcessor::renderLayers(source, layers, options.maxEdge);
        if (output.isNull())
            return QStringLiteral("The image could not be rendered.");
        QImageWriter writer(options.path, options.format);
        if (options.format == QByteArray("jpg"))
            writer.setQuality(options.quality);
        if (!writer.write(output))
            return writer.errorString();
        return QString{};
    }));
}

void MainWindow::showPreferences()
{
    QSettings settings;
    const QString originalKey = settings.value(
        QStringLiteral("ui/theme"), QStringLiteral("gray")).toString();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("ChromaDesk Preferences"));
    dialog.setMinimumWidth(390);
    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(QStringLiteral("Interface Color"), &dialog);
    title->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(title);
    auto *themeCombo = new QComboBox(&dialog);
    const auto &themes = ThemeManager::options();
    for (const auto &option : themes)
        themeCombo->addItem(option.label, option.key);
    themeCombo->setCurrentIndex(qMax(0, themeCombo->findData(originalKey)));
    layout->addWidget(themeCombo);
    auto *description = new QLabel(&dialog);
    description->setWordWrap(true);
    description->setObjectName(QStringLiteral("mutedLabel"));
    layout->addWidget(description);
    auto updatePreview = [themeCombo, description, &themes](int index) {
        if (index < 0 || index >= themes.size())
            return;
        description->setText(themes.at(index).description
            + QStringLiteral(". Changes are previewed immediately."));
        ThemeManager::apply(*qApp, themes.at(index).theme);
    };
    connect(themeCombo, &QComboBox::currentIndexChanged, &dialog, updatePreview);
    updatePreview(themeCombo->currentIndex());

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        settings.setValue(QStringLiteral("ui/theme"), themeCombo->currentData().toString());
    } else {
        ThemeManager::apply(*qApp, ThemeManager::fromKey(originalKey));
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, QStringLiteral("About ChromaDesk"),
        QStringLiteral("<h2>ChromaDesk %1</h2>"
                       "<p>A fast, non-destructive photo editor built in modern C++ for Windows and macOS.</p>"
                       "<p>AI-assisted masks, Dust Heal, and Clone Stamp run locally. "
                       "Generative Fill uploads a resized edit only after explicit consent. "
                       "Original files are never overwritten. Camera RAW: %2.</p>"
                       "<p><small>Qt is used under the licence selected by the distributor. Review THIRD_PARTY.md before distribution.</small></p>")
            .arg(QString::fromLatin1(CHROMADESK_VERSION),
                 ImageLoader::rawSupportAvailable() ? QStringLiteral("enabled")
                                                    : QStringLiteral("not installed")));
}

QStringList MainWindow::imagePathsFromDrop(const QList<QUrl> &urls) const
{
    QStringList paths;
    for (const QUrl &url : urls) {
        const QString local = url.toLocalFile();
        const QFileInfo info(local);
        if (info.isFile()) {
            paths << info.absoluteFilePath();
        } else if (info.isDir()) {
            QDirIterator iterator(local, ImageLoader::supportedNameFilters(), QDir::Files,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext())
                paths << iterator.next();
        }
    }
    return paths;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    addPaths(imagePathsFromDrop(event->mimeData()->urls()));
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_catalogSaveTimer->isActive() && m_document.isOpen()) {
        m_catalogSaveTimer->stop();
        m_catalog.updateLayers(m_document.path(), m_document.layerStack());
    }
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    if (!m_openAiApiKey.isEmpty()) {
        m_openAiApiKey.fill(QChar(u'\0'));
        m_openAiApiKey.clear();
    }
    QMainWindow::closeEvent(event);
}

} // namespace chroma
