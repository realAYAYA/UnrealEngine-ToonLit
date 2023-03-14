// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IN_GAME_PURCHASE_H
#define RAIL_SDK_RAIL_IN_GAME_PURCHASE_H

#include "rail/sdk/rail_in_game_purchase_define.h"

// @desc Classes related to in-game purchases.
// In-game add-ons can be either store-managed or developer-managed.
// Interfaces in rail_assets.h and rail_in_game_purchase.h are for store-managed in-game items.
// If you have your own server to track developer-managed in-game items, you will need to use Web
// APIs instead. Please check wiki for details.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailInGamePurchase {
  public:
    // @desc Get all the products that can be purchased
    // A player may not need to purchase to get a in-game product. For example, some in-game
    // items might be bought with coins earned in the game rather than with real world money.
    // Will trigger event RequestAllPurchasableProductsResponse
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncRequestAllPurchasableProducts(const RailString& user_data) = 0;

    // @desc Get all the in-game products, whether or not they can be purchased with real
    // world money.
    // Will trigger event RequestAllProductsResponse
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncRequestAllProducts(const RailString& user_data) = 0;

    // @desc Get info for the product of 'product_id'
    // @param product_id ID of the product
    // @param product Pointer to the retrieved product info
    // @return Returns kSuccess on success
    virtual RailResult GetProductInfo(RailProductID product_id,
                        RailPurchaseProductInfo* product) = 0;

    // Deprecated
    virtual RailResult AsyncPurchaseProducts(const RailArray<RailProductItem>& cart_items,
                        const RailString& user_data) = 0;

    // Deprecated
    virtual RailResult AsyncFinishOrder(const RailString& order_id,
                        const RailString& user_data) = 0;

    // @desc This interface is recommended for store-managed items instead of AsyncPurchaseProducts.
    // With this interface, AsyncFinishOrder no longer needs to be called.
    // @param cart_items The items to purchase
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncPurchaseProductsToAssets(const RailArray<RailProductItem>& cart_items,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IN_GAME_PURCHASE_H
