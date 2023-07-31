// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_WEB_API_H
#define RAIL_SDK_RAIL_WEB_API_H

#include <string>

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_web_api_define.h"

namespace rail {

// web api
// web_request_method: [web-api-dns]/web/[feature set]/[class]/[methods]?[parameters]
// e.g.: http://api.rail.tgp.qq.com/web/in_game_purchase/order/query_order?
//       rail_order_id=xxx&rail_game_id=xxx&rail_id=xxx&rail_access_key_id=xxx&rail_signature=xxx

// OAUTH web api
// e.g.: https://api.rail.tgp.qq.com/web/oauth2.0/user/get_railid?access_token=xxx

inline std::string RailGetWebAPIUrl(EnumRailPlatformType platform_type,
                    const char* web_api_method) {
    std::string url = platform_type == kRailPlatformQQGame ? kRailWebAPIQQGame : kRailWebAPITGP;
    url += web_api_method;
    return url;
}

// ///////////////////////////////////////////////////////////////////
// web api

// [in-game purchase]
static const char* kRailWebAPI_InGamePurchase_Order_CreateOrder =
    "/web/in_game_purchase/order/create_order";
static const char* kRailWebAPI_InGamePurchase_Order_FinishOrder =
    "/web/in_game_purchase/order/finish_order";
static const char* kRailWebAPI_InGamePurchase_Order_QueryOrder =
    "/web/in_game_purchase/order/query_order";

// [session keeper]
static const char* kRailWebAPI_SessionKeeper_SessionTicket_AuthTicket =
    "/web/session_keeper/session_ticket/auth_ticket";
static const char* kRailWebAPI_SessionKeeper_SessionTicket_Hello =
    "/web/session_keeper/session_ticket/hello";

// [user]
static const char* kRailWebAPI_User_Ownership_QueryRelativeGameidsOwnership =
    "/web/user/ownership/query_relative_gameids_ownership";

// [user_profile]
static const char* kRailWebAPI_UserProfile_Profile_GetUserCountryInfo =
    "/web/user_profile/profile/get_user_country_info";
static const char* kRailWebAPI_UserProfile_Friend_GetUserFriendList =
    "/web/user_profile/friend/get_user_friend_list";
static const char* kRailWebAPI_UserProfile_Friend_GetFriendsProfile =
    "/web/user_profile/friend/get_friends_profile";
static const char* kRailWebAPI_UserProfile_Profile_GetPlayerProfile =
    "/web/user_profile/profile/get_player_profile";

// [assets]
static const char* kRailWebAPI_AssetsService_Assets_AddAssets =
    "/web/assets_service/assets/add_assets";
static const char* kRailWebAPI_AssetsService_Assets_DistributePromoAssets =
    "/web/assets_service/assets/add_promo_assets";
static const char* kRailWebAPI_AssetsService_Assets_QueryEligiblePromoProduct =
    "/web/assets_service/assets/query_qualified_promo_product_class_ids";
static const char* kRailWebAPI_AssetsService_Assets_TriggerAssetsDrop =
    "/web/assets_service/assets/trigger_assets_drop";
static const char* kRailWebAPI_AssetsService_Assets_ConsumeAsset =
    "/web/assets_service/assets/consume_asset";
static const char* kRailWebAPI_AssetsService_Assets_QueryAllAssets =
    "/web/assets_service/assets/query_all_assets";
static const char* kRailWebAPI_AssetsService_Assets_GetProductClassInfo =
    "/web/assets_service/assets/get_product_class_info";
static const char* kRailWebAPI_AssetsService_Assets_ExchangeAssets =
    "/web/assets_service/assets/exchange_assets";

// [play_round]
static const char* kRailWebAPI_PlayRound_Rouds_ReportRound = "/web/play_round/rounds/report_round";
static const char* kRailWebAPI_PlayRound_Rouds_QueryRound = "/web/play_round/rounds/query_round";

// [game_comments]
static const char* kRailWebAPI_UserSpace_Comments_GameComments_GetComment =
    "/web/user_space/comments/game_comments/get_comment";

// [anti cheat]
static const char* kRailWebAPI_AntiCheat_CheatReporting_ReportPlayerCheating =
    "/web/anti_cheat/cheat_reporting/report_player_cheating";
static const char* kRailWebAPI_AntiCheat_CheatReporting_GetCheatingReports =
    "/web/anti_cheat/cheat_report/get_cheating_reports";
static const char* kRailWebAPI_AntiCheat_CheatAction_GameBanOnPlayer =
    "/web/anti_cheat/cheat_action/game_ban_on_player";
static const char* kRailWebAPI_AntiCheat_CheatAction_CancelGameBanOnPlayer =
    "/web/anti_cheat/cheat_action/cancel_game_ban_on_player";
static const char* kRailWebAPI_AntiCheat_BannedStatus_QueryPlayerIsBanned =
    "/web/anti_cheat/banned_status/query_player_is_banned";

// [dirty words]
static const char* kRailWebAPI_DirtyWords_CheckDirtyWords_DirtyWordsFilter =
    "/web/dirty_words/check_dirty_words/dirty_words_filter";
// ///////////////////////////////////////////////////////////////
// OAUTH web api

// [login]
static const char* kRailWebAPI_OAUTH20_Login_Authorize = "/web/oauth2.0/login/authorize";

// [user]
static const char* kRailWebAPI_OAUTH20_User_GetRailID = "/web/oauth2.0/user/get_railid";

};  // namespace rail

#endif  // RAIL_SDK_RAIL_WEB_API_H
