// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FLOATING_WINDOW_DEFINE_H
#define RAIL_SDK_RAIL_FLOATING_WINDOW_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum RailWindowClass {
    kRailWindowClassUnknown = 0,
    kRailWindowClassNormal = 1,
    kRailWindowClassNotify = 2,
};

// floating window type define
enum EnumRailWindowType {
    kRailWindowUnknown = 0,
    kRailWindowFriendList = 10,
    kRailWindowPurchaseProduct = 14,
    kRailWindowAchievement = 15,
    kRailWindowUserSpaceStore = 16,
    kRailWindowLeaderboard = 17,
    kRailWindowHomepage = 18,
    kRailWindowLiveCommenting = 19,
};

enum EnumRailNotifyWindowType {
    kRailNotifyWindowUnknown = 0,
    kRailNotifyWindowOverlayPanel = 1,
    kRailNotifyWindowUnlockAchievement = 2,
    kRailNotifyWindowFriendInvite = 3,
    kRailNotifyWindowAddFriend = 4,
    kRailNotifyWindowAntiAddiction = 5,
};

enum EnumRailNotifyWindowPosition {
    kRailNotifyWindowPositionTopLeft = 1,
    kRailNotifyWindowPositionTopRight = 2,
    kRailNotifyWindowPositionBottomLeft = 3,
    kRailNotifyWindowPositionBottomRight = 4,
};

struct RailWindowLayout {
    RailWindowLayout() {
        position_type = kRailNotifyWindowPositionTopLeft;
        x_margin = 0;
        y_margin = 0;
    }
    //  if position_type is kRailNotifyWindowPositionTopLeft
    //  x_margin andy_margin are the margins to the left-top of the window
    //  if position_type is kRailNotifyWindowPositionTopRight
    //  x_margin and y_margin are the margins to the right-top of the window
    //  if position_type is kRailNotifyWindowPositionBottomLeft
    //  x_margin and y_margin are the margins to the left-bottom of the window
    //  if position_type is kRailNotifyWindowPositionBottomRight
    //  x_margin and y_margin are the margins to the right-bottom of the window
    uint32_t x_margin;
    uint32_t y_margin;
    EnumRailNotifyWindowPosition position_type;
};

enum EnumRailStoreType {
    kRailStoreTypeDlc = 1,
    kRailStoreTypeGame = 2,
    kRailStoreTypeDlcs = 3,
};

// show store options
struct RailStoreOptions {
    RailStoreOptions() {
        store_type = kRailStoreTypeDlc;
        window_margin_top = 100;   // in pixel
        window_margin_left = 100;  // in pixel
    }

    EnumRailStoreType store_type;
    int32_t window_margin_top;
    int32_t window_margin_left;
};

namespace rail_event {

struct ShowFloatingWindowResult : public RailEvent<kRailEventShowFloatingWindow> {
    ShowFloatingWindowResult() {
        result = kFailure;
        is_show = false;
        window_type = kRailWindowUnknown;
    }

    bool is_show;
    EnumRailWindowType window_type;
};

struct ShowNotifyWindow : public RailEvent<kRailEventShowFloatingNotifyWindow> {
    ShowNotifyWindow() {
        result = kFailure;
        window_type = kRailNotifyWindowUnknown;
    }

    EnumRailNotifyWindowType window_type;
    RailString json_content;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_FLOATING_WINDOW_DEFINE_H
