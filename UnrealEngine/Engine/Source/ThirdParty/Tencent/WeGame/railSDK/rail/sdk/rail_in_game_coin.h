// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IN_GAME_COIN_H
#define RAIL_SDK_RAIL_IN_GAME_COIN_H

#include "rail/sdk/rail_in_game_coin_define.h"

// @desc The interfaces here are only for the platform's first-party games, which need to integrate
// the 'Midas' payment system. Most third-party developers need to check 'rail_in_game_purchase.h'
// to implement functionalities related to in-game purchases.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailInGameCoin {
  public:
    // @desc callback is RailInGameCoinRequestCoinInfoResponse
    virtual RailResult AsyncRequestCoinInfo(const RailString& user_data) = 0;

    // @desc callback is RailInGameCoinPurchaseCoinsResponse
    virtual RailResult AsyncPurchaseCoins(const RailCoins& purchase_info,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IN_GAME_COIN_H
