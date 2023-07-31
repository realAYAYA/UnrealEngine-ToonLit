// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GAME_H
#define RAIL_SDK_RAIL_GAME_H

#include "rail/sdk/rail_game_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailGame {
  public:
    // get current game id
    virtual RailGameID GetGameID() = 0;

    // flag was combined with GameContentDamageFlag
    virtual RailResult ReportGameContentDamaged(EnumRailGameContentDamageFlag flag) = 0;

    // get game install path
    virtual RailResult GetGameInstallPath(RailString* app_path) = 0;

    // query game subscribe state, callback is QuerySubscribeWishPlayState
    virtual RailResult AsyncQuerySubscribeWishPlayState(const RailString& user_data) = 0;

    //
    // game multi-languages
    //
    // get language code that player selected for current game
    virtual RailResult GetPlayerSelectedLanguageCode(RailString* language_code) = 0;

    // get available language codes that game support
    virtual RailResult GetGameSupportedLanguageCodes(RailArray<RailString>* language_codes) = 0;

    //
    // game state
    //
    virtual RailResult SetGameState(EnumRailGamePlayingState game_state) = 0;

    virtual RailResult GetGameState(EnumRailGamePlayingState* game_state) = 0;

    // register all game playing state that you will define in game. We will show this information
    // on friend overlay.
    virtual RailResult RegisterGameDefineGamePlayingState(
                        const RailArray<RailGameDefineGamePlayingState>& game_playing_states) = 0;

    // set current player's game state. We will send a broadcast to notify every on-line friend of
    // current player. The maximum value of game_playing_state parameter can not exceed
    // kRailMaxGameDefinePlayingStateValue.
    virtual RailResult SetGameDefineGamePlayingState(uint32_t game_playing_state) = 0;

    // get current player's game state.
    virtual RailResult GetGameDefineGamePlayingState(uint32_t* game_playing_state) = 0;

    //
    // game branch and version info
    //
    // get branch build number
    virtual RailResult GetBranchBuildNumber(RailString* build_number) = 0;

    // get current branch information
    virtual RailResult GetCurrentBranchInfo(RailBranchInfo* branch_info) = 0;

    //
    // game time counting for player
    //
    // for time counting in the game with counting_key to be a identification of the counting
    virtual RailResult StartGameTimeCounting(const RailString& counting_key) = 0;

    // to pair with StartGameTimeCounting
    virtual RailResult EndGameTimeCounting(const RailString& counting_key) = 0;

    //
    // game owner and purchase time
    //
    // return the RailID that purchase game
    virtual RailID GetGamePurchasePlayerRailID() = 0;

    // return the earliest game purchase time, number of seconds since Jan 1, 1970
    virtual uint32_t GetGameEarliestPurchaseTime() = 0;

    // return number of seconds since the game activated
    virtual uint32_t GetTimeCountSinceGameActivated() = 0;

    // return number of seconds since the mouse last moved
    virtual uint32_t GetTimeCountSinceLastMouseMoved() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GAME_H
