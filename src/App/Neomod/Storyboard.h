#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.
#include "types.h"
#include "MD5Hash.h"
#include "Rect.h"
#include "Vectors_fwd.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Image;

// see https://osu.ppy.sh/wiki/en/Storyboard/Scripting
class Storyboard {
   public:
    // in drawing order
    // everything up to FOREGROUND goes below the hitobjects
    enum class Layer : u8 { BACKGROUND, FAIL, PASS, FOREGROUND, OVERLAY, COUNT };

    struct SpriteState {
        vec2 pos;
        vec2 scale;
        f32 rotation;  // radians, clockwise
        vec3 colour;   // 0 to 1
        f32 alpha;
        bool flipH;
        bool flipV;
        bool additive;
        u32 frame;
    };

    struct HitSound {
        u8 normalSet;
        u8 additionSet;
        u8 additions;
        u16 index;
    };

    Storyboard(std::string folder, std::string_view osuFile, std::string_view osbFile, std::string backgroundFile);
    ~Storyboard();

    Storyboard(const Storyboard &) = delete;
    Storyboard &operator=(const Storyboard &) = delete;

    // "Artist - Title (Creator).osb", or else the first .osb in the folder
    [[nodiscard]] static std::string findOsbFile(std::string_view folder, std::string_view artist,
                                                 std::string_view title, std::string_view creator);

    [[nodiscard]] bool isEmpty() const;

    [[nodiscard]] bool replacesBackground() const;
    [[nodiscard]] vec2 getBackgroundOffset() const;

    [[nodiscard]] uSz getNumSprites() const;
    [[nodiscard]] std::optional<SpriteState> getSpriteState(uSz sprite, f64 time) const;
    [[nodiscard]] std::optional<f64> getEndTime() const;

    void load(std::string_view skinFolder);
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] const Image *getBackgroundImage() const;

    [[nodiscard]] MD5Hash getHash();

    void update(f64 time, bool passing, bool seeked);
    void onHitSound(HitSound sound, f64 time);
    void onHitObjectHit(f64 time);

    struct View {
        McRect screen;
        vec2 pivot{0.f};
        bool flipX{false};
        bool flipY{false};
        f32 rotation{0.f};

        void transform() const;
    };

    void draw(Layer first, Layer last, const View &view) const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
