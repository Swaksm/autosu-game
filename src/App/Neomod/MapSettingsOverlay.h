#pragma once
// Copyright (c) 2026, WH, All rights reserved.
#include "AnimationHandler.h"
#include "UIScreen.h"

#include <array>
#include <optional>

class CBaseUICheckbox;
class CBaseUISlider;
class ConVar;
class McFont;
class UICheckbox;
class UIIconButton;
class UISlider;

// the played map's settings (see MapSettings), in a panel along the bottom edge: it opens by itself when a play begins
// (which waits until the player is done with it), and on hover whenever they can be changed without holding anything
// up: from a strip while paused or while a match waits for its players, from the bare edge while a continue waits
class MapSettingsOverlay final : public UIScreen {
    NOCOPY_NOMOVE(MapSettingsOverlay)
   public:
    MapSettingsOverlay();
    ~MapSettingsOverlay() override;

    void draw() override;
    void tick() override;
    void updateInput(CBaseUIEventCtx &c) override;
    void onResolutionChange(vec2 newResolution) override;

    // a play has begun (not a retry): outside of a match, the panel shows while it loads and a while after
    // (map_settings_hold_time)
    void onPlayStart();
    // whether the play has to wait for the panel before it starts
    [[nodiscard]] bool isHoldingStart() const { return this->bPeeking; }

   private:
    // a checkbox for a bool convar, checked while it's on (or off, if inverted)
    struct Toggle {
        ConVar *cvar;
        bool inverted;
        UICheckbox *checkbox;
        UIIconButton *reset;  // takes the map's own value away
    };

    void updateLayout();
    void onDimChange(CBaseUISlider *slider);
    void onOffsetChange(CBaseUISlider *slider);
    void onToggleChange(CBaseUICheckbox *checkbox);
    // shows what the settings are now (which keybinds and the server may change as well)
    void syncWidgets();
    // nullopt: back to the usual value
    void setMapConvar(const ConVar &cvar, std::optional<double> value);
    void setLocalOffset(i16 offset);
    [[nodiscard]] f32 getFullHeight() const;

    UISlider *dimSlider;
    UIIconButton *dimReset;
    UISlider *offsetSlider;
    UIIconButton *offsetReset;
    std::array<Toggle, 3> toggles{};

    McFont *font;
    f32 labelX{0.f};  // where the slider rows' names start

    vec2 mousePosWhenAvailable{0.f};
    AnimFloat shownHeight{0.f};
    f32 targetHeight{0.f};
    AnimFloat accent{1.f};  // how much of the top edge's accent is left: it runs out while a play waits for the panel
    bool bPeeking{false};
    bool bAvailable{false};
    bool bMouseMoved{false};  // since the panel became available (a cursor resting at the bottom doesn't open it)
    bool bHovered{false};     // as of the last input pass (nothing above the panel had the cursor)
};
