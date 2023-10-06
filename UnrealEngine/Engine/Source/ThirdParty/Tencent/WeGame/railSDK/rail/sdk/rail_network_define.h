// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_NETWORK_DEFINE_H
#define RAIL_SDK_RAIL_NETWORK_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailNetworkSessionState {
    RailNetworkSessionState() {
        is_connection_active = false;
        is_connecting = false;
        is_using_relay = false;
        bytes_in_send_buffer = 0;
        packets_in_send_buffer = 0;
        remote_ip = 0;
        remote_port = 0;
        session_error = kSuccess;
    }
    bool is_connection_active;
    bool is_connecting;
    bool is_using_relay;
    RailResult session_error;
    uint32_t bytes_in_send_buffer;
    uint32_t packets_in_send_buffer;
    uint32_t remote_ip;
    uint16_t remote_port;
};

struct RailGamePeer {
    RailGamePeer() {
        peer = 0;
        game_id = 0;
    }

    RailID peer;
    RailGameID game_id;
};

namespace rail_event {

// This callback event is returned when a network session is requested to connect from the
// other player in a same game, which specifies the other player's rail_id and the local rail_id.
struct CreateSessionRequest : public RailEvent<kRailEventNetworkCreateSessionRequest> {
    CreateSessionRequest() {
        local_peer = 0;
        remote_peer = 0;
    }

    RailID local_peer;
    RailID remote_peer;
};

// This callback event is returned if the request to establish a network session with
// the other player in a same game fails.
struct CreateSessionFailed : public RailEvent<kRailEventNetworkCreateSessionFailed> {
    CreateSessionFailed() {
        result = kFailure;
        local_peer = 0;
        remote_peer = 0;
    }

    RailID local_peer;
    RailID remote_peer;
};

// This callback event is returned when a network session is requested to connect from a player
// in different games, which specifies the other player's rail_id, game_id and the local rail_id.
struct NetworkCreateRawSessionRequest
    : public RailEvent<kRailEventNetworkCreateRawSessionRequest> {
    NetworkCreateRawSessionRequest() {
        local_peer = 0;
        remote_game_peer.peer = 0;
        remote_game_peer.game_id = 0;
    }

    RailID local_peer;
    RailGamePeer remote_game_peer;
};

// This callback event is returned if the request to establish a network session with
// the other player in different games fails.
struct NetworkCreateRawSessionFailed
    : public RailEvent<kRailEventNetworkCreateRawSessionFailed> {
    NetworkCreateRawSessionFailed() {
        result = kFailure;
        local_peer = 0;
        remote_game_peer.peer = 0;
        remote_game_peer.game_id = 0;
    }

    RailID local_peer;
    RailGamePeer remote_game_peer;
};
}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_NETWORK_DEFINE_H
