// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_PLAYER_DEFINE_H
#define RAIL_SDK_RAIL_PLAYER_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailPlayerOnLineState {
    kRailOnlineStateUnknown = 0,
    kRailOnlineStateOffLine = 1,  // player is off-line.
    kRailOnlineStateOnLine  = 2,  // player is on-line.
    kRailOnlineStateBusy    = 3,  // player is on-line, but busy.
    kRailOnlineStateLeave   = 4,  // player is auto away.
    kRailOnlineStateGameDefinePlayingState = 5,  // player is in the game define playing state
};

enum EnumRailPlayerOwnershipType {
    kRailPlayerOwnershipTypeNone = 0,
    kRailPlayerOwnershipTypeOwns = 1,
    kRailPlayerOwnershipTypeFree = 2,
    kRailPlayerOwnershipTypeFreeWeekend = 3,
};

enum EnumRailPlayerBannedStatus {
    kRailPlayerBannedStatusUnknown = 0,
    kRailPlayerBannedStatusNormal = 1,  // player's state is normal
    kRailPlayerBannedStatusBannned = 2,  // player is banned for anti cheat
};

enum RailPlayerAccountType {
    kRailPlayerAccountUnknow = 0,
    kRailPlayerAccountQQ = 1,
    kRailPlayerAccountWeChat = 2,
};

struct PlayerPersonalInfo {
    PlayerPersonalInfo() {
        rail_id = 0;
        error_code = kFailure;  // if error_code is not kSuccess, the player's personal
                                // information is not available.
        rail_level = 0;
    }

    RailID rail_id;
    RailResult error_code;
    uint32_t rail_level;
    RailString rail_name;
    RailString avatar_url;
    RailString email_address;
};

struct RailGetAuthenticateURLOptions {
    RailGetAuthenticateURLOptions() {
        client_id = 0;
    }

    RailString url;
    RailString oauth2_state;  // the size of oauth2_state parameter can not more than 64 characters.
    uint64_t client_id;
};

namespace rail_event {

struct AcquireSessionTicketResponse : public RailEvent<kRailEventSessionTicketGetSessionTicket> {
    AcquireSessionTicketResponse() {
    }

    RailSessionTicket session_ticket;
};

struct StartSessionWithPlayerResponse : public RailEvent<kRailEventSessionTicketAuthSessionTicket> {
    StartSessionWithPlayerResponse() {
        remote_rail_id = 0;
    }

    RailID remote_rail_id;
};

struct PlayerGetGamePurchaseKeyResult : public RailEvent<kRailEventPlayerGetGamePurchaseKey> {
    PlayerGetGamePurchaseKeyResult() {
        result = kErrorPlayerGameNotSupportPurchaseKey;
    }

    RailString purchase_key;
};

struct QueryPlayerBannedStatus : public RailEvent<kRailEventQueryPlayerBannedStatus> {
    QueryPlayerBannedStatus() {
        status = kRailPlayerBannedStatusUnknown;
    }

    EnumRailPlayerBannedStatus status;
};

struct GetAuthenticateURLResult : public RailEvent<kRailEventPlayerGetAuthenticateURL> {
    GetAuthenticateURLResult() {
        result = kFailure;
        ticket_expire_time = 0;
    }

    RailString source_url;
    RailString authenticate_url;
    uint32_t ticket_expire_time;  // UTC time
};

struct RailAntiAddictionGameOnlineTimeChanged
    : public RailEvent<kRailEventPlayerAntiAddictionGameOnlineTimeChanged> {
    RailAntiAddictionGameOnlineTimeChanged() {
        game_online_time_count_minutes = 0;
    }

    uint32_t game_online_time_count_minutes;
};

struct RailGetEncryptedGameTicketResult
    : public RailEvent<kRailEventPlayerGetEncryptedGameTicketResult> {
    RailGetEncryptedGameTicketResult() {
        result = kFailure;
    }

    RailString encrypted_game_ticket;
};

struct RailGetPlayerMetadataResult
    : public RailEvent<kRailEventPlayerGetPlayerMetadataResult> {
    RailGetPlayerMetadataResult() {
        result = kFailure;
    }

    RailArray<RailKeyValue> key_values;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_PLAYER_DEFINE_H
