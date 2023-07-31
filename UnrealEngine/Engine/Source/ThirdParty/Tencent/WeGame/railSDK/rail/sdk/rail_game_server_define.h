// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GAME_SERVER_DEFINE_H
#define RAIL_SDK_RAIL_GAME_SERVER_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct GameServerInfo {
    GameServerInfo() {
        Reset();
    }

    void Reset() {
        game_server_rail_id = 0;
        owner_rail_id = 0;
        is_dedicated = false;
        game_server_name.clear();
        game_server_map.clear();
        has_password = false;
        is_friend_only = false;
        max_players = 0;
        current_players = 0;
        bot_players = 0;
        server_host.clear();
        server_fullname.clear();
        server_description.clear();
        server_tags.clear();
        server_version.clear();
        spectator_host.clear();
        server_info.clear();
        server_mods.clear();
        server_kvs.clear();
    }

    RailID game_server_rail_id;
    RailID owner_rail_id;
    bool is_dedicated;
    RailString game_server_name;
    RailString game_server_map;
    bool has_password;
    bool is_friend_only;
    uint32_t max_players;
    uint32_t current_players;  // current_players includes bot_players
    uint32_t bot_players;
    RailString server_host;
    RailString server_fullname;
    RailString server_description;
    RailString server_tags;
    RailString server_version;
    RailString spectator_host;
    RailString server_info;

    RailArray<RailString> server_mods;
    RailArray<RailKeyValue> server_kvs;
};

struct CreateGameServerOptions {
    explicit CreateGameServerOptions() {
        enable_team_voice = true;
        has_password = false;
    }

    bool enable_team_voice;
    bool has_password;
};

enum GameServerListSorterKeyType {
    kGameServerListSorterKeyTypeCustom = 1,
    kGameServerListSorterGameServerName = 2,
    kGameServerListSorterCurrentPlayerNumber = 3,
};

struct GameServerListSorter {
    GameServerListSorter() {
        sorter_key_type = kGameServerListSorterKeyTypeCustom;
        sort_value_type = kRailPropertyValueTypeString;
        sort_type  = kRailSortTypeAsc;
    }

    // key type
    GameServerListSorterKeyType sorter_key_type;

    // sort by key
    RailString  sort_key;
    EnumRailPropertyValueType sort_value_type;

    // sort type [ASC or DESC]
    EnumRailSortType sort_type;
};

struct GameServerListFilterKey {
    GameServerListFilterKey() {
        value_type = kRailPropertyValueTypeString;
        comparison_type = kRailComparisonTypeEqualToOrLessThan;
    }

    RailString  key_name;  // filter key name
    EnumRailPropertyValueType value_type;  // value of 'key'(indicated by key_name), type of value
    // comparison type between value( value of 'key') and filter_value
    EnumRailComparisonType comparison_type;
    RailString filter_value;  // user define filter value
};

struct GameServerListFilter {
    GameServerListFilter() {
        filter_dedicated_server = kRailOptionalAny;
        filter_password = kRailOptionalAny;
        filter_friends_created = kRailOptionalAny;
    }

    // all filters below are logic AND relationship
    // example:
    // filters AND owner_id(if owner_id is valid) AND
    //
    //     delicated_server_only(if filter_dedicated_server = kRailOptionalYes) AND
    //     not_delicated_server(if filter_dedicated_server = kRailOptionalNo) AND
    //     not_care_whether_delicated_server(if filter_dedicated_server = kRailOptionalAny) AND
    //
    // filter_game_server_name(if filter_game_server_name is not empty) AND
    // filter_game_server_map(if filter_game_server_map is not empty) AND
    // filter_game_server_host(if filter_game_server_host is not empty) AND
    //
    //     has_password_only(if filter_password = kRailOptionalYes) AND
    //     not_has_password(if filter_password = kRailOptionalNo) AND
    //     not_care_whether_password(if filter_password = kRailOptionalAny) AND
    //
    //     friends_created_only(if filter_friends_created = kRailOptionalYes) AND
    //     not_friends_created(if filter_friends_created = kRailOptionalNo) AND
    //     not_care_whether_friends_created(if filter_friends_created = kRailOptionalAny)

