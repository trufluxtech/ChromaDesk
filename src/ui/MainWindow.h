#pragma once

#include "core/Catalog.h"
#include "core/ImageDocument.h"
#include "ui/ImageCanvas.h"

#include <QHash>
#include <QMainWindow>
#include <QUrl>

class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProgressDialog;
class QPushButton;
class QScrollArea;
class QSlider;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace chroma {

class AdjustmentControl;
enum class ExportMode;
class GenerativeFillClient;
class HistogramWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void importPhotos();
    void importFolder();
    void openLibraryItem(QListWidgetItem *item);
    void saveEdits();
    void saveEditedCopy();
    void exportPhoto();
    void convertPhoto();
    void resetEdits();
    void rotateClockwise();
    void toggleCrop(bool enabled);
    void applyPreset();
    void refreshCreativeFilters();
    void applyCreativeFilter();
    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayerUp();
    void moveLayerDown();
    void setLayerOpacity(int value);
    void setMaskFeather(int value);
    void selectLayer(QListWidgetItem *current, QListWidgetItem *previous);
    void updateLayerItem(QListWidgetItem *item);
    void createAiSubjectMask();
    void createAiBackgroundMask();
    void createAiSkyMask();
    void createColorRangeMask();
    void createLuminanceRangeMask();
    void editSelectedMask();
    void beginRectangleMask();
    void beginEllipseMask();
    void completeShapeMask(const QRectF &bounds,
                           ImageCanvas::ShapeMaskMode mode);
    void toggleDustRemoval(bool enabled);
    void toggleCloneStamp(bool enabled);
    void addDustRemoval(const QPointF &target);
    void addCloneStamp(const QPointF &source, const QPointF &target);
    void beginGenerativeFill();
    void finishGenerativeFill(const QImage &image, const QString &errorMessage);
    void applyAiAutoEnhance();
    void applyAiSubjectPop();
    void applyAiBackgroundBlur();
    void applyAiMonochromeBackground();
    void applyAiEnhanceSky();
    void applyAiReplaceColor();
    void addTextLayer();
    void editTextLayer();
    void addWatermark();
    void editWatermark();
    void moveSelectedWatermark(const QPointF &position);
    void setBorderSize(int value);
    void chooseBorderColor();
    void preparePassportSheet();
    void setRating(int rating);
    void togglePicked();
    void showPreferences();
    void showAbout();

private:
    enum AdjustmentId {
        Exposure = 100,
        Contrast,
        Highlights,
        Shadows,
        Whites,
        Blacks,
        Temperature,
        Tint,
        Saturation,
        Vibrance,
        ReplaceAmount,
        Clarity,
        Blur
    };

    void createActions();
    void createMenus();
    void createToolbar();
    void createLibraryDock();
    void createDevelopDock();
    AdjustmentControl *addAdjustment(QVBoxLayout *layout, const QString &label,
                                     double minimum, double maximum, double step,
                                     int decimals, AdjustmentId id);
    void refreshLibrary();
    void applyLibraryViewSettings();
    void queueThumbnail(const QString &path);
    void openPath(const QString &path);
    void addPaths(const QStringList &paths);
    void applyAdjustment(AdjustmentId id, double value, int mergeId);
    void addMaskedLayer(const QString &name, const LayerMask &mask,
                        const EditState &edits = {});
    void writePhoto(ExportMode mode);
    void syncControls(const EditState &state);
    void refreshLayers();
    void syncActiveLayer();
    void updateActiveLayerOverlay(bool force = false);
    void updateBorderButton(const QColor &color);
    void updateDocumentPresentation();
    void updateItemCaption(const QString &path);
    void deactivateRetouchTools();
    [[nodiscard]] double retouchRadius() const;
    void requestGenerativeFill(const QRectF &bounds);
    [[nodiscard]] QStringList imagePathsFromDrop(const QList<QUrl> &urls) const;

    Catalog m_catalog;
    ImageDocument m_document;
    QListWidget *m_libraryList = nullptr;
    QDockWidget *m_libraryDock = nullptr;
    QToolButton *m_libraryIconViewButton = nullptr;
    QToolButton *m_libraryListViewButton = nullptr;
    QSlider *m_libraryIconSize = nullptr;
    QLabel *m_libraryIconSizeValue = nullptr;
    ImageCanvas *m_canvas = nullptr;
    HistogramWidget *m_histogram = nullptr;
    QLabel *m_fileInfo = nullptr;
    QLabel *m_editSummary = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QComboBox *m_filterCategory = nullptr;
    QListWidget *m_filterList = nullptr;
    QComboBox *m_passportSheetSize = nullptr;
    QListWidget *m_layerList = nullptr;
    QSlider *m_layerOpacity = nullptr;
    QLabel *m_layerOpacityValue = nullptr;
    QSlider *m_maskFeather = nullptr;
    QLabel *m_maskFeatherValue = nullptr;
    QLabel *m_maskSummary = nullptr;
    QLabel *m_aiEngineStatus = nullptr;
    QSlider *m_borderSize = nullptr;
    QLabel *m_borderSizeValue = nullptr;
    QPushButton *m_borderColorButton = nullptr;
    QPushButton *m_dustRemovalButton = nullptr;
    QPushButton *m_cloneStampButton = nullptr;
    QPushButton *m_generativeFillButton = nullptr;
    QSlider *m_retouchSize = nullptr;
    QLabel *m_retouchSizeValue = nullptr;
    GenerativeFillClient *m_generativeFillClient = nullptr;
    QProgressDialog *m_generativeFillProgress = nullptr;
    QHash<int, AdjustmentControl *> m_controls;
    QAction *m_exportAction = nullptr;
    QAction *m_saveEditsAction = nullptr;
    QAction *m_saveCopyAction = nullptr;
    QAction *m_convertAction = nullptr;
    QAction *m_cropAction = nullptr;
    QAction *m_originalAction = nullptr;
    QAction *m_sideBySideAction = nullptr;
    QAction *m_splitAction = nullptr;
    QAction *m_pickAction = nullptr;
    QAction *m_libraryVisibilityAction = nullptr;
    QTimer *m_catalogSaveTimer = nullptr;
    QString m_pendingShapeLayerId;
    QRectF m_pendingGenerativeBounds;
    QString m_pendingGenerativePrompt;
    QString m_openAiApiKey;
    QString m_overlayLayerId;
    LayerMask m_overlayMask;
    RetouchOperation m_overlayRetouchOperation;
    EditState m_overlayBaseEdits;
    quint64 m_overlayGeneration = 0;
    bool m_overlayInitialized = false;
    bool m_syncingLayers = false;
    bool m_generativeFillConsent = false;
};

} // namespace chroma
