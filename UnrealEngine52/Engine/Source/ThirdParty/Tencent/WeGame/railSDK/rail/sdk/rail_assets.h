// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ASSETS_H
#define RAIL_SDK_RAIL_ASSETS_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_assets_define.h"
#include "rail/sdk/rail_in_game_purchase_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailAssets;

// rail assets helper singleton
class IRailAssetsHelper {
  public:
    virtual IRailAssets* OpenAssets() = 0;

    virtual IRailAssets* OpenGameServerAssets() = 0;
};

// assets object class
class IRailAssets : public IRailComponent {
  public:
    //  async get all Assets, trigger event RequestAllAssetsFinished
    virtual RailResult AsyncRequestAllAssets(const RailString& user_data) = 0;

    virtual RailResult QueryAssetInfo(RailAssetID asset_id, RailAssetInfo* asset_info) = 0;

    // async update assets property
    virtual RailResult AsyncUpdateAssetsProperty(
                        const RailArray<RailAssetProperty>& asset_property_list,
                        const RailString& user_data) = 0;

    //  consume assets in the unit of integer at once
    virtual RailResult AsyncDirectConsumeAssets(const RailArray<RailAssetItem>& assets,
                        const RailString& user_data) = 0;

    // consume assets in the unit of non-integer(eg: percent, time...)
    virtual RailResult AsyncStartConsumeAsset(RailAssetID asset_id,
                        const RailString& user_data) = 0;

    virtual RailResult AsyncUpdateConsumeProgress(RailAssetID asset_id,
                        const RailString& progress,
                        const RailString& user_data) = 0;

    virtual RailResult AsyncCompleteConsumeAsset(RailAssetID asset_id,
                        uint32_t quantity,
                        const RailString& user_data) = 0;

    // trigger event ExchangeAssetsFinished
    // exchange into new assets with owned assets,
    // it could be splitting one asset to server assets, or merge into one asset with server assets
    // when finish exchanging, the old asset disappear,
    // and the new asset is generated.the new asset has a new id asset-id
    virtual RailResult AsyncExchangeAssets(const RailArray<RailAssetItem>& old_assets,
                        const RailProductItem& to_product_info,
                        const RailString& user_data) = 0;

    // when finish exchanging, the old asset disappears
    // and the new asset is generated.but the new asset uses a asset-id that has already existed
    // warning: all the specified asset-id exchanging only succeed
    // when they are in the state of non-consume
    // trigger event ExchangeAssetsToFinished
    virtual RailResult AsyncExchangeAssetsTo(const RailArray<RailAssetItem>& old_assets,
                        const RailProductItem& to_product_info,
                        const RailAssetID& add_to_exist_assets,
                        const RailString& user_data) = 0;

    // splitting assets
    // warning: all the split assets should have the same product id
    // split some staffs in one asset-id into a new asset, and it has a new asset-id
    // the asset-id will generated automatically after splitting
    // to_quantity：the number of to_quantity to the new asset
    virtual RailResult AsyncSplitAsset(RailAssetID source_asset,
                        uint32_t to_quantity,
                        const RailString& user_data) = 0;

    // add the staffs with number of to_quantity to a asset-id that has already existed
    virtual RailResult AsyncSplitAssetTo(RailAssetID source_asset,
                        uint32_t to_quantity,
                        RailAssetID add_to_asset,
                        const RailString& user_data) = 0;

    // assets merging
    // the quantity in the RailAssetItem is the number
    // that will be merged into a new asset or merged to a existed asset
    // it will create a new asset
    virtual RailResult AsyncMergeAsset(const RailArray<RailAssetItem>& source_assets,
                        const RailString& user_data) = 0;

    virtual RailResult AsyncMergeAssetTo(const RailArray<RailAssetItem>& source_assets,
                        RailAssetID add_to_asset,
                        const RailString& user_data) = 0;

    // serialize all assets to buffer, the result buffer is encrypted
    // the serialized result buffer can be transmitted to gameservers or other players,
    // and they can call DeserializeAssetsFromBuffer to deserialize this buffer
    // NOTE: you can only serialize yourself assets
    virtual RailResult SerializeAssetsToBuffer(RailString* buffer) = 0;

    // serialize specified assets to buffer, the result buffer is encrypted
    virtual RailResult SerializeAssetsToBuffer(const RailArray<RailAssetID>& assets,
                        RailString* buffer) = 0;

    // deserialize assets from buffer
    virtual RailResult DeserializeAssetsFromBuffer(RailID assets_owner,
                        const RailString& buffer,
                        RailArray<RailAssetInfo>* assets_info) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_ASSETS_H
