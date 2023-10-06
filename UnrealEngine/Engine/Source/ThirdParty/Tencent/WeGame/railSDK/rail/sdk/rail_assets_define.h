// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ASSETS_DEFINE_H
#define RAIL_SDK_RAIL_ASSETS_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailAssetFlags {
    kRailAssetFlagsNotSale = 1,  // The asset cannot be sold.
};

enum EnumRailAssetState {
    kRailAssetStateNormal = 1,     // The normal asset.
    kRailAssetStateInConsume = 2,  // The asset has been in consume.
};

enum EnumRailAssetOrigin {
    kRailAssetOriginGenerate = 1,  // game server auto generate
    kRailAssetOriginPurchase = 2,  // in game purchase
    kRailAssetOriginFriends = 3,
    kRailAssetOriginTrade = 4,
    kRailAssetOriginMarket = 5,  // market of rail platform
    kRailAssetOriginPromo = 6,
    kRailAssetOriginExchange = 7,
    kRailAssetOriginDrop = 8,
};

struct RailAssetItem {
    RailAssetItem() {
        asset_id = 0;
        quantity = 0;
    };

    RailAssetID asset_id;
    uint32_t quantity;
};

struct RailGeneratedAssetItem {
    RailGeneratedAssetItem() {
        product_id = 0;
        container_id = 0;
    }
    RailProductID product_id;
    RailAssetItem asset;
    uint64_t container_id;
};

struct RailAssetInfo {
    RailAssetInfo() {
        asset_id = 0;
        product_id = 0;
        position = -1;
        quantity = 0;
        state = kRailAssetStateNormal;
        flag = kRailAssetFlagsNotSale;
        origin = kRailAssetOriginGenerate;
        expire_time = 0;
        container_id = 0;
    }
    RailAssetID asset_id;
    RailProductID product_id;
    RailString product_name;  // defined here to prevent from querying too much
    int32_t position;
    RailString progress;
    uint32_t quantity;
    uint32_t state;   // refer to EnumRailAssetState.
    uint32_t flag;    // refer to EnumRailAssetFlags.
    uint32_t origin;  // refer to EnumRailAssetOrigin.
    uint32_t expire_time;  // UTC time. If expire_time is zero, this is a permanent asset.
    uint64_t container_id;
};

struct RailProductItem {
    RailProductItem() {
        product_id = 0;
        quantity = 0;
    };

    RailProductID product_id;
    uint32_t quantity;
};

struct RailAssetProperty {
    RailAssetProperty() {
        position = 0;
        asset_id = 0;
    }

    RailAssetID asset_id;
    uint32_t position;
};

namespace rail_event {

struct RequestAllAssetsFinished : public RailEvent<kRailEventAssetsRequestAllAssetsFinished> {
    RequestAllAssetsFinished() {}

    RailArray<RailAssetInfo> assetinfo_list;
};

struct UpdateAssetsPropertyFinished
    : public RailEvent<kRailEventAssetsUpdateAssetPropertyFinished> {
    UpdateAssetsPropertyFinished() {}

    RailArray<RailAssetProperty> asset_property_list;
};

struct DirectConsumeAssetsFinished : public RailEvent<kRailEventAssetsDirectConsumeFinished> {
    DirectConsumeAssetsFinished() {}

    RailArray<RailAssetItem> assets;
};

struct StartConsumeAssetsFinished : public RailEvent<kRailEventAssetsStartConsumeFinished> {
    StartConsumeAssetsFinished() { asset_id = 0; }

    RailAssetID asset_id;
};

struct UpdateConsumeAssetsFinished : public RailEvent<kRailEventAssetsUpdateConsumeFinished> {
    UpdateConsumeAssetsFinished() { asset_id = 0; }

    RailAssetID asset_id;
};

struct CompleteConsumeAssetsFinished : public RailEvent<kRailEventAssetsCompleteConsumeFinished> {
    CompleteConsumeAssetsFinished() {}

    RailAssetItem asset_item;
};

struct SplitAssetsFinished : public RailEvent<kRailEventAssetsSplitFinished> {
    SplitAssetsFinished() {
        source_asset = 0;
        to_quantity = 0;
        new_asset_id = 0;
    }

    RailAssetID source_asset;
    uint32_t to_quantity;
    RailAssetID new_asset_id;
};

struct SplitAssetsToFinished : public RailEvent<kRailEventAssetsSplitToFinished> {
    SplitAssetsToFinished() {
        source_asset = 0;
        to_quantity = 0;
        split_to_asset_id = 0;
    }

    RailAssetID source_asset;
    uint32_t to_quantity;
    RailAssetID split_to_asset_id;
};

struct MergeAssetsFinished : public RailEvent<kRailEventAssetsMergeFinished> {
    MergeAssetsFinished() { new_asset_id = 0; }

    RailArray<RailAssetItem> source_assets;
    RailAssetID new_asset_id;
};

struct MergeAssetsToFinished : public RailEvent<kRailEventAssetsMergeToFinished> {
    MergeAssetsToFinished() { merge_to_asset_id = 0; }

    RailArray<RailAssetItem> source_assets;
    RailAssetID merge_to_asset_id;
};

struct CompleteConsumeByExchangeAssetsToFinished
    : public RailEvent<kRailEventAssetsCompleteConsumeByExchangeAssetsToFinished> {
    CompleteConsumeByExchangeAssetsToFinished() {}
};

struct ExchangeAssetsFinished : public RailEvent<kRailEventAssetsExchangeAssetsFinished> {
    ExchangeAssetsFinished() {}

    RailArray<RailAssetItem> old_assets;
    RailArray<RailGeneratedAssetItem> new_asset_item_list;
};

struct ExchangeAssetsToFinished : public RailEvent<kRailEventAssetsExchangeAssetsToFinished> {
    ExchangeAssetsToFinished() { exchange_to_asset_id = 0; }

    RailArray<RailAssetItem> old_assets;
    RailProductItem to_product_info;
    RailAssetID exchange_to_asset_id;
};

struct RailAssetsChanged : public RailEvent<kRailEventAssetsAssetsChanged> {
    RailAssetsChanged() {
    }
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_ASSETS_DEFINE_H
