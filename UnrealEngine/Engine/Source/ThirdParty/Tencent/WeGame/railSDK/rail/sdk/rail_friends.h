// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FRIENDS_H
#define RAIL_SDK_RAIL_FRIENDS_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_friends_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailFriends {
  public:
    // @desc Get friends' metadata information asynchronously in one batch.
    // The info includes nickname, URL address for avatar and online status etc.
    // The data structure RailUsersInfoData contains the user info retrieved.
    // @param rail_ids A list of friends' rail IDs
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetPersonalInfo(const RailArray<RailID>& rail_ids,
                        const RailString& user_data) = 0;

    // @desc Get the current player's or his friend's metadata asynchronously.
    // @param rail_id The Rail ID for the player or his/her friend
    // @param keys The keys for the metadata to retrive
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetFriendMetadata(const RailID& rail_id,
                        const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // @desc Configure multiple metadata entries for the current user asynchronously.
    // @param key_values The maximum number of keys is kRailCommonMaxRepeatedKeys.
    // The max length for a key is kRailCommonMaxKeyLength
    // The max length for a value is kRailCommonMaxValueLength
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetMyMetadata(const RailArray<RailKeyValue>& key_values,
                        const RailString& user_data) = 0;

    // @desc Clear metadata for the current user including both customized and platform
    // reserved entries. Recommended to call when exiting the game. If the game crashes,
    // the platform will clear all the metadata automatically
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncClearAllMyMetadata(const RailString& user_data) = 0;

    // @desc Set the command arguments to properly launch a game after a player accepts
    // a game invitation
    // @param command_line For example, AsyncSetInviteCommandline("room_id=10001 rail_id=123", "");
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetInviteCommandLine(const RailString& command_line,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get the command line set by the player's friend who sent
    // a gaming invitation.
    // @param rail_id Rail ID of the friend who sent the invitation
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetInviteCommandLine(const RailID& rail_id,
                        const RailString& user_data) = 0;

    // @desc Report information of players who played with you recently to
    // show on the game platform.
    // The max length of the string reported is RailMaxPlayedWithUserContentLen
    // For better efficiency, it is recommended to report info for all players in the same room
    // at once. If new players join the room, just dynamically report info of the new players.
    // @param player_list A list of players who played the same game, including the current player.
    // The max length for the list is kRailMaxPlayedWithUsersCount
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncReportPlayedWithUserList(
                        const RailArray<RailUserPlayedWith>& player_list,
                        const RailString& user_data) = 0;

    // @desc Synchronously get a list of friends who also owns the game.
    // For example, A, B, C and D are mutual friends on the platform. A, B and C own the game but
    // not D. If A calls the interface, RailFriendInfo of B and C will be retrieved. Please note
    // the RailID in RailFriendInfo may not be the same as the friend's login RailID.
    // Once the game client is started, the friends list will be automatically updated. When the
    // update is done, you will receive the callback RailFriendsListChanged or the result
    // kErrorFriendsServerBusy.
    // This interface is supposed to be called on the callback RailFriendsListChanged
    // @param friends_list The friend list retrieved.
    // @return Returns kSuccess on success
    virtual RailResult GetFriendsList(RailArray<RailFriendInfo>* friends_list) = 0;

    // @desc Asynchronously get the info of games a friend is playing, such as game ID,
    // room ID and game server ID.
    // @param rail_id The RailID of a friend
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryFriendPlayedGamesInfo(const RailID& rail_id,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get a list of friends who have played with the current player.
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryPlayedWithFriendsList(const RailString& user_data) = 0;

    // @desc Asynchronously get the total time the specified friend has played with
    // the current player
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryPlayedWithFriendsTime(const RailArray<RailID>& rail_ids,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get a list of games for each friend who has played with
    // the current player
    // @param rail_ids A list of friends' RAIL IDs
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryPlayedWithFriendsGames(const RailArray<RailID>& rail_ids,
                        const RailString& user_data) = 0;

    // @desc Asynchronous interface to add a friend
    // The player who received the adding-friend request will get kRailEventFriendsAddFriendResult
    // To refresh the friend list, just wait for the event kRailEventFriendsNotifyBuddyListChanged
    // A popup will show when a player receives a adding-friend invitation. If
    // the player misses the popup, he/she can later check the message in the notification center.
    // @param request Contains the friend's RailID
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncAddFriend(const RailFriendsAddFriendRequest& request,
                        const RailString& user_data) = 0;

    // @desc Update the friend list asynchronously. Upon kRailEventFriendsFriendsListChanged,
    // you can call GetFriendsList to get the updated friend list
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncUpdateFriendsData(const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_FRIENDS_H
