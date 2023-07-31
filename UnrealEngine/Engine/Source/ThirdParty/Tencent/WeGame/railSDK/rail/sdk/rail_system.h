// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_SYSTEM_H
#define RAIL_SDK_RAIL_SYSTEM_H

#include "rail/sdk/rail_system_state_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailSystemHelper {
  public:
    // when player's ownership is expired, you will receive a RailSystemState callback event.
    // the game process will be terminated or not according to timeout_seconds.
    //     timeout_seconds = 0: terminate game immediately
    //     timeout_seconds > 0: terminate game if timeout expired
    //     timeout_seconds < 0: never terminate game
    virtual RailResult SetTerminationTimeoutOwnershipExpired(int32_t timeout_seconds) = 0;

    // the platform state will be on-line or offline. If the state is offline, you can not
    // access network. You should call this interface after RailInitialize had been called
    // to determine use network or not. In offline mode, all asynchronous interface will
    // return kErrorClientInOfflineMode.
    virtual RailSystemState GetPlatformSystemState() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_SYSTEM_H
