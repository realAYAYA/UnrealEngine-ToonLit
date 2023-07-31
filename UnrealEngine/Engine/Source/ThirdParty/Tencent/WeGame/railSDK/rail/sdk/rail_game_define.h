// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GAME_DEFINE_H
#define RAIL_SDK_RAIL_GAME_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

const uint32_t kRailMaxGameDefinePlayingStateValue = 2146483648;

enum EnumRailGameContentDamageFlag {
    kRailGameContentMissing = 1,
    kRailGameContentWrong = 2,
};

enum EnumRailGamePlayingState {
    kRailGamePlayingStateUnknow = 0,
    kRailGamePlayingStateLaunched = 1,  // just start game client, not actually playing the game
    kRailGamePlayingStatePlaying = 2,   // playing the game
    kRailGamePlayingStateStopped = 3,   // game process stopped

    kRailGamePlayingStateGameDefinePlayingState = 100,  // you could define your own game state
};

struct RailBranchInfo {
    RailBranchInfo() {}

    RailString branch_name;
    RailString branch_type;
    RailString branch_id;
    RailString build_number;
};

struct RailGameDefineGamePlayingState {
    RailGameDefineGamePlayingState() {
        game_define_game_playing_state = kRailGamePlayingStateGameDefinePlayingState;
    }

    uint32_t game_define_game_playing_state;  // custom game playing state
    RailString state_name_zh_cn;              // the Chinese description of game playing state
    RailString state_name_en_us;              // the English description of game playing state
};

namespace rail_event {

struct QuerySubscribeWishPlayStateResult
    : public RailEvent<kRailEventAppQuerySubscribeWishPlayStateResult> {
    QuerySubscribeWishPlayStateResult() { is_subscribed = false; }

    bool is_subscribed;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GAME_DEFINE_H
