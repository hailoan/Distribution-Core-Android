#ifndef VIDEOLIB_APPEARANCE_H
#define VIDEOLIB_APPEARANCE_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct AdjustmentSnapshot {
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float exposure = 0.0f;
    float darks = 1.0f;
    float levelMinimum = 0.0f;
    float levelGamma = 1.0f;
    float levelMaximum = 1.0f;
    float vignette = 0.0f;
    float vibrance = 0.0f;
    float temperature = 0.0f;
    float hue = 0.0f;
    float highlights = -2.0f;
    float shadows = 0.0f;
    float lights = 1.0f;
    float clarity = 0.0f;
};

struct FilterTexture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba8888;

    bool operator==(const FilterTexture &other) const {
        return width == other.width && height == other.height && rgba8888 == other.rgba8888;
    }
};

struct FilterDescriptor {
    int version = 1;
    std::string source;
    float opacity = 1.0f;
    std::vector<FilterTexture> textures;

    bool operator==(const FilterDescriptor &other) const {
        return version == other.version && source == other.source &&
               opacity == other.opacity && textures == other.textures;
    }
};

// Animated pre-filter pass. Unlike a filter — which transforms an already
// sampled color — an effect samples the frame itself through getColor(vec2),
// so it can displace coordinates (RGB split, scanlines).
//
// The shader's u_time is the frame's own presentation time, supplied per draw
// via GlProgram::setEffectTime, so the effect animates with playback. Deriving
// it from media time rather than wall time means the same frame always renders
// identically — a paused redraw is stable and an export matches the preview it
// came from.
//
// `speed` scales that clock: u_time = frameTime * speed. Below 1 the animation
// runs slower than the video, above 1 faster, and 0 freezes it on the opening
// pose. It is a plain uniform multiplier, so changing it never relinks the
// program — which is what lets it be driven from a slider.
struct EffectDescriptor {
    int version = 1;
    std::string source;
    float opacity = 1.0f;
    float speed = 1.0f;

    bool operator==(const EffectDescriptor &other) const {
        return version == other.version && source == other.source &&
               opacity == other.opacity && speed == other.speed;
    }
};

struct AppearanceSnapshot {
    AdjustmentSnapshot adjustments;
    std::optional<FilterDescriptor> filter;
    std::optional<EffectDescriptor> effect;
};

enum class AppearanceError : int {
    None = 0,
    InvalidValue = 1,
    InvalidLevels = 2,
    UnsupportedFilterVersion = 3,
    InvalidFilterSource = 4,
    InvalidFilterOpacity = 5,
    InvalidFilterTexture = 6,
    SurfaceUnavailable = 7,
    Released = 8,
    ShaderCompilation = 9,
    ProgramLink = 10,
    DeviceCapability = 11,
    ResourceAllocation = 12,
    RenderFailure = 13,
    UnsupportedEffectVersion = 14,
    InvalidEffectSource = 15,
    InvalidEffectOpacity = 16,
    InvalidEffectSpeed = 17,
};

struct AppearanceApplyResult {
    AppearanceError error = AppearanceError::None;
    std::string diagnostic;

    bool accepted() const { return error == AppearanceError::None; }

    static AppearanceApplyResult success() { return {}; }

    static AppearanceApplyResult failure(AppearanceError error, std::string diagnostic = {}) {
        return {error, std::move(diagnostic)};
    }
};

#endif // VIDEOLIB_APPEARANCE_H
