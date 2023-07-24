// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_USERS_DEFINE_H
#define RAIL_SDK_RAIL_USERS_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"
#include "rail/sdk/rail_player_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailUsersInviteType {
    kRailUsersInviteTypeGame   = 1,  // [%inviter%] invite you to game
    kRailUsersInviteTypeRoom   = 2,  // [%inviter%] invite you to room
};

enum EnumRailUsersInviteResponseType {
    kRailInviteResponseTypeUnknown = 0,
    kRailInviteResponseTypeAccepted = 1,
    kRailInviteResponseTypeRejected = 2,
    kRailInviteResponseTypeIgnore = 3,
    kRailInviteResponseTypeTimeout = 4,
};

enum EnumRailUsersLimits {
    kRailUsersLimitsNone = 0,             // no restrictions
    kRailUsersLimitsNoChats = 1,          // all chats is restricted
    kRailUsersLimitsNoTrading = 2,        // can't trade
    kRailUsersLimitsNoRoomChat = 3,       // can't chat in room
    kRailUsersLimitsNoVoiceSpeaking = 4,  // can't use voice to speak
    kRailUsersLimitsNoGameInvited = 5,    // can't be invited
    kRailUsersLimitsNoGameInvitee = 6,    // can't invite other user
};

// Invite options when invite user
//    invite_type, invite type
//
//    other parameters are reserved, keep it unchanged.
struct RailInviteOptions {
    EnumRailUsersInviteType invite_type;
    bool need_respond_in_game;
    RailString additional_message;
    uint32_t expire_time;  // UTC time

    RailInviteOptions() {
        invite_type = kRailUsersInviteTypeGame;
        need_respond_in_game = true;
        additional_message = "";
        expire_time = 60 * 3;  // default is 3 minutes
    }
};

namespace rail_event {

struct RailUsersInfoData : public RailEvent<kRailEventUsersGetUsersInfo> {
    RailUsersInfoData() {
        result = kFailure;
    }
    // maximum count of user_info_list is kRailCommonMaxRepeatedKeys;
    RailArray<PlayerPersonalInfo> user_info_list;
};

// this is sent to game when invitation is triggered by cross
struct RailUsersNotifyInviter : public RailEvent<kRailEventUsersNotifyInviter> {
    RailUsersNotifyInviter() {
        result = kFailure;
        invitee_id = 0;
    }

    RailID invitee_id;  // invitee's rail id
};

// this is sent to game of the invitee side when after cross return the user action
struct RailUsersRespondInvitation : public RailEvent<kRailEventUsersRespondInvitation> {
    RailUsersRespondInvitation() {
        result = kFailure;
        inviter_id = 0;
        response = kRailInviteResponseTypeUnknown;
    }

    RailID inviter_id;  // inviter's rail id
    EnumRailUsersInviteResponseType response;
    RailInviteOptions original_invite_option;  // invite option
};

// this is sent to game of inviter side when the invitee side return the result back
struct RailUsersInviteJoinGameResult :
    public RailEvent<kRailEventUsersInviteJoinGameResult> {
    RailUsersInviteJoinGameResult() {
        result = kFailure;
        invite_type = kRailUsersInviteTypeGame;
        response_value = kRailInviteResponseTypeUnknown;
    }

    RailID invitee_id;  // invitee's rail id
    EnumRailUsersInviteResponseType response_value;
    EnumRailUsersInviteType invite_type;
};

struct RailUsersGetInviteDetailResult:
    public RailEvent<kRailEventUsersGetInviteDetailResult> {
    RailUsersGetInviteDetailResult() {
        result = kFailure;
        invite_type = kRailUsersInviteTypeGame;
    }

    RailID inviter_id;
    RailString command_line;
    EnumRailUsersInviteType invite_type;
};

struct RailUsersCancelInviteResult:
    public RailEvent<kRailEventUsersCancelInviteResult> {
    RailUsersCancelInviteResult() {
        result = kFailure;
        invite_type = kRailUsersInviteTypeGame;
    }

    EnumRailUsersInviteType invite_type;
};

struct RailUsersInviteUsersResult :
    public RailEvent<kRailEventUsersInviteUsersResult> {
    RailUsersInviteUsersResult() {
        result = kFailure;
        invite_type = kRailUsersInviteTypeGame;
    }

    EnumRailUsersInviteType invite_type;
};

struct RailUsersGetUserLimitsResult :
    public RailEvent<kRailEventUsersGetUserLimitsResult> {
    RailUsersGetUserLimitsResult() {
        result = kFailure;
    }

    RailID user_id;
    RailArray<EnumRailUsersLimits> user_limits;
};

struct RailShowChatWindowWithFriendResult
    : public RailEvent<kRailEventUsersShowChatWindowWithFriendResult> {
    RailShowChatWindowWithFriendResult() {
        result = kFailure;
        is_show = false;
    }

    bool is_show;
};

struct RailShowUserHomepageWindowResult
    : public RailEvent<kRailEventUsersShowUserHomepageWindowResult> {
    RailShowUserHomepageWindowResult() {
        result = kFailure;
        is_show = false;
    }

    bool is_show;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_USERS_DEFINE_H
