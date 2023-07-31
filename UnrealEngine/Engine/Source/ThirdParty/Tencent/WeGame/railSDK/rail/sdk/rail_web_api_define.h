// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_WEB_API_DEFINE_H
#define RAIL_SDK_RAIL_WEB_API_DEFINE_H

namespace rail {
// +-----------------------------------------+
// |                Rail Web API             |
// +-----------------------------------------+
// web api domain
static const char* kRailWebAPITGP = "https://api.rail.tgp.qq.com";
static const char* kRailWebAPIQQGame = "https://sf.minigame.qq.com";

static const uint32_t kRailGameOwnershipPermanent = 0xffffffff;

enum EnumRailPlayerBanStatus {
    kRailPlayerBanStatusBanned = 1,
    kRailPlayerBanStatusNotBanned = 2,
};

};  // namespace rail
#endif  // RAIL_SDK_RAIL_WEB_API_DEFINE_H

