#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

#include "AnimationHandler.h"
#include "CBaseUIButton.h"

#include <memory>

class ConVar;

class UIAvatar;
struct UserInfo;

class UserCard2 final : public CBaseUIButton {
    NOCOPY_NOMOVE(UserCard2)
   public:
    UserCard2(i32 user_id);
    ~UserCard2() override;

    void update_userid(i32 new_userid);

    void draw() override;
    void onMouseInside() override;
    void onMouseOutside() override;

    UserInfo *info{nullptr};
    std::unique_ptr<UIAvatar> avatar{nullptr};

   private:
    AnimFloat hover_anim{0.f};
};
