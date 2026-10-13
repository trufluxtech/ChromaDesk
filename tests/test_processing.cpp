#include "core/EditState.h"
#include "core/ImageProcessor.h"
#include "core/LayerMask.h"
#include "core/LayerStack.h"
#include "core/PassportSheet.h"
#include "core/PresetRepository.h"
#include "core/RetouchOperation.h"
#include "core/SmartMaskEngine.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using chroma::EditState;
using chroma::ImageProcessor;
using chroma::LayerMask;
using chroma::LayerMaskType;
using chroma::LayerStack;
using chroma::RetouchType;
using chroma::SmartMaskEngine;

class ProcessingTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultStateRoundTrips();
    void exposureBrightensImage();
    void cropProducesExpectedDimensions();
    void rotationSwapsDimensions();
    void monochromePresetRemovesSaturation();
    void sourceIsNeverMutated();
    void layerValuesRoundTrip();
    void layerOpacityControlsComposite();
    void maskValuesRoundTrip();
    void colorMaskOnlyChangesMatchingPixels();
    void luminanceMaskOnlyChangesSelectedRange();
    void rectangleMaskOnlyChangesInsideShape();
    void subjectAndBackgroundMasksAreComplements();
    void skyMaskPrefersConnectedUpperSky();
    void colorReplacementChangesOnlySelectedColor();
    void textOverlayValuesRoundTrip();
    void textOverlayRendersOnLayer();
    void watermarkValuesRoundTrip();
    void imageBorderRoundTripsAndRenders();
    void retouchValuesRoundTrip();
    void dustHealReplacesSmallSpot();
    void cloneStampCopiesSourceTexture();
    void generatedImageUsesMarkedArea();
    void passportSheetCreatesSixPrintSizedCopies();
    void creativeFiltersHaveExpectedCountAndCategories();
    void baseLayerCannotBeRemoved();
};

void ProcessingTests::defaultStateRoundTrips()
{
    EditState state;
    state.exposure = 1.2;
    state.temperature = -18.0;
    state.replacementColor = QColor(QStringLiteral("#3f72ff"));
    state.replacementAmount = 72.0;
    state.borderSize = 0.08;
    state.borderColor = QColor(QStringLiteral("#f4e6cc"));
    state.rotation = 270;
    state.crop = QRectF(0.1, 0.2, 0.7, 0.6);
    const EditState restored = EditState::fromJson(state.toJson());
    QVERIFY(restored == state);
}

void ProcessingTests::exposureBrightensImage()
{
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(QColor(64, 64, 64));
    EditState state;
    state.exposure = 1.0;
    const QImage output = ImageProcessor::render(image, state);
    QVERIFY(output.pixelColor(0, 0).red() > image.pixelColor(0, 0).red());
}

void ProcessingTests::cropProducesExpectedDimensions()
{
    QImage image(400, 200, QImage::Format_RGB32);
    image.fill(Qt::gray);
    EditState state;
    state.crop = QRectF(0.25, 0.25, 0.5, 0.5);
    const QImage output = ImageProcessor::orientedAndCropped(image, state);
    QCOMPARE(output.size(), QSize(200, 100));
}

void ProcessingTests::rotationSwapsDimensions()
{
    QImage image(400, 200, QImage::Format_RGB32);
    EditState state;
    state.rotation = 90;
    const QImage output = ImageProcessor::orientedAndCropped(image, state);
    QCOMPARE(output.size(), QSize(200, 400));
}

void ProcessingTests::monochromePresetRemovesSaturation()
{
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(QColor(220, 80, 35));
    EditState state;
    state.saturation = -100.0;
    const QColor result = ImageProcessor::render(image, state).pixelColor(0, 0);
    QVERIFY(std::abs(result.red() - result.green()) <= 1);
    QVERIFY(std::abs(result.green() - result.blue()) <= 1);
}

void ProcessingTests::sourceIsNeverMutated()
{
    QImage source(8, 8, QImage::Format_RGB32);
    source.fill(QColor(30, 90, 180));
    const QImage snapshot = source.copy();
    EditState state;
    state.contrast = 75.0;
    state.rotation = 90;
    (void)ImageProcessor::render(source, state);
    QCOMPARE(source, snapshot);
}

void ProcessingTests::layerValuesRoundTrip()
{
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Warm Subject"));
    auto *layer = layers.find(id);
    QVERIFY(layer != nullptr);
    layer->opacity = 0.42;
    layer->visible = false;
    layer->edits.exposure = 1.3;
    layer->edits.temperature = 24.0;
    layer->edits.blur = 12.0;
    layer->mask.type = LayerMaskType::ColorRange;
    layer->mask.targetColor = QColor(QStringLiteral("#4a90e2"));
    layer->mask.colorTolerance = 0.24;

    const LayerStack restored = LayerStack::fromJson(layers.toJson());
    QVERIFY(restored == layers);
    QCOMPARE(restored.find(id)->name, QStringLiteral("Warm Subject"));
    QCOMPARE(restored.find(id)->edits.exposure, 1.3);
    QVERIFY(restored.find(id)->mask.type == LayerMaskType::ColorRange);
}

