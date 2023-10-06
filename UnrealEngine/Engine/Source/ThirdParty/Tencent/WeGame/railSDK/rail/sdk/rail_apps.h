// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_APPS_H
#define RAIL_SDK_RAIL_APPS_H

#include "rail/sdk/rail_game_define.h"

// @desc Interfaces to check game installation status and player's following status in community

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailApps {
  public:
    // @desc Check if the specified game is installed. If you have another game released on the
    // game platform, it might be useful to check that game's installation status. To check the
    // current game's DLC installation status, please see IsDlcInstalled in 'rail_dlc.h'
    // @param game_id ID of the game.
    // @return True if the game is installed.
    virtual bool IsGameInstalled(const RailGameID& game_id) = 0;

    // @desc Check if the current player has followed the game account in the community
    // The callback is QuerySubscribeWishPlayState.
    // @param game_id ID of the game
    // @param user_data Will be copied to the asynchronous result
    // @return kSuccess on success
    virtual RailResult AsyncQuerySubscribeWishPlayState(const RailGameID& game_id,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_APPS_H
