// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_USERS_H
#define RAIL_SDK_RAIL_USERS_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_users_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

// rail users helper singleton
class IRailUsersHelper {
  public:
    // get users' information on the platform asynchronously,
    // such as nickname, URL address of avatar, and on-line status.
    // users need to register and will received callback data is RailUsersInfoData.
    virtual RailResult AsyncGetUsersInfo(const RailArray<RailID>& rail_ids,
                        const RailString& user_data) = 0;

    // Asynchronously invite another user
    //    developers call this to send an invitation, the receiver should call
    //       AsyncGetInviteDetail to get command line set by this methods, invite user count
    //       limit one time is kRailCommonUsersInviteMaxUsersOnce
    //
    //    command_line  kRailInviteTypeGame(invite_type)   command line can be any string(To keep
    //                      compatibility with AsyncSetInviteCommandLine/AsyncGetInviteCommandLine)
    //                      when launch game, TGP will help generate command line parameter
    //                      "--rail_connect_to_railid=[A_railid] --rail_connect_cmd=[command_line]".
    //
    //                      Recommend to use AsyncGetInviteDetail to get value, but is compatible
    //                         with AsyncGetInviteCommandLine.(may be deprecated in the future)
    //
    //                  kRailInviteTypeRoom(invite_type)  command line must has
    //                      " --rail_connect_to_roomid=[room id]"," --rail_room_password=[password]"
    //                      parameters other than these is permitted if added to the end of these.
    //                      The parameter set by the called is directly passed when launch game.
    //
    //                      use AsyncGetInviteDetail to get value
    //
    //    options   Set invite type, and command line arguments should follow the rules before
    //
    //    caller(inviter) will receive result with a callback "RailUsersInviteUsersResult",
    //                  then when an invitee accept or decline the invitation, caller(inviter)
    //                  will receive a "RailUsersInviteJoinGameResult"(more than once)
    //
    //    invitee      when invitee accept, game will be launched with command line parameters
    //                 set by inviter when not running if need_launch_game is set,
    //                 or when game is running, game will receive
    //                 a callback "RailUsersRespondInvitation"
    virtual RailResult AsyncInviteUsers(const RailString& command_line,
                        const RailArray<RailID>& users,
                        const RailInviteOptions& options,
                        const RailString& user_data) = 0;

    // Asynchronously get invitation detail information(command line) set by the inviter
    //
    //    callback is "RailUsersGetInviteDetailResult"
    virtual RailResult AsyncGetInviteDetail(const RailID& inviter,
                        EnumRailUsersInviteType invite_type,
                        const RailString& user_data) = 0;

    // Asynchronously cancel my invitation
    //
    //     callback is "RailUsersCancelInviteResult"
    virtual RailResult AsyncCancelInvite(EnumRailUsersInviteType invite_type,
                        const RailString& user_data) = 0;

    // Asynchronously cancel all my invitation
    //
    //     callback is "RailUsersCancelInviteResult"
    virtual RailResult AsyncCancelAllInvites(const RailString& user_data) = 0;

    virtual RailResult AsyncGetUserLimits(const RailID& user_id, const RailString& user_data) = 0;

    // show chat window with your friend in game. Callback is RailShowChatWindowWithFriendResult.
    virtual RailResult AsyncShowChatWindowWithFriend(const RailID& rail_id,
                        const RailString& user_data) = 0;

    // show other user's homepage window in game. Callback is RailShowUserHomepageWindowResult.
    virtual RailResult AsyncShowUserHomepageWindow(const RailID& rail_id,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_USERS_H
