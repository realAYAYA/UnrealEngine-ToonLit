// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_EVENT_H
#define RAIL_SDK_RAIL_EVENT_H

#include <stddef.h>

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_result.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum RAILEventID {
    kRailEventBegin = 0,
    // [1, 2000), reserved for rail platform events
    kRailEventFinalize = 1,
    kRailEventSystemStateChanged = 2,
    kRailPlatformNotifyEventJoinGameByGameServer = 100,
    kRailPlatformNotifyEventJoinGameByRoom = 101,
    kRailPlatformNotifyEventJoinGameByUser = 102,

    // feature set events begin
    kRailEventStats = 2000,
    kRailEventStatsPlayerStatsReceived = 2001,
    kRailEventStatsPlayerStatsStored = 2002,
    kRailEventStatsNumOfPlayerReceived = 2003,
    kRailEventStatsGlobalStatsReceived = 2004,

    // achievement
    kRailEventAchievement = 2100,
    kRailEventAchievementPlayerAchievementReceived = 2101,
    kRailEventAchievementPlayerAchievementStored = 2102,
    kRailEventAchievementGlobalAchievementReceived = 2103,

    // leaderboard
    kRailEventLeaderboard = 2200,
    kRailEventLeaderboardReceived = 2201,
    kRailEventLeaderboardEntryReceived = 2202,
    kRailEventLeaderboardUploaded = 2203,
    kRailEventLeaderboardAttachSpaceWork = 2204,
    kRailEventLeaderboardAsyncCreated = 2205,

    // game server
    kRailEventGameServer = 3000,
    kRailEventGameServerListResult = 3001,
    kRailEventGameServerCreated = 3002,
    kRailEventGameServerSetMetadataResult = 3003,
    kRailEventGameServerGetMetadataResult = 3004,
    kRailEventGameServerGetSessionTicket = 3005,
    kRailEventGameServerAuthSessionTicket = 3006,
    kRailEventGameServerPlayerListResult = 3007,
    kRailEventGameServerRegisterToServerListResult = 3008,
    kRailEventGameServerFavoriteGameServers = 3009,
    kRailEventGameServerAddFavoriteGameServer = 3010,
    kRailEventGameServerRemoveFavoriteGameServer = 3011,

    // user space
    kRailEventUserSpace = 4000,
    kRailEventUserSpaceGetMySubscribedWorksResult = 4001,
    kRailEventUserSpaceGetMyFavoritesWorksResult = 4002,
    kRailEventUserSpaceQuerySpaceWorksResult = 4003,
    kRailEventUserSpaceUpdateMetadataResult = 4004,
    kRailEventUserSpaceSyncResult = 4005,
    kRailEventUserSpaceSubscribeResult = 4006,
    kRailEventUserSpaceModifyFavoritesWorksResult = 4007,
    kRailEventUserSpaceRemoveSpaceWorkResult = 4008,
    kRailEventUserSpaceVoteSpaceWorkResult = 4009,
    kRailEventUserSpaceSearchSpaceWorkResult = 4010,
    kRailEventUserSpaceQuerySpaceWorksResultV2 = 4011,
    kRailEventUserSpaceDownloadProgress = 4012,
    kRailEventUserSpaceDownloadResult = 4013,
    kRailEventUserSpaceRateSpaceWorkResult = 4014,

    // net channel
    kRailEventNetChannel = 5000,
    kRailEventNetChannelCreateChannelResult = 5001,
    kRailEventNetChannelInviteJoinChannelRequest = 5002,
    kRailEventNetChannelJoinChannelResult = 5003,
    kRailEventNetChannelChannelException = 5004,
    kRailEventNetChannelChannelNetDelay = 5005,
    kRailEventNetChannelInviteMemmberResult = 5006,
    kRailEventNetChannelMemberStateChanged = 5007,

    // storage
    kRailEventStorageBegin = 6000,
    kRailEventStorageQueryQuotaResult = 6001,
    kRailEventStorageShareToSpaceWorkResult = 6002,
    kRailEventStorageAsyncReadFileResult = 6003,
    kRailEventStorageAsyncWriteFileResult = 6004,
    kRailEventStorageAsyncListStreamFileResult = 6005,
    kRailEventStorageAsyncRenameStreamFileResult = 6006,
    kRailEventStorageAsyncDeleteStreamFileResult = 6007,
    kRailEventStorageAsyncReadStreamFileResult = 6008,
    kRailEventStorageAsyncWriteStreamFileResult = 6009,

    // assets
    kRailEventAssetsBegin = 7000,
    kRailEventAssetsRequestAllAssetsFinished = 7001,
    kRailEventAssetsCompleteConsumeByExchangeAssetsToFinished = 7002,
    kRailEventAssetsExchangeAssetsFinished = 7003,
    kRailEventAssetsExchangeAssetsToFinished = 7004,
    kRailEventAssetsDirectConsumeFinished = 7005,
    kRailEventAssetsStartConsumeFinished = 7006,
    kRailEventAssetsUpdateConsumeFinished = 7007,
    kRailEventAssetsCompleteConsumeFinished = 7008,
    kRailEventAssetsSplitFinished = 7009,
    kRailEventAssetsSplitToFinished = 7010,
    kRailEventAssetsMergeFinished = 7011,
    kRailEventAssetsMergeToFinished = 7012,
    kRailEventAssetsRequestAllProductFinished = 7013,
    kRailEventAssetsUpdateAssetPropertyFinished = 7014,
    kRailEventAssetsAssetsChanged = 7015,

    // utils
    kRailEventUtilsBegin = 8000,
    kRailEventUtilsSignatureResult = 8002,
    kRailEventUtilsGetImageDataResult = 8003,
    kRailEventUtilsGameSettingMetadataChanged = 8004,

    // in-game purchase
    kRailEventInGamePurchaseBegin = 9000,
    kRailEventInGamePurchaseAllProductsInfoReceived = 9001,
    kRailEventInGamePurchaseAllPurchasableProductsInfoReceived = 9002,
    kRailEventInGamePurchasePurchaseProductsResult = 9003,
    kRailEventInGamePurchaseFinishOrderResult = 9004,
    kRailEventInGamePurchasePurchaseProductsToAssetsResult = 9005,

    // in-game store purchase
    kRailEventInGameStorePurchaseBegin = 9500,
    kRailEventInGameStorePurchasePayWindowDisplayed = 9501,
    kRailEventInGameStorePurchasePayWindowClosed = 9502,
    kRailEventInGameStorePurchasePaymentResult = 9503,

    // room
    kRailEventRoom = 10000,
    // kRailEventRoomGetRoomGroupListResult = 10001,  // not used.
    kRailEventRoomGetRoomListResult = 10002,  // rename from kRailEventRoomListResult, 2018/11/27.
    kRailEventRoomCreated = 10003,
    kRailEventRoomGetRoomMembersResult = 10004,  // rename from
                                                 // kRailEventRoomGotRoomMembers, 2018/11/27.
    kRailEventRoomJoinRoomResult = 10005,
    kRailEventRoomKickOffMemberResult = 10006,
    kRailEventRoomSetRoomMetadataResult = 10007,
    kRailEventRoomGetRoomMetadataResult = 10008,
    kRailEventRoomGetMemberMetadataResult = 10009,
    kRailEventRoomSetMemberMetadataResult = 10010,
    kRailEventRoomLeaveRoomResult = 10011,
    kRailEventRoomGetAllDataResult = 10012,
    kRailEventRoomGetUserRoomListResult = 10013,
    kRailEventRoomClearRoomMetadataResult = 10014,
    kRailEventRoomOpenRoomResult = 10015,
    kRailEventRoomSetRoomTagResult = 10016,
    kRailEventRoomGetRoomTagResult = 10017,
    kRailEventRoomSetNewRoomOwnerResult = 10018,
    kRailEventRoomSetRoomTypeResult = 10019,
    kRailEventRoomSetRoomMaxMemberResult = 10020,

    kRailEventRoomNotify = 11000,
    kRailEventRoomNotifyMetadataChanged = 11001,
    kRailEventRoomNotifyMemberChanged = 11002,
    kRailEventRoomNotifyMemberkicked = 11003,
    kRailEventRoomNotifyRoomDestroyed = 11004,
    kRailEventRoomNotifyRoomOwnerChanged = 11005,
    kRailEventRoomNotifyRoomDataReceived = 11006,
    kRailEventRoomNotifyRoomGameServerChanged = 11007,

    // friends
    kRailEventFriend = 12000,
    kRailEventFriendsDialogShow = 12001,
    kRailEventFriendsSetMetadataResult = 12002,
    kRailEventFriendsGetMetadataResult = 12003,
    kRailEventFriendsClearMetadataResult = 12004,
    kRailEventFriendsGetInviteCommandLine = 12005,
    kRailEventFriendsReportPlayedWithUserListResult = 12006,
    kRailEventFriendsFriendsListChanged = 12010,
    kRailEventFriendsGetFriendPlayedGamesResult = 12011,
    kRailEventFriendsQueryPlayedWithFriendsListResult = 12012,
    kRailEventFriendsQueryPlayedWithFriendsTimeResult = 12013,
    kRailEventFriendsQueryPlayedWithFriendsGamesResult = 12014,
    kRailEventFriendsAddFriendResult = 12015,
    kRailEventFriendsOnlineStateChanged = 12016,
    kRailEventFriendsMetadataChanged = 12017,

    // player
    kRailEventSessionTicket = 13000,
    kRailEventSessionTicketGetSessionTicket = 13001,
    kRailEventSessionTicketAuthSessionTicket = 13002,
    kRailEventPlayerGetGamePurchaseKey = 13003,
    kRailEventQueryPlayerBannedStatus = 13004,
    kRailEventPlayerGetAuthenticateURL = 13005,
    kRailEventPlayerAntiAddictionGameOnlineTimeChanged = 13006,
    kRailEventPlayerGetEncryptedGameTicketResult = 13007,
    kRailEventPlayerGetPlayerMetadataResult = 13008,

    // users
    kRailEventUsersGetUsersInfo = 13501,
    kRailEventUsersNotifyInviter = 13502,
    kRailEventUsersRespondInvitation = 13503,
    kRailEventUsersInviteJoinGameResult = 13504,
    kRailEventUsersInviteUsersResult = 13505,
    kRailEventUsersGetInviteDetailResult = 13506,
    kRailEventUsersCancelInviteResult = 13507,
    kRailEventUsersGetUserLimitsResult = 13508,
    kRailEventUsersShowChatWindowWithFriendResult = 13509,
    kRailEventUsersShowUserHomepageWindowResult = 13510,

    // floating window
    kRailEventShowFloatingWindow = 14000,
    kRailEventShowFloatingNotifyWindow = 14001,

    // browser
    kRailEventBrowser = 15000,
    kRailEventBrowserCreateResult = 15001,
    kRailEventBrowserReloadResult = 15002,
    kRailEventBrowserCloseResult = 15003,
    kRailEventBrowserJavascriptEvent = 15004,
    kRailEventBrowserTryNavigateNewPageRequest = 15005,
    kRailEventBrowserPaint = 15006,
    kRailEventBrowserDamageRectPaint = 15007,
    kRailEventBrowserNavigeteResult = 15008,
    kRailEventBrowserStateChanged = 15009,
    kRailEventBrowserTitleChanged = 15010,

    // network
    kRailEventNetwork = 16000,
    kRailEventNetworkCreateSessionRequest = 16001,
    kRailEventNetworkCreateSessionFailed = 16002,
    kRailEventNetworkCreateRawSessionRequest = 16003,
    kRailEventNetworkCreateRawSessionFailed = 16004,

    // dlc
    kRailEventDlcBegin = 17000,
    kRailEventDlcInstallStart = 17001,
    kRailEventDlcInstallStartResult = 17002,
    kRailEventDlcInstallProgress = 17003,
    kRailEventDlcInstallFinished = 17004,
    kRailEventDlcUninstallFinished = 17005,
    kRailEventDlcCheckAllDlcsStateReadyResult = 17006,
    kRailEventDlcQueryIsOwnedDlcsResult = 17007,
    kRailEventDlcOwnershipChanged = 17008,
    kRailEventDlcRefundChanged = 17009,

    // screenshots
    kRailEventScreenshot = 18000,
    kRailEventScreenshotTakeScreenshotFinished = 18001,
    kRailEventScreenshotTakeScreenshotRequest = 18002,
    kRailEventScreenshotPublishScreenshotFinished = 18003,

    // voice channel
    kRailEventVoiceChannel = 19000,
    kRailEventVoiceChannelCreateResult = 19001,
    kRailEventVoiceChannelDataCaptured = 19002,
    kRailEventVoiceChannelJoinedResult = 19003,
    kRailEventVoiceChannelLeaveResult = 19004,
    kRailEventVoiceChannelAddUsersResult = 19005,
    kRailEventVoiceChannelRemoveUsersResult = 19006,
    kRailEventVoiceChannelInviteEvent = 19007,
    kRailEventVoiceChannelMemberChangedEvent = 19008,
    kRailEventVoiceChannelUsersSpeakingStateChangedEvent = 19009,
    kRailEventVoiceChannelPushToTalkKeyChangedEvent = 19010,
    kRailEventVoiceChannelSpeakingUsersChangedEvent = 19011,

    // app
    kRailEventAppBegin = 20000,
    kRailEventAppQuerySubscribeWishPlayStateResult = 20001,

    // text input
    kRailEventTextInputBegin = 21000,
    kRailEventTextInputShowTextInputWindowResult = 21001,

    // IME helper input
    kRailEventIMEHelperTextInputBegin = 22000,
    kRailEventIMEHelperTextInputSelectedResult = 22001,
    kRailEventIMEHelperTextInputCompositionStateChanged = 22002,

    // http session
    kRailEventHttpSessionBegin = 23000,
    kRailEventHttpSessionResponseResult = 23001,

    // small object service
    kRailEventSmallObjectServiceBegin = 24000,
    kRailEventSmallObjectServiceQueryObjectStateResult = 24001,
    kRailEventSmallObjectServiceDownloadResult = 24002,

    // zone server
    kRailEventZoneServerBegin = 25000,
    kRailEventZoneServerSwitchPlayerSelectedZoneResult = 25001,

    // group chat
    kRailEventGroupChatBegin = 26000,
    kRailEventGroupChatQueryGroupsInfoResult = 26001,
    kRailEventGroupChatOpenGroupChatResult = 26002,

    // in-game coin
    kRailEventInGameCoinBegin = 27000,
    kRailEventInGameCoinRequestCoinInfoResult = 27001,
    kRailEventInGameCoinPurchaseCoinsResult = 27002,

    kCustomEventBegin = 10000000,  // 10,000,000 + customize define events
};

