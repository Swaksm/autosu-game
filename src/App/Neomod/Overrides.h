#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include <string>
#include <utility>
#include <vector>
#include "types.h"

// what the player has set for one map in place of their usual settings
struct MapSettings {
    // convar name -> value, the client's values while the map is played (see CvarEditor::CONTEXT)
    std::vector<std::pair<std::string, std::string>> convars;
    i16 local_offset = 0;
    bool draw_background = true;
};

struct MapOverrides {
    std::string background_image_filename;
    MapSettings settings;
    i64 last_played = 0;
    u32 ppv2_version = 0;
    f32 star_rating = 0.0;
    f32 loudness = 0.f;
    i32 min_bpm = 0;
    i32 max_bpm = 0;
    i32 avg_bpm = 0;
    i16 online_offset = 0;
    u16 nb_circles = 0;
    u16 nb_sliders = 0;
    u16 nb_spinners = 0;
};
