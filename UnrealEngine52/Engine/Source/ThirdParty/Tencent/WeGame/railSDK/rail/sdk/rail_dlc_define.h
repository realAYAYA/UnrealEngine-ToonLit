// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_DLC_DEFINE_H
#define RAIL_SDK_RAIL_DLC_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailDlcInfo {
    RailDlcID dlc_id;     // ID of the DLC
    RailGameID game_id;   // ID of the game
    RailString version;   // version number of the DLC
    RailString name;      // DLC's name
    RailString description;  // Description of the DLC
    double original_price;   // The original price of the DLC
    double discount_price;   // The discount price of the DLC
    RailDlcInfo() {
        dlc_id = kInvalidDlcId;
        game_id = kInvalidGameId;
        discount_price = 0.0;
        original_price = 0.0;
    }
};

struct RailDlcInstallProgress {
    uint32_t progress;
    uint64_t finished_bytes;
    uint64_t total_bytes;
    uint32_t speed;
    RailDlcInstallProgress() {
        progress = 0;
        finished_bytes = 0;
        total_bytes = 0;
        speed = 0;
    }
};

struct RailDlcOwned {
    bool is_owned;
    RailDlcID dlc_id;
    RailDlcOwned() { is_owned = false; }
};

namespace rail_event {

struct DlcInstallStart : public RailEvent<kRailEventDlcInstallStart> {
    RailDlcID dlc_id;
    DlcInstallStart() { dlc_id = 0; }
};

struct DlcInstallStartResult : public RailEvent<kRailEventDlcInstallStartResult> {
    RailDlcID dlc_id;
    RailResult result;
    DlcInstallStartResult() {
        dlc_id = 0;
        result = kSuccess;
    }
};

struct DlcInstallProgress : public RailEvent<kRailEventDlcInstallProgress> {
    RailDlcID dlc_id;
    RailDlcInstallProgress progress;
    DlcInstallProgress() { dlc_id = 0; }
};

struct DlcInstallFinished : public RailEvent<kRailEventDlcInstallFinished> {
    RailDlcID dlc_id;
    RailResult result;
    DlcInstallFinished() {
        dlc_id = 0;
        result = kErrorDlcInstallFailed;
    }
};

struct DlcUninstallFinished : public RailEvent<kRailEventDlcUninstallFinished> {
    RailDlcID dlc_id;
    RailResult result;
    DlcUninstallFinished() {
        dlc_id = 0;
        result = kErrorDlcUninstallFailed;
    }
};

struct CheckAllDlcsStateReadyResult : public RailEvent<kRailEventDlcCheckAllDlcsStateReadyResult> {
    CheckAllDlcsStateReadyResult() {}
};

struct QueryIsOwnedDlcsResult : public RailEvent<kRailEventDlcQueryIsOwnedDlcsResult> {
    RailArray<RailDlcOwned> dlc_owned_list;
    QueryIsOwnedDlcsResult() {}
};

struct DlcOwnershipChanged : public RailEvent<kRailEventDlcOwnershipChanged> {
    DlcOwnershipChanged() { is_active = true; }
    RailDlcID dlc_id;
    bool is_active;
};

struct DlcRefundChanged : public RailEvent<kRailEventDlcRefundChanged> {
    DlcRefundChanged() { refund_state = kRailGameRefundStateUnknown; }
    RailDlcID dlc_id;
    EnumRailGameRefundState refund_state;
};
}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_DLC_DEFINE_H
