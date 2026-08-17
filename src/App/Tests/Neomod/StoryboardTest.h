// Copyright (c) 2026, kiwec, All rights reserved.
#pragma once
#include "App.h"

namespace Mc::Tests {

class StoryboardTest : public App {
    NOCOPY_NOMOVE(StoryboardTest)
   public:
    StoryboardTest();
    ~StoryboardTest() override = default;

    void update() override;

   private:
    void runTests();

    int m_passes{0};
    int m_failures{0};
    bool m_ran{false};
};

}  // namespace Mc::Tests