void ProcessingTests::layerOpacityControlsComposite()
{
    QImage source(1, 1, QImage::Format_RGB32);
    source.fill(QColor(60, 60, 60));

    LayerStack halfStack;
    const QString id = halfStack.addLayer(QStringLiteral("Exposure"));
    halfStack.find(id)->edits.exposure = 2.0;
    halfStack.find(id)->opacity = 0.5;

    LayerStack fullStack = halfStack;
    fullStack.find(id)->opacity = 1.0;
    const int original = source.pixelColor(0, 0).red();
    const int half = ImageProcessor::renderLayers(source, halfStack).pixelColor(0, 0).red();
    const int full = ImageProcessor::renderLayers(source, fullStack).pixelColor(0, 0).red();
    QVERIFY(half > original);
    QVERIFY(half < full);
}

void ProcessingTests::maskValuesRoundTrip()
{
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Luminance"));
    auto *layer = layers.find(id);
    layer->mask.type = LayerMaskType::LuminanceRange;
    layer->mask.luminanceMinimum = 0.31;
    layer->mask.luminanceMaximum = 0.68;
    layer->mask.feather = 0.12;
    const LayerStack restored = LayerStack::fromJson(layers.toJson());
    QVERIFY(restored == layers);
}

void ProcessingTests::colorMaskOnlyChangesMatchingPixels()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, QColor(180, 30, 30));
    source.setPixelColor(1, 0, QColor(30, 30, 180));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Red only"));
    auto *layer = layers.find(id);
    layer->edits.exposure = 1.0;
    layer->mask.type = LayerMaskType::ColorRange;
    layer->mask.targetColor = QColor(180, 30, 30);
    layer->mask.colorTolerance = 0.05;
    layer->mask.feather = 0.0;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QVERIFY(output.pixelColor(0, 0).red() > source.pixelColor(0, 0).red());
    QCOMPARE(output.pixelColor(1, 0), source.pixelColor(1, 0));
}

void ProcessingTests::luminanceMaskOnlyChangesSelectedRange()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, QColor(35, 35, 35));
    source.setPixelColor(1, 0, QColor(190, 190, 190));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Highlights"));
    auto *layer = layers.find(id);
    layer->edits.exposure = 0.5;
    layer->mask.type = LayerMaskType::LuminanceRange;
    layer->mask.luminanceMinimum = 0.6;
    layer->mask.luminanceMaximum = 1.0;
    layer->mask.feather = 0.0;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QCOMPARE(output.pixelColor(0, 0), source.pixelColor(0, 0));
    QVERIFY(output.pixelColor(1, 0).red() > source.pixelColor(1, 0).red());
}

void ProcessingTests::rectangleMaskOnlyChangesInsideShape()
{
    QImage source(4, 1, QImage::Format_RGB32);
    source.fill(QColor(60, 60, 60));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Shape"));
    auto *layer = layers.find(id);
    layer->edits.exposure = 1.0;
    layer->mask.type = LayerMaskType::Rectangle;
    layer->mask.bounds = QRectF(0.0, 0.0, 0.5, 1.0);
    layer->mask.feather = 0.0;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QVERIFY(output.pixelColor(0, 0).red() > source.pixelColor(0, 0).red());
    QCOMPARE(output.pixelColor(3, 0), source.pixelColor(3, 0));
}

void ProcessingTests::subjectAndBackgroundMasksAreComplements()
{
    QImage source(24, 24, QImage::Format_RGB32);
    source.fill(QColor(235, 235, 235));
    for (int y = 5; y < 21; ++y) {
        for (int x = 8; x < 17; ++x)
            source.setPixelColor(x, y, QColor(45, 90, 175));
    }
    LayerMask subjectRecipe;
    subjectRecipe.type = LayerMaskType::AiSubject;
    subjectRecipe.feather = 0.0;
    LayerMask backgroundRecipe = subjectRecipe;
    backgroundRecipe.type = LayerMaskType::AiBackground;
    const QImage subject = SmartMaskEngine::generate(source, subjectRecipe);
    const QImage background = SmartMaskEngine::generate(source, backgroundRecipe);
    QCOMPARE(subject.size(), source.size());
    QCOMPARE(background.size(), source.size());
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            QCOMPARE(static_cast<int>(subject.constScanLine(y)[x])
                + static_cast<int>(background.constScanLine(y)[x]), 255);
        }
    }
}