typedef RAILEventID RAIL_EVENT_ID;

// event base class, all event inherit from class EventBase
class EventBase {
  public:
    EventBase() : result(kErrorUnknown) {}
    virtual ~EventBase() {}
    // event_id for dispatch
    virtual RAIL_EVENT_ID get_event_id() = 0;

    RailID rail_id;
    RailGameID game_id;
    RailString user_data;
    RailResult result;

    RailResult get_result() const { return result; }
    void set_result(RailResult r) { result = r; }
};

template<RAIL_EVENT_ID num_event_id>
class RailEvent : public EventBase {
  public:
    RailEvent() {}
    virtual ~RailEvent() {}
    enum { kInternalRailEventEventId = num_event_id };
    virtual RAIL_EVENT_ID get_event_id() { return num_event_id; }
};

// rail event interface
class IRailEvent {
  public:
    // param: refer to other events define
    virtual void OnRailEvent(RAIL_EVENT_ID event_id, EventBase* param) = 0;
};

#define RAIL_REGISTER_EVENT_IMPL(THISCLASS, METHOD, EVENT_DEFINE, \
                            RailRegisterEventImpl, RailUnregisterEventImpl) \
    void METHOD(rail::RAIL_EVENT_ID id, EVENT_DEFINE* event_data); \
