// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_SYSTEM_STATE_DEFINE_H
#define RAIL_SDK_RAIL_SYSTEM_STATE_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

// list of states the player or platform system can be in
enum RailSystemState {
    kSystemStateUnknown = 0,
    kSystemStatePlatformOnline = 1,
    kSystemStatePlatformOffline = 2,
    kSystemStatePlatformExit = 3,

    kSystemStatePlayerOwnershipExpired = 20,
    kSystemStatePlayerOwnershipActivated = 21,
    kSystemStatePlayerOwnershipBanned = 22,

    kSystemStateGameExitByAntiAddiction = 40,
};

namespace rail_event {

// posted when platform system state changed
struct RailSystemStateChanged : public RailEvent<kRailEventSystemStateChanged> {
    RailSystemStateChanged() { state = kSystemStateUnknown; }

    RailSystemState state;
};

// join gameserver notify
struct RailPlatformNotifyEventJoinGameByGameServer
    : public RailEvent<kRailPlatformNotifyEventJoinGameByGameServer> {
    RailPlatformNotifyEventJoinGameByGameServer() {}
    RailID gameserver_railid;
    RailString commandline_info;
};

// join room notify
struct RailPlatformNotifyEventJoinGameByRoom
    : public RailEvent<kRailPlatformNotifyEventJoinGameByRoom> {
    RailPlatformNotifyEventJoinGameByRoom() {
        room_id = 0;
    }

    uint64_t room_id;
    RailString commandline_info;
};

// posted when you want to join other user's game
struct RailPlatformNotifyEventJoinGameByUser
    : public RailEvent<kRailPlatformNotifyEventJoinGameByUser> {
    RailPlatformNotifyEventJoinGameByUser() { result = kFailure; }
    RailID rail_id_to_join;
    RailString commandline_info;
};

// posted when rail sdk finalizes
struct RailFinalize : public RailEvent<kRailEventFinalize> {
    RailFinalize() { result = kFailure; }
};
}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_SYSTEM_STATE_DEFINE_H
