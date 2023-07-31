// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FACTORY_H
#define RAIL_SDK_RAIL_FACTORY_H

#include "rail/sdk/rail_achievement.h"
#include "rail/sdk/rail_apps.h"
#include "rail/sdk/rail_assets.h"
#include "rail/sdk/rail_browser.h"
#include "rail/sdk/rail_dlc.h"
#include "rail/sdk/rail_floating_window.h"
#include "rail/sdk/rail_friends.h"
#include "rail/sdk/rail_game.h"
#include "rail/sdk/rail_game_server.h"
#include "rail/sdk/rail_group_chat.h"
#include "rail/sdk/rail_http_session.h"
#include "rail/sdk/rail_ime_helper.h"
#include "rail/sdk/rail_in_game_coin.h"
#include "rail/sdk/rail_in_game_purchase.h"
#include "rail/sdk/rail_in_game_store_purchase.h"
#include "rail/sdk/rail_leaderboard.h"
#include "rail/sdk/rail_network.h"
#include "rail/sdk/rail_player.h"
#include "rail/sdk/rail_room.h"
#include "rail/sdk/rail_screenshot.h"
#include "rail/sdk/rail_small_object_service.h"
#include "rail/sdk/rail_statistic.h"
#include "rail/sdk/rail_storage.h"
#include "rail/sdk/rail_system.h"
#include "rail/sdk/rail_text_input.h"
#include "rail/sdk/rail_user_space.h"
#include "rail/sdk/rail_users.h"
#include "rail/sdk/rail_utils.h"
#include "rail/sdk/rail_voice_channel.h"
#include "rail/sdk/rail_zone_server.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailFactory {
  public:
    virtual IRailPlayer* RailPlayer() = 0;
    virtual IRailUsersHelper* RailUsersHelper() = 0;

    virtual IRailFriends* RailFriends() = 0;

    virtual IRailFloatingWindow* RailFloatingWindow() = 0;
    virtual IRailBrowserHelper* RailBrowserHelper() = 0;

    virtual IRailInGamePurchase* RailInGamePurchase() = 0;
    virtual IRailInGameCoin* RailInGameCoin() = 0;

    virtual IRailRoomHelper* RailRoomHelper() = 0;
    virtual IRailGameServerHelper* RailGameServerHelper() = 0;

    virtual IRailStorageHelper* RailStorageHelper() = 0;
    virtual IRailUserSpaceHelper* RailUserSpaceHelper() = 0;

    virtual IRailStatisticHelper* RailStatisticHelper() = 0;
    virtual IRailLeaderboardHelper* RailLeaderboardHelper() = 0;
    virtual IRailAchievementHelper* RailAchievementHelper() = 0;

    virtual IRailNetwork* RailNetworkHelper() = 0;

    virtual IRailApps* RailApps() = 0;
    virtual IRailGame* RailGame() = 0;
    virtual IRailUtils* RailUtils() = 0;

    virtual IRailAssetsHelper* RailAssetsHelper() = 0;

    virtual IRailDlcHelper* RailDlcHelper() = 0;

    virtual IRailScreenshotHelper* RailScreenshotHelper() = 0;

    virtual IRailVoiceHelper* RailVoiceHelper() = 0;

    virtual IRailSystemHelper* RailSystemHelper() = 0;

    virtual IRailTextInputHelper* RailTextInputHelper() = 0;

    virtual IRailIMEHelper* RailIMETextInputHelper() = 0;

    virtual IRailHttpSessionHelper* RailHttpSessionHelper() = 0;

    virtual IRailSmallObjectServiceHelper* RailSmallObjectServiceHelper() = 0;

    virtual IRailZoneServerHelper* RailZoneServerHelper() = 0;

    virtual IRailGroupChatHelper* RailGroupChatHelper() = 0;

    virtual IRailInGameStorePurchaseHelper* RailInGameStorePurchaseHelper() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_FACTORY_H
