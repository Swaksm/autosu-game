#pragma once
// Copyright (c) 2026, WH, All rights reserved.
#include "CBaseUIBox.h"

#include <functional>
#include <string>
#include <vector>

class CBaseUIStyledButton;

// a row of tabs of which one is chosen: a click on another chooses that one and tells the bar's user, who shows what
// belongs to it
class CBaseUITabBar final : public CBaseUIBox {
    NOCOPY_NOMOVE(CBaseUITabBar)
   public:
    explicit CBaseUITabBar(std::string name = {});
    ~CBaseUITabBar() override;

    // a tab (an element named `name`); the first one added is chosen
    CBaseUITabBar *add(std::string name, std::string label);
    // (doesn't call the callback)
    CBaseUITabBar *setChosen(int index);
    [[nodiscard]] int getChosen() const { return this->chosen; }
    // called with the tab the user chose
    CBaseUITabBar *setChangeCallback(std::function<void(int)> callback);

   private:
    std::vector<CBaseUIStyledButton *> tabs;
    std::function<void(int)> changeCallback;
    int chosen{0};
};
