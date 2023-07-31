// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GAME_SERVER_H
#define RAIL_SDK_RAIL_GAME_SERVER_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_game_server_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailGameServer;

class IRailGameServerHelper {
  public:
    // trigger event GetGameServerPlayerListResult
    virtual RailResult AsyncGetGameServerPlayerList(const RailID& gameserver_rail_id,
                        const RailString& user_data) = 0;

    // trigger event GetGameServerListResult
    // game server will be returned when any conditions in alternative_filters matching
    // alternative_filters[0] OR alternative_filters[1] OR ... OR alternative_filters[N]
    virtual RailResult AsyncGetGameServerList(uint32_t start_index,
                        uint32_t end_index,
                        const RailArray<GameServerListFilter>& alternative_filters,
                        const RailArray<GameServerListSorter>& sorter,
                        const RailString& user_data) = 0;

    // trigger event CreateGameServerResult
    virtual IRailGameServer* AsyncCreateGameServer(
                                const CreateGameServerOptions& options = CreateGameServerOptions(),
                                const RailString& game_server_name = "",
                                const RailString& user_data = "") = 0;

    // async get ids for game which is collected
    virtual RailResult AsyncGetFavoriteGameServers(const RailString& user_data = "") = 0;

    // collect a game for later use
    virtual RailResult AsyncAddFavoriteGameServer(const RailID& game_server_id,
                        const RailString& user_data = "") = 0;

    // remove a game id from collection
    virtual RailResult AsyncRemoveFavoriteGameServer(const RailID& game_server_id,
                        const RailString& user_Data = "") = 0;
};

class IRailGameServer : public IRailComponent {
  public:
    virtual const RailID GetGameServerRailID() = 0;

    virtual RailResult GetGameServerName(RailString* name) = 0;

    // user can favorite the game server by full name
    virtual RailResult GetGameServerFullName(RailString* full_name) = 0;

    virtual const RailID GetOwnerRailID() = 0;

    //
    // optional property data
    //

    virtual bool SetHost(const RailString& game_server_host) = 0;

    virtual bool GetHost(RailString* game_server_host) = 0;

    virtual bool SetMapName(const RailString& game_server_map) = 0;

    virtual bool GetMapName(RailString* game_server_map) = 0;

    virtual bool SetPasswordProtect(bool has_password) = 0;

    virtual bool GetPasswordProtect() = 0;

    virtual bool SetMaxPlayers(uint32_t max_player_count) = 0;

    virtual uint32_t GetMaxPlayers() = 0;

    virtual bool SetBotPlayers(uint32_t bot_player_count) = 0;

    virtual uint32_t GetBotPlayers() = 0;

    virtual bool SetGameServerDescription(const RailString& game_server_description) = 0;

    virtual bool GetGameServerDescription(RailString* game_server_description) = 0;

    virtual bool SetGameServerTags(const RailString& game_server_tags) = 0;

    virtual bool GetGameServerTags(RailString* game_server_tags) = 0;

    virtual bool SetMods(const RailArray<RailString>& server_mods) = 0;

    virtual bool GetMods(RailArray<RailString>* server_mods) = 0;

    virtual bool SetSpectatorHost(const RailString& spectator_host) = 0;

    virtual bool GetSpectatorHost(RailString* spectator_host) = 0;

    virtual bool SetGameServerVersion(const RailString& version) = 0;

    virtual bool GetGameServerVersion(RailString* version) = 0;

    virtual bool SetIsFriendOnly(bool is_friend_only) = 0;

    virtual bool GetIsFriendOnly() = 0;

    // clear all Key-Value pairs set by SetMetadata or AsyncSetMetadata
    virtual bool ClearAllMetadata() = 0;

    // gameserver meta data
    // get gameserver single Key-Value
    virtual RailResult GetMetadata(const RailString& key, RailString* value) = 0;

    // set gameserver single Key-Value
    virtual RailResult SetMetadata(const RailString& key, const RailString& value) = 0;

    // set gameserver multi Key-Value
    // trigger event SetGameServerMetadataResult
    virtual RailResult AsyncSetMetadata(const RailArray<RailKeyValue>& key_values,
                        const RailString& user_data) = 0;

    // get gameserver multi Key-Value
    // trigger event GetGameServerMetadataResult
    virtual RailResult AsyncGetMetadata(const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // get gameserver all Key-Value
    // trigger event GetGameServerMetadataResult
    virtual RailResult AsyncGetAllMetadata(const RailString& user_data) = 0;

    // session ticket
    // Retrieve ticket to be sent to the entity who wishes to authenticate you
    // trigger event AsyncAcquireGameServerSessionTicketResponse
    virtual RailResult AsyncAcquireGameServerSessionTicket(const RailString& user_data) = 0;

    // Authenticate session ticket to be sure it is valid and isn't reused
    // trigger event GameServerStartSessionWithPlayerResponse
    virtual RailResult AsyncStartSessionWithPlayer(const RailSessionTicket& player_ticket,
                        const RailID& player_rail_id,
                        const RailString& user_data) = 0;

    // called when no longer playing game with this entity
    virtual void TerminateSessionOfPlayer(const RailID& player_rail_id) = 0;

    //  Abandon session ticket from AsyncAcquireGameServerSessionTicket
    virtual void AbandonGameServerSessionTicket(const RailSessionTicket& session_ticket) = 0;

    // report player info
    virtual RailResult ReportPlayerJoinGameServer(
                        const RailArray<GameServerPlayerInfo>& player_infos) = 0;

    virtual RailResult ReportPlayerQuitGameServer(
                        const RailArray<GameServerPlayerInfo>& player_infos) = 0;

    virtual RailResult UpdateGameServerPlayerList(
                        const RailArray<GameServerPlayerInfo>& player_infos) = 0;

    virtual uint32_t GetCurrentPlayers() = 0;

    virtual void RemoveAllPlayers() = 0;

    // trigger event GameServerRegisterToServerListResult
    virtual RailResult RegisterToGameServerList() = 0;

    virtual RailResult UnregisterFromGameServerList() = 0;

    virtual RailResult CloseGameServer() = 0;

    virtual RailResult GetFriendsInGameServer(RailArray<RailID>* friend_ids) = 0;

    virtual bool IsUserInGameServer(const RailID& user_rail_id) = 0;

    virtual bool SetServerInfo(const RailString& server_info) = 0;

    virtual bool GetServerInfo(RailString* server_info) = 0;

    virtual RailResult EnableTeamVoice(bool enable) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GAME_SERVER_H
