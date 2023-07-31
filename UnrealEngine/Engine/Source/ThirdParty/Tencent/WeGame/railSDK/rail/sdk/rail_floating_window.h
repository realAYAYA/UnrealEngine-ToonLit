// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FLOATING_WINDOW_H
#define RAIL_SDK_RAIL_FLOATING_WINDOW_H

#include "rail/sdk/rail_floating_window_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailFloatingWindow {
  public:
    // @desc Display the WeGame platform overlay over the game. A typical usage as below:
    // IRailFloatingWindow* helper = RailFactory()::RailFloatingWindow();
    // helper->AsyncShowRailFloatingWindow(kRailWindowAchievement, "");
    // @param window_type Overlay type. For example, kRailWindowAchievement/kRailWindowLeaderboard
    // For more please check the definition of EnumRailWindowType
    // @param user_data Will be copied to the asynchronous result
    // @return kSuccess on success
    virtual RailResult AsyncShowRailFloatingWindow(EnumRailWindowType window_type,
                        const RailString& user_data) = 0;

    // @desc Close a specified overlay window. Usually players can directly close an overlay.
    // @param window_type Overlay type
    // @param user_data Will be copied to the asynchronous result
    // @return kSuccess on success
    virtual RailResult AsyncCloseRailFloatingWindow(EnumRailWindowType window_type,
                        const RailString& user_data) = 0;

    // @desc Set the position for a specified overlay window
    // @param window_type Overlay type
    // @param layout The position of the overlay
    virtual RailResult SetNotifyWindowPosition(EnumRailNotifyWindowType window_type,
                        const RailWindowLayout& layout) = 0;

    // @desc Show store page overlay for the game or the game's DLC
    // @param id Can be a game's ID or a DLC's ID. Players can directly purchase a DLC
    // on the DLC store page overlay.
    // @param options Options for the game/DLC/DLCs
    // @param user_data Will be copied to the asynchronous result
    // @return kSuccess on success
    virtual RailResult AsyncShowStoreWindow(const uint64_t& id,
                        const RailStoreOptions& options,
                        const RailString& user_data) = 0;

    // @desc Check whether the overlay feature is available for the developer. If available,
    // developers can use the APIs to show the overlays for achivement, leaderboard etc.
    // @return true if the the feature is available
    virtual bool IsFloatingWindowAvailable() = 0;

    // @desc Show the game's store page overlay over the game.
    // This is similar to the interface AsyncShowRailFloatingWindow.
    // @param user_data Will be copied to the asynchronous result
    // @return kSuccess on success
    virtual RailResult AsyncShowDefaultGameStoreWindow(const RailString& user_data) = 0;

    // @desc To enable or disable the notifications.
    // @window_type Overlay type
    // @param enable If you would like to use your own notifications rather than the WeGame
    // platform's, please set it to false
    // @return kSuccess on success
    virtual RailResult SetNotifyWindowEnable(EnumRailNotifyWindowType window_type, bool enable) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_FLOATING_WINDOW_H