class CEventHandler_##METHOD : public rail::IRailEvent { \
  public: \
    explicit CEventHandler_##METHOD() : event_id_(EVENT_DEFINE::kInternalRailEventEventId) { \
        RailRegisterEventImpl(event_id_, this); \
    } \
    virtual ~CEventHandler_##METHOD() { \
        RailUnregisterEventImpl(event_id_, this); \
    } \
    virtual void OnRailEvent(rail::RAIL_EVENT_ID id, rail::EventBase* event_data) { \
        char* parent_addr = reinterpret_cast<char*>(this) - \
            offsetof(THISCLASS, event_handler_##METHOD##_); \
        THISCLASS* parent = reinterpret_cast<THISCLASS*>(parent_addr); \
        parent->METHOD(id, reinterpret_cast<EVENT_DEFINE*>(event_data)); \
    } \
  private: \
    rail::RAIL_EVENT_ID event_id_; \
}; \
CEventHandler_##METHOD event_handler_##METHOD##_;

// use RAIL_REGISTER_EVENT when load rail_api.dll
#define RAIL_REGISTER_EVENT(THISCLASS, METHOD, EVENT_DEFINE, GetImplementedModule) \
    RAIL_REGISTER_EVENT_IMPL(THISCLASS, METHOD, EVENT_DEFINE, \
        rail::helper::Invoker(GetImplementedModule()).RailRegisterEvent, \
        rail::helper::Invoker(GetImplementedModule()).RailUnregisterEvent)

// use RAIL_INTERNAL_REGISTER_EVENT when link rail lib,
// usually you don't need to use it
#define RAIL_INTERNAL_REGISTER_EVENT(THISCLASS, METHOD, EVENT_DEFINE) \
    RAIL_REGISTER_EVENT_IMPL(THISCLASS, METHOD, EVENT_DEFINE, \
                        rail::RailRegisterEvent, rail::RailUnregisterEvent)

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_EVENT_H
