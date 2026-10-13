#include "core/PresetRepository.h"

namespace chroma {
namespace {

CreativeFilter makeFilter(const char *category, const char *name,
                          const char *description, double exposure,
                          double contrast, double highlights, double shadows,
                          double whites, double blacks, double temperature,
                          double tint, double saturation, double vibrance,
                          double clarity)
{
    EditState edits;
    edits.exposure = exposure;
    edits.contrast = contrast;
    edits.highlights = highlights;
    edits.shadows = shadows;
    edits.whites = whites;
    edits.blacks = blacks;
    edits.temperature = temperature;
    edits.tint = tint;
    edits.saturation = saturation;
    edits.vibrance = vibrance;
    edits.clarity = clarity;
    return {QString::fromLatin1(category), QString::fromLatin1(name),
            QString::fromLatin1(description), edits};
}

} // namespace

const QList<Preset> &PresetRepository::builtIns()
{
    static const QList<Preset> presets = [] {
        QList<Preset> result;

        EditState clean;
        clean.exposure = 0.15;
        clean.contrast = 8.0;
        clean.highlights = -18.0;
        clean.shadows = 16.0;
        clean.vibrance = 12.0;
        clean.clarity = 6.0;
        result.append({QStringLiteral("Clean & Natural"),
                       QStringLiteral("Balanced contrast and gentle colour"), clean});

        EditState warm;
        warm.contrast = 12.0;
        warm.highlights = -24.0;
        warm.shadows = 12.0;
        warm.blacks = 8.0;
        warm.temperature = 18.0;
        warm.tint = 5.0;
        warm.saturation = -6.0;
        warm.vibrance = 18.0;
        result.append({QStringLiteral("Warm Film"),
                       QStringLiteral("Warm highlights and softened colour"), warm});

        EditState cool;
        cool.exposure = 0.1;
        cool.contrast = -8.0;
        cool.highlights = -30.0;
        cool.shadows = 24.0;
        cool.blacks = 12.0;
        cool.temperature = -16.0;
        cool.tint = 4.0;
        cool.vibrance = -10.0;
        result.append({QStringLiteral("Cool Matte"),
                       QStringLiteral("Open shadows with a cool matte finish"), cool});

        EditState vivid;
        vivid.contrast = 18.0;
        vivid.highlights = -12.0;
        vivid.shadows = 8.0;
        vivid.whites = 10.0;
        vivid.blacks = -8.0;
        vivid.vibrance = 30.0;
        vivid.clarity = 12.0;
        result.append({QStringLiteral("Vivid Landscape"),
                       QStringLiteral("Crisp contrast and expressive colour"), vivid});

        EditState mono;
        mono.contrast = 22.0;
        mono.highlights = -14.0;
        mono.shadows = 10.0;
        mono.blacks = -12.0;
        mono.saturation = -100.0;
        mono.clarity = 18.0;
        result.append({QStringLiteral("Classic B&W"),
                       QStringLiteral("Strong monochrome tonal separation"), mono});

        return result;
    }();
    return presets;
}

const QList<CreativeFilter> &PresetRepository::creativeFilters()
{
    static const QList<CreativeFilter> filters = {
        // Cinematic
        makeFilter("Cinematic", "Teal & Orange", "Cool shadows and warm skin-friendly highlights",
                   0.05, 24, -24, 10, 8, -14, 12, -7, -4, 20, 12),
        makeFilter("Cinematic", "Blockbuster", "Bold contrast with polished cinematic colour",
                   -0.05, 34, -32, 12, 16, -22, 8, -5, -8, 24, 18),
        makeFilter("Cinematic", "Midnight Cinema", "Cool, dense tones for night scenes",
                   -0.25, 28, -34, 8, -10, -26, -24, 6, -16, 8, 16),
        makeFilter("Cinematic", "Silver Screen", "Restrained colour and luminous highlights",
                   0.15, 18, -18, 20, 18, -10, -4, 2, -58, -8, 8),
        makeFilter("Cinematic", "Neo Noir", "Deep blacks and a cool dramatic palette",
                   -0.20, 42, -22, -18, 6, -38, -18, 8, -48, -12, 24),
        makeFilter("Cinematic", "Desert Film", "Sun-baked warmth with softened blues",
                   0.10, 18, -28, 16, 12, -8, 28, 8, -10, 18, 7),
        makeFilter("Cinematic", "Arctic Cinema", "Clean icy colour and crisp separation",
                   0.10, 22, -25, 15, 16, -12, -30, 3, -12, 15, 14),
        makeFilter("Cinematic", "Golden Hour Film", "Glowing warmth with protected highlights",
                   0.20, 14, -38, 24, 14, -6, 34, 10, -3, 25, 5),
        makeFilter("Cinematic", "Bleach Bypass", "Muted colour and hard metallic contrast",
                   0.00, 46, -18, -14, 20, -34, -6, 1, -62, -20, 28),
        makeFilter("Cinematic", "Faded Cinema", "Lifted blacks and nostalgic screen colour",
                   0.12, -4, -28, 26, -8, 24, 12, 5, -18, 5, -4),

        // Film
        makeFilter("Film", "Warm Negative", "Warm, balanced negative-film rendering",
                   0.12, 10, -30, 18, 6, 10, 24, 5, -8, 18, 2),
        makeFilter("Film", "Cool Negative", "Cool negative colour with open shadows",
                   0.08, 8, -26, 24, 4, 12, -20, 4, -10, 10, 3),
        makeFilter("Film", "Vintage 1970", "Earthy warmth and gently faded density",
                   0.10, -2, -35, 22, -10, 22, 30, 12, -18, 10, -8),
        makeFilter("Film", "Vintage 1980", "Punchy colour with a cool-magenta cast",
                   0.05, 16, -20, 12, 10, -8, -8, 14, 4, 26, 5),
        makeFilter("Film", "Instant Soft", "Soft instant-photo contrast and pastel colour",
                   0.24, -14, -32, 30, -12, 28, 10, 6, -20, 8, -18),
        makeFilter("Film", "Slide Vivid", "Dense slide-film contrast and saturated colour",
                   -0.05, 30, -20, 5, 18, -22, 5, 3, 12, 34, 16),
        makeFilter("Film", "Matte Print", "Raised blacks and restrained print-like colour",
                   0.10, -6, -25, 24, -6, 26, 4, 3, -14, 4, -6),
        makeFilter("Film", "Sepia Story", "Warm sepia-inspired storytelling tone",
                   0.08, 12, -28, 18, 6, 8, 38, 12, -55, -18, 4),
        makeFilter("Film", "Classic Fade", "Low contrast and mellow vintage colour",
                   0.18, -16, -34, 32, -12, 30, 14, 7, -22, 2, -10),
        makeFilter("Film", "Pastel Film", "Bright airy colour with softened contrast",
                   0.30, -18, -42, 34, -8, 24, 8, 8, -16, 14, -14),

        // Portrait
        makeFilter("Portrait", "Natural Skin", "Balanced skin colour and gentle definition",
                   0.15, 6, -24, 18, 8, 2, 8, 5, -4, 10, -5),
        makeFilter("Portrait", "Soft Portrait", "Soft contrast with flattering light",
                   0.25, -10, -36, 28, 8, 12, 10, 5, -8, 6, -22),
        makeFilter("Portrait", "Studio Clean", "Neutral, crisp studio presentation",
                   0.18, 12, -20, 16, 18, -8, 0, 2, -2, 8, 10),
        makeFilter("Portrait", "Warm Portrait", "Gentle warmth and luminous skin",
                   0.20, 5, -32, 22, 12, 5, 22, 8, -5, 12, -8),
        makeFilter("Portrait", "Cool Portrait", "Editorial coolness with controlled skin colour",
                   0.15, 8, -28, 20, 10, 2, -16, 5, -8, 8, -3),
        makeFilter("Portrait", "High Key", "Bright whites and delicate facial contrast",
                   0.55, -12, -40, 36, 32, 18, 4, 3, -8, 4, -18),
        makeFilter("Portrait", "Low Key", "Dark dramatic portrait with protected highlights",
                   -0.35, 32, -36, -20, 10, -34, 6, 5, -12, 6, 18),
        makeFilter("Portrait", "Editorial", "Crisp modern colour for fashion portraits",
                   0.10, 22, -24, 12, 16, -18, -4, 7, -8, 18, 16),
        makeFilter("Portrait", "Dreamy", "Bright softened highlights and gentle colour",
                   0.38, -20, -45, 38, 6, 24, 14, 10, -18, 10, -28),
        makeFilter("Portrait", "Monochrome Portrait", "Soft monochrome with open skin tones",
                   0.20, 16, -30, 22, 14, -8, 0, 0, -100, 0, 8),

        // Landscape
        makeFilter("Landscape", "Vivid Landscape", "Crisp contrast and expressive colour",
                   0.05, 24, -28, 18, 16, -18, 2, 2, 8, 34, 20),
        makeFilter("Landscape", "Forest Deep", "Rich greens and deep woodland shadows",
                   -0.05, 25, -26, 15, 8, -25, -8, -4, -4, 28, 18),
        makeFilter("Landscape", "Ocean Blue", "Cool clear blues with bright sea highlights",
                   0.08, 20, -30, 16, 20, -14, -20, -2, 2, 30, 16),
        makeFilter("Landscape", "Desert Glow", "Warm sand tones and glowing sunlight",
                   0.12, 20, -34, 18, 18, -12, 32, 8, 2, 28, 14),
        makeFilter("Landscape", "Snow Clean", "Bright clean snow with preserved detail",
                   0.35, 12, -48, 28, 30, -6, -12, 2, -8, 12, 12),
        makeFilter("Landscape", "Sunset Boost", "Warmer highlights and richer sunset colour",
                   0.08, 18, -42, 20, 14, -14, 26, 16, 6, 38, 10),
        makeFilter("Landscape", "Tropical", "Lush vibrant colour for sunlit travel scenes",
                   0.15, 14, -28, 24, 16, -10, 10, -3, 8, 42, 12),
        makeFilter("Landscape", "Misty Morning", "Lifted shadows and quiet cool atmosphere",
                   0.25, -22, -38, 36, -10, 28, -14, 4, -18, 5, -16),
        makeFilter("Landscape", "Autumn Rich", "Deep warm foliage and crisp texture",
                   0.05, 24, -30, 14, 12, -22, 24, 10, 4, 36, 18),
        makeFilter("Landscape", "HDR Natural", "Recovered highlights and detailed shadows",
                   0.08, 16, -55, 45, 14, -16, 0, 2, -2, 26, 28),

        // Black & White
        makeFilter("Black & White", "Classic B&W", "Balanced traditional monochrome contrast",
                   0.05, 24, -24, 18, 12, -18, 0, 0, -100, 0, 16),
        makeFilter("Black & White", "High Contrast B&W", "Graphic whites and deep blacks",
                   -0.05, 48, -16, -12, 28, -42, 0, 0, -100, 0, 30),
        makeFilter("Black & White", "Matte B&W", "Raised blacks and understated monochrome",
                   0.12, -4, -28, 28, -8, 28, 0, 0, -100, 0, 2),
        makeFilter("Black & White", "Silver B&W", "Bright silvery highlights and fine separation",
                   0.18, 20, -34, 22, 24, -16, 0, 0, -100, 0, 20),
        makeFilter("Black & White", "Soft B&W", "Gentle monochrome for portraits and weddings",
                   0.25, -12, -38, 30, 10, 16, 0, 0, -100, 0, -18),
        makeFilter("Black & White", "Documentary", "Direct tonal contrast with natural detail",
                   0.00, 32, -28, 14, 14, -28, 0, 0, -100, 0, 24),
        makeFilter("Black & White", "Graphite", "Dark grey density with crisp texture",
                   -0.18, 28, -22, 4, 2, -30, 0, 0, -100, 0, 32),
        makeFilter("Black & White", "Noir", "Moody low-key monochrome drama",
                   -0.32, 52, -20, -28, 10, -48, 0, 0, -100, 0, 34),
        makeFilter("Black & White", "Bright B&W", "Airy whites with clean monochrome colour",
                   0.42, 10, -42, 34, 34, -2, 0, 0, -100, 0, 8),
        makeFilter("Black & White", "Faded B&W", "Nostalgic lifted blacks and soft whites",
                   0.16, -16, -36, 34, -14, 32, 0, 0, -100, 0, -8),
    };
    return filters;
}

} // namespace chroma
