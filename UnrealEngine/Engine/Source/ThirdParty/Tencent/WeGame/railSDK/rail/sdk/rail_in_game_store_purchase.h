// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_H
#define RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_H

#include "rail/sdk/rail_in_game_store_purchase_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailInGameStorePurchaseHelper {
  public:
    // callback is RailInGameStorePurchasePayWindowDisplayed.
    virtual RailResult AsyncShowPaymentWindow(const RailString& order_id,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_H
