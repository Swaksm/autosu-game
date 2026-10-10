// Copyright (c) 2026, WH, All rights reserved.
#include "CBaseUITabBar.h"

#include "CBaseUIStyledButton.h"

#include <utility>

CBaseUITabBar::CBaseUITabBar(std::string name) : CBaseUIBox(Axis::ROW, std::move(name)) {}

CBaseUITabBar::~CBaseUITabBar() = default;

CBaseUITabBar *CBaseUITabBar::add(std::string name, std::string label) {
    auto *tab = new CBaseUIStyledButton(std::move(label), std::move(name));
    const int index = (int)this->tabs.size();
    tab->setChecked(index == this->chosen);
    tab->setClickCallback([this, index] {
        if(index == this->chosen) return;
        this->setChosen(index);
        if(this->changeCallback) this->changeCallback(index);
    });
    this->tabs.push_back(tab);
    CBaseUIBox::add(tab, BoxItem::fit());
    return this;
}

CBaseUITabBar *CBaseUITabBar::setChosen(int index) {
    if(index < 0 || index >= (int)this->tabs.size()) return this;
    this->chosen = index;
    for(int i = 0; i < (int)this->tabs.size(); i++) this->tabs[i]->setChecked(i == index);
    return this;
}

CBaseUITabBar *CBaseUITabBar::setChangeCallback(std::function<void(int)> callback) {
    this->changeCallback = std::move(callback);
    return this;
}