void ProcessingTests::skyMaskPrefersConnectedUpperSky()
{
    QImage source(24, 20, QImage::Format_RGB32);
    source.fill(QColor(35, 120, 45));
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < source.width(); ++x)
            source.setPixelColor(x, y, QColor(80, 155, 235));
    }
    chroma::LayerMask maskRecipe;
    maskRecipe.type = LayerMaskType::AiSky;
    maskRecipe.feather = 0.0;
    const QImage mask = SmartMaskEngine::generate(source, maskRecipe);
    QVERIFY(!mask.isNull());
    QVERIFY(mask.constScanLine(2)[source.width() / 2] > 180);
    QVERIFY(mask.constScanLine(18)[source.width() / 2] < 25);
}

void ProcessingTests::colorReplacementChangesOnlySelectedColor()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, QColor(180, 30, 30));
    source.setPixelColor(1, 0, QColor(30, 170, 40));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Replace red with blue"));
    auto *layer = layers.find(id);
    layer->mask.type = LayerMaskType::ColorRange;
    layer->mask.targetColor = QColor(180, 30, 30);
    layer->mask.colorTolerance = 0.05;
    layer->mask.feather = 0.0;
    layer->edits.replacementColor = QColor(35, 85, 230);
    layer->edits.replacementAmount = 100.0;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QVERIFY(output.pixelColor(0, 0).blue() > output.pixelColor(0, 0).red());
    QCOMPARE(output.pixelColor(1, 0), source.pixelColor(1, 0));
}

void ProcessingTests::textOverlayValuesRoundTrip()
{
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Caption"));
    auto *layer = layers.find(id);
    layer->textOverlay.text = QStringLiteral("ChromaDesk");
    layer->textOverlay.fontFamily = QStringLiteral("Sans Serif");
    layer->textOverlay.color = QColor(QStringLiteral("#80ffffff"));
    layer->textOverlay.size = 0.12;
    layer->textOverlay.position = QPointF(0.9, 0.1);
    layer->textOverlay.bold = true;
    const LayerStack restored = LayerStack::fromJson(layers.toJson());
    QVERIFY(restored == layers);
}

void ProcessingTests::textOverlayRendersOnLayer()
{
    QImage source(320, 240, QImage::Format_RGB32);
    source.fill(QColor(15, 15, 15));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Text"));
    auto *layer = layers.find(id);
    layer->textOverlay.text = QStringLiteral("TEST");
    layer->textOverlay.size = 0.24;
    layer->textOverlay.position = QPointF(0.5, 0.5);
    layer->textOverlay.color = Qt::white;
    layer->textOverlay.outline = false;
    const QImage output = ImageProcessor::renderLayers(source, layers);
    int changedPixels = 0;
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            if (output.pixelColor(x, y).red() > 40)
                ++changedPixels;
        }
    }
    QVERIFY(changedPixels > 20);
}

void ProcessingTests::watermarkValuesRoundTrip()
{
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Watermark"));
    auto *layer = layers.find(id);
    layer->watermarkOverlay.type = chroma::WatermarkType::Text;
    layer->watermarkOverlay.text = QStringLiteral("© ChromaDesk");
    layer->watermarkOverlay.fontFamily = QStringLiteral("Sans Serif");
    layer->watermarkOverlay.color = QColor(QStringLiteral("#ccffffff"));
    layer->watermarkOverlay.position = QPointF(0.74, 0.91);
    layer->watermarkOverlay.size = 0.09;
    layer->watermarkOverlay.opacity = 0.48;
    layer->watermarkOverlay.bold = true;
    const LayerStack restored = LayerStack::fromJson(layers.toJson());
    QVERIFY(restored == layers);
    QVERIFY(restored.find(id)->watermarkOverlay.isEnabled());
}

void ProcessingTests::imageBorderRoundTripsAndRenders()
{
    QImage source(100, 50, QImage::Format_RGB32);
    source.fill(QColor(24, 80, 160));
    EditState state;
    state.borderSize = 0.10;
    state.borderColor = QColor(QStringLiteral("#f02030"));
    const EditState restored = EditState::fromJson(state.toJson());
    QVERIFY(restored == state);

    const QImage output = ImageProcessor::render(source, state);
    QCOMPARE(output.size(), QSize(110, 60));
    QCOMPARE(output.pixelColor(0, 0), state.borderColor);
    QCOMPARE(output.pixelColor(5, 5), source.pixelColor(0, 0));
}

void ProcessingTests::retouchValuesRoundTrip()
{
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Clone"));
    auto *layer = layers.find(id);
    layer->retouchOperation.type = RetouchType::CloneStamp;
    layer->retouchOperation.source = QPointF(0.21, 0.42);
    layer->retouchOperation.target = QPointF(0.71, 0.68);
    layer->retouchOperation.radius = 0.075;
    const LayerStack restored = LayerStack::fromJson(layers.toJson());
    QVERIFY(restored == layers);
    QVERIFY(restored.find(id)->retouchOperation.isEnabled());
}