    // user define filter condition
    RailArray<GameServerListFilterKey> filters;  // filter by all conditions in filters array
                                                 // filters[0] AND filters[1] AND ... AND filters[N]
    // filter for someone's game server
    RailID owner_id;
    // filter game servers whether dedicated or not
    EnumRailOptionalValue filter_dedicated_server;
    RailString filter_game_server_name;
    RailString filter_game_server_map;
    // filter_game_server_host should be the same string you set by IRailGameServer::SetHost
    RailString filter_game_server_host;
    // filter game servers whether have password or not
    EnumRailOptionalValue filter_password;
    // filter game servers whether created by friends or not
    EnumRailOptionalValue filter_friends_created;
    // filter game servers whose tags contains all tags in tags_contained
    // tags_contained is comma-delimited
    RailString tags_contained;
    // filter game servers whose tags doesn't contains any tag in tags_not_contained
    // tags_not_contained is comma-delimited
    RailString tags_not_contained;
};

struct GameServerPlayerInfo {
    GameServerPlayerInfo() {
        member_score = 0;
    }

    RailID member_id;
    RailString member_nickname;
    int64_t member_score;
};

namespace rail_event {

struct AsyncAcquireGameServerSessionTicketResponse :
    public RailEvent<kRailEventGameServerGetSessionTicket> {
    AsyncAcquireGameServerSessionTicketResponse() {
    }

    RailSessionTicket session_ticket;
};

struct GameServerStartSessionWithPlayerResponse :
    public RailEvent<kRailEventGameServerAuthSessionTicket> {
    GameServerStartSessionWithPlayerResponse() {
        remote_rail_id = 0;
    }

    RailID remote_rail_id;
};

// AsyncCreateGameServer result
struct CreateGameServerResult :
    public RailEvent<kRailEventGameServerCreated> {
    CreateGameServerResult() {
    }

    RailID game_server_id;
};

// set gameserver meta data result
struct SetGameServerMetadataResult :
    public RailEvent<kRailEventGameServerSetMetadataResult> {
    SetGameServerMetadataResult() {
    }

    RailID game_server_id;
};

// get gameserver meta data result
struct GetGameServerMetadataResult :
    public RailEvent<kRailEventGameServerGetMetadataResult> {
    GetGameServerMetadataResult() {
    }

    RailID game_server_id;
    RailArray<RailKeyValue> key_value;
};

// RegisterToGameServerList callback event
struct GameServerRegisterToServerListResult:
    public RailEvent<kRailEventGameServerRegisterToServerListResult> {
    GameServerRegisterToServerListResult() {
    }
};

// gameserver player list
struct GetGameServerPlayerListResult : public RailEvent<kRailEventGameServerPlayerListResult> {
    GetGameServerPlayerListResult() {
    }

    RailID game_server_id;
    RailArray<GameServerPlayerInfo> server_player_info;
};

// gameserver list
struct GetGameServerListResult : public RailEvent<kRailEventGameServerListResult> {
    GetGameServerListResult() {
        start_index = 0;
        end_index = 0;
        total_num = 0;
    }

    uint32_t start_index;
    uint32_t end_index;
    uint32_t total_num;
    RailArray<GameServerInfo> server_info;
};

// FavoriteGameServers
struct AsyncGetFavoriteGameServersResult
    : public RailEvent<kRailEventGameServerFavoriteGameServers> {
    AsyncGetFavoriteGameServersResult() {
    }

    RailArray<RailID> server_id_array;
};

struct AsyncAddFavoriteGameServerResult
    : public RailEvent<kRailEventGameServerAddFavoriteGameServer> {
    AsyncAddFavoriteGameServerResult() {
        server_id = 0;
    }

    RailID server_id;
};

struct AsyncRemoveFavoriteGameServerResult
    : public RailEvent<kRailEventGameServerRemoveFavoriteGameServer> {
    AsyncRemoveFavoriteGameServerResult() {
        server_id = 0;
    }

    RailID server_id;
};
}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GAME_SERVER_DEFINE_H
