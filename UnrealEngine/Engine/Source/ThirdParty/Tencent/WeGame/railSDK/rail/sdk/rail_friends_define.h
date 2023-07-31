// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FRIENDS_DEFINE_H
#define RAIL_SDK_RAIL_FRIENDS_DEFINE_H

#include "rail/sdk/rail_player_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

const uint32_t kRailMaxQueryPlayedWithFriendsTimeLimit = 20;

// size limits on users played with data
enum EnumRailUserPlayedWithLimit {
    kRailMaxPlayedWithUsersCount = 50,
    kRailMaxPlayedWithUserContentLen = 100,
};

// list of relationships to other user
enum EnumRailFriendType {
    kRailFriendTypeUnknown = 0,       // invalid relationship
    kRailFriendTypeWeGameFriend = 1,  // WeGame friends
    kRailFriendTypeStranger = 2,      // other friends, such QQ or WeChat friends
};

// defines a key-value pair that can be set as parameters to yourself,
// and you can get other users' key-value pairs.
struct RailKeyValueResult {
    RailKeyValueResult() {
        error_code = kFailure;
    }

    RailResult error_code;
    RailString key;
    RailString value;
};

// user who played with you recently
struct RailUserPlayedWith {
    RailUserPlayedWith() {
        rail_id = 0;
    }

    RailID rail_id;
    RailString user_rich_content;  // rich content data support
};

enum RailFriendPlayedGamePlayState {
    kRailFriendPlayedGamePlayStatePlaying = 1,
    kRailFriendPlayedGamePlayStatePlayed = 2,
};

struct RailFriendPlayedGameInfo {
    RailFriendPlayedGameInfo() {
        in_game_server = false;
        in_room = false;
        friend_played_game_play_state = kRailFriendPlayedGamePlayStatePlaying;
    }

    bool in_game_server;
    bool in_room;
    RailArray<uint64_t> game_server_id_list;
    RailArray<uint64_t> room_id_list;
    RailID friend_id;
    RailGameID game_id;
    RailFriendPlayedGamePlayState friend_played_game_play_state;
};

struct RailPlayedWithFriendsTimeItem {
    RailPlayedWithFriendsTimeItem() {
        play_time = 0;
        rail_id = 0;
    }

    uint32_t play_time;
    RailID rail_id;
};

struct RailPlayedWithFriendsGameItem {
    RailPlayedWithFriendsGameItem() {
        rail_id = 0;
    }

    RailArray<RailGameID> game_ids;
    RailID rail_id;
};

struct RailFriendsAddFriendRequest {
    RailFriendsAddFriendRequest() {
        target_rail_id = 0;
    }

    RailID target_rail_id;
    // might implement other option in the future
};

struct RailFriendOnLineState {
    RailFriendOnLineState() {
        friend_rail_id = 0;
        friend_online_state = kRailOnlineStateUnknown;
        game_define_game_playing_state = 0;
    }

    RailID friend_rail_id;
    EnumRailPlayerOnLineState friend_online_state;
    uint32_t game_define_game_playing_state;  // if friend_online_state value is
                                              // kRailOnlineStateGameDefinePlayingState, you could
                                              // get a game define playing state via this parameter.
};

struct RailFriendInfo {
    RailFriendInfo() {
        friend_rail_id = 0;
        friend_type = kRailFriendTypeUnknown;
    }

    RailID friend_rail_id;
    EnumRailFriendType friend_type;
    RailFriendOnLineState online_state;
};

struct RailFriendMetadata {
    RailFriendMetadata() {
        friend_rail_id = 0;
    }

    RailID friend_rail_id;
    RailArray<RailKeyValue> metadatas;
};

namespace rail_event {
// called when the user has finished a set of key-value pairs store.
struct RailFriendsSetMetadataResult : public RailEvent<kRailEventFriendsSetMetadataResult> {
    RailFriendsSetMetadataResult() {
        result = kFailure;
    }
};

// received a user's particular key-value pairs
struct RailFriendsGetMetadataResult : public RailEvent<kRailEventFriendsGetMetadataResult> {
    RailFriendsGetMetadataResult() {
        result = kFailure;
        friend_id = 0;
    }

    RailID friend_id;
    RailArray<RailKeyValueResult> friend_kvs;
};

// called when the user has cleared all the key-value pairs
struct RailFriendsClearMetadataResult : public RailEvent<kRailEventFriendsClearMetadataResult> {
    RailFriendsClearMetadataResult() {
        result = kFailure;
    }
};

// received a command-line string for how the user can connect to a game
struct RailFriendsGetInviteCommandLine : public RailEvent<kRailEventFriendsGetInviteCommandLine> {
    RailFriendsGetInviteCommandLine() {
        result = kFailure;
        friend_id = 0;
    }

    RailID friend_id;
    RailString invite_command_line;
};

// called when the user has reported the list of users played with
struct RailFriendsReportPlayedWithUserListResult :
    public RailEvent<kRailEventFriendsReportPlayedWithUserListResult> {
    RailFriendsReportPlayedWithUserListResult() {
        result = kFailure;
    }
};

// if the friends list is changed, you could call GetFriendsList interface to
// get a updated friends list.
struct RailFriendsListChanged :
    public RailEvent<kRailEventFriendsFriendsListChanged> {
    RailFriendsListChanged() {
        result = kFailure;
    }
};

struct RailFriendsQueryFriendPlayedGamesResult :
    public RailEvent<kRailEventFriendsGetFriendPlayedGamesResult> {
    RailFriendsQueryFriendPlayedGamesResult() {
        result = kFailure;
    }

    RailArray<RailFriendPlayedGameInfo> friend_played_games_info_list;
};

struct RailFriendsQueryPlayedWithFriendsListResult :
    public RailEvent<kRailEventFriendsQueryPlayedWithFriendsListResult> {
    RailFriendsQueryPlayedWithFriendsListResult() {
        result = kFailure;
    }

    RailArray<RailID> played_with_friends_list;
};

struct RailFriendsQueryPlayedWithFriendsTimeResult :
    public RailEvent<kRailEventFriendsQueryPlayedWithFriendsTimeResult> {
    RailFriendsQueryPlayedWithFriendsTimeResult() {
        result = kFailure;
    }

    RailArray<RailPlayedWithFriendsTimeItem> played_with_friends_time_list;
};

struct RailFriendsQueryPlayedWithFriendsGamesResult :
    public RailEvent<kRailEventFriendsQueryPlayedWithFriendsGamesResult> {
    RailFriendsQueryPlayedWithFriendsGamesResult() {
        result = kFailure;
    }

    RailArray<RailPlayedWithFriendsGameItem> played_with_friends_game_list;
};

struct RailFriendsAddFriendResult :
    public RailEvent<kRailEventFriendsAddFriendResult> {
    RailFriendsAddFriendResult() {
        result = kFailure;
        target_rail_id = 0;
    }

    RailID target_rail_id;
};

// if some friend's on-line state changed, you could receive this callback. You can
// then update the friend on-line state showing on your friend overlay.
struct RailFriendsOnlineStateChanged :
    public RailEvent<kRailEventFriendsOnlineStateChanged> {
    RailFriendsOnlineStateChanged() {
    }

    RailFriendOnLineState friend_online_state;
};

struct RailFriendsMetadataChanged : public RailEvent<kRailEventFriendsMetadataChanged> {
    RailFriendsMetadataChanged() {
    }

    RailArray<RailFriendMetadata> friends_changed_metadata;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_FRIENDS_DEFINE_H