void ProcessingTests::dustHealReplacesSmallSpot()
{
    QImage source(64, 64, QImage::Format_RGB32);
    source.fill(QColor(145, 145, 145));
    for (int y = 29; y <= 35; ++y) {
        for (int x = 29; x <= 35; ++x)
            source.setPixelColor(x, y, QColor(8, 8, 8));
    }
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Dust Heal"));
    auto *layer = layers.find(id);
    layer->retouchOperation.type = RetouchType::HealSpot;
    layer->retouchOperation.target = QPointF(0.5, 0.5);
    layer->retouchOperation.radius = 0.075;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QVERIFY(output.pixelColor(32, 32).red() > 120);
}

void ProcessingTests::cloneStampCopiesSourceTexture()
{
    QImage source(80, 40, QImage::Format_RGB32);
    source.fill(QColor(35, 45, 60));
    for (int y = 16; y <= 24; ++y) {
        for (int x = 16; x <= 24; ++x)
            source.setPixelColor(x, y, QColor(225, 45, 30));
    }
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Clone Stamp"));
    auto *layer = layers.find(id);
    layer->retouchOperation.type = RetouchType::CloneStamp;
    layer->retouchOperation.source = QPointF(20.0 / 79.0, 20.0 / 39.0);
    layer->retouchOperation.target = QPointF(60.0 / 79.0, 20.0 / 39.0);
    layer->retouchOperation.radius = 0.10;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    const QColor cloned = output.pixelColor(60, 20);
    QVERIFY(cloned.red() > 200);
    QVERIFY(cloned.green() < 70);
}

void ProcessingTests::generatedImageUsesMarkedArea()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QImage generated(40, 20, QImage::Format_RGB32);
    generated.fill(QColor(215, 35, 40));
    const QString generatedPath = directory.filePath(QStringLiteral("fill.png"));
    QVERIFY(generated.save(generatedPath));

    QImage source(40, 20, QImage::Format_RGB32);
    source.fill(QColor(25, 70, 205));
    LayerStack layers;
    const QString id = layers.addLayer(QStringLiteral("Generative Fill"));
    auto *layer = layers.find(id);
    layer->retouchOperation.type = RetouchType::GeneratedImage;
    layer->retouchOperation.generatedImagePath = generatedPath;
    layer->mask.type = LayerMaskType::Rectangle;
    layer->mask.bounds = QRectF(0.0, 0.0, 0.5, 1.0);
    layer->mask.feather = 0.0;

    const QImage output = ImageProcessor::renderLayers(source, layers);
    QVERIFY(output.pixelColor(5, 10).red() > 190);
    QCOMPARE(output.pixelColor(35, 10), source.pixelColor(35, 10));
}

void ProcessingTests::passportSheetCreatesSixPrintSizedCopies()
{
    QImage portrait(700, 900, QImage::Format_RGB32);
    portrait.fill(QColor(60, 120, 210));
    int copies = 0;
    const QImage sheet = chroma::PassportSheet::create(
        portrait, chroma::PassportSheetOptions::fourBySix(), &copies);
    QCOMPARE(sheet.size(), QSize(1200, 1800));
    QCOMPARE(copies, 6);
    QVERIFY(sheet.dotsPerMeterX() >= 11800);
    QVERIFY(sheet.pixelColor(300, 300) != QColor(Qt::white));
}

void ProcessingTests::creativeFiltersHaveExpectedCountAndCategories()
{
    const auto &filters = chroma::PresetRepository::creativeFilters();
    QCOMPARE(filters.size(), 50);
    QSet<QString> names;
    QHash<QString, int> categoryCounts;
    for (const auto &filter : filters) {
        QVERIFY(!filter.name.isEmpty());
        QVERIFY(!filter.description.isEmpty());
        QVERIFY(!names.contains(filter.name));
        names.insert(filter.name);
        ++categoryCounts[filter.category];
    }
    QCOMPARE(names.size(), 50);
    QCOMPARE(categoryCounts.size(), 5);
    QCOMPARE(categoryCounts.value(QStringLiteral("Cinematic")), 10);
    QCOMPARE(categoryCounts.value(QStringLiteral("Film")), 10);
    QCOMPARE(categoryCounts.value(QStringLiteral("Portrait")), 10);
    QCOMPARE(categoryCounts.value(QStringLiteral("Landscape")), 10);
    QCOMPARE(categoryCounts.value(QStringLiteral("Black & White")), 10);
}

void ProcessingTests::baseLayerCannotBeRemoved()
{
    LayerStack layers;
    const QString baseId = layers.baseLayer()->id;
    QVERIFY(!layers.removeLayer(baseId));
    QCOMPARE(layers.size(), 1);
}

QTEST_MAIN(ProcessingTests)
#include "test_processing.moc"
