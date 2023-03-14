// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IN_GAME_PURCHASE_DEFINE_H
#define RAIL_SDK_RAIL_IN_GAME_PURCHASE_DEFINE_H

#include "rail/sdk/rail_assets_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

// define product id, [1, 1000000000] is used for game self
// like in-game-purchase, assert and so on
enum EnumRailProductId {
    EnumRailProductId_For_Game_Start = 1,
    EnumRailProductId_For_Game_End = 1000000000,

    EnumRailProductId_For_Platfrom_Start = 1000000001,
    EnumRailProductId_For_Platfrom_Storage_Space = 1000000001,
    EnumRailProductId_For_Platfrom_All = 1000000011
};

// in game purchase products discount type
enum PurchaseProductDiscountType {
    kPurchaseProductDiscountTypeInvalid = 0,
    kPurchaseProductDiscountTypeNone = 1,
    kPurchaseProductDiscountTypePermanent = 2,
    kPurchaseProductDiscountTypeTimed = 3,
};

// in game purchase order state
enum PurchaseProductOrderState {
    kPurchaseProductOrderStateInvalid = 0,
    kPurchaseProductOrderStateCreateOrderOk = 100,
    kPurchaseProductOrderStatePayOk = 200,
    kPurchaseProductOrderStateDeliverOk = 300,
};

struct RailDiscountInfo {
    RailDiscountInfo() {
        off = 0;
        type = kPurchaseProductDiscountTypeNone;
        discount_price = 0.0;
        start_time = 0;
        end_time = 0;
    }

    float off;
    float discount_price;  // this value will be automatically calculated
                           // by backend server according off parameter.
    PurchaseProductDiscountType type;
    uint32_t start_time;
    uint32_t end_time;
};

// product info
struct RailPurchaseProductExtraInfo {
    RailPurchaseProductExtraInfo() {}

    RailString exchange_rule;
    RailString bundle_rule;
};

struct RailPurchaseProductInfo {
    RailPurchaseProductInfo() {
        product_id = 0;
        is_purchasable = false;
        original_price = 0.0;
    }

    RailProductID product_id;
    bool is_purchasable;
    RailString name;
    RailString description;
    RailString category;
    RailString product_thumbnail;
    RailPurchaseProductExtraInfo extra_info;
    // when is_purchasable is true, the following parameters will be available
    float original_price;
    RailString currency_type;
    RailDiscountInfo discount;
};

namespace rail_event {

struct RailInGamePurchaseRequestAllPurchasableProductsResponse
    : RailEvent<kRailEventInGamePurchaseAllPurchasableProductsInfoReceived> {
    RailInGamePurchaseRequestAllPurchasableProductsResponse() {
        result = kFailure;
    }

    RailArray<RailPurchaseProductInfo> purchasable_products;
};

struct RailInGamePurchaseRequestAllProductsResponse
    : RailEvent<kRailEventInGamePurchaseAllProductsInfoReceived> {
    RailInGamePurchaseRequestAllProductsResponse() {
        result = kFailure;
    }

    RailArray<RailPurchaseProductInfo> all_products;
};

// Deprecated
struct RailInGamePurchasePurchaseProductsResponse
    : RailEvent<kRailEventInGamePurchasePurchaseProductsResult> {
    RailInGamePurchasePurchaseProductsResponse() {
        result = kFailure;
        user_data = "";
    }

    RailString order_id;
    RailArray<RailProductItem> delivered_products;
};

struct RailInGamePurchasePurchaseProductsToAssetsResponse
    : RailEvent<kRailEventInGamePurchasePurchaseProductsToAssetsResult> {
    RailInGamePurchasePurchaseProductsToAssetsResponse() {
        result = kFailure;
    }

    RailString order_id;
    RailArray<RailAssetInfo> delivered_assets;
};

// Deprecated
struct RailInGamePurchaseFinishOrderResponse :
    RailEvent<kRailEventInGamePurchaseFinishOrderResult> {
    RailInGamePurchaseFinishOrderResponse() {
        result = kFailure;
    }

    RailString order_id;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IN_GAME_PURCHASE_DEFINE_H
