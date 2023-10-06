// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_DEFINE_H
#define RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

namespace rail_event {

struct RailInGameStorePurchasePayWindowDisplayed :
    public RailEvent<kRailEventInGameStorePurchasePayWindowDisplayed> {
    RailInGameStorePurchasePayWindowDisplayed() {
        result = kFailure;
    }

    RailString order_id;
};

struct RailInGameStorePurchasePayWindowClosed :
    public RailEvent<kRailEventInGameStorePurchasePayWindowClosed> {
    RailInGameStorePurchasePayWindowClosed() {
        result = kFailure;
    }

    RailString order_id;
};

struct RailInGameStorePurchaseResult :
    public RailEvent<kRailEventInGameStorePurchasePaymentResult> {
    RailInGameStorePurchaseResult() {
        result = kErrorInGameStorePurchasePaymentFailure;
    }

    RailString order_id;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IN_GAME_STORE_PURCHASE_DEFINE_H
