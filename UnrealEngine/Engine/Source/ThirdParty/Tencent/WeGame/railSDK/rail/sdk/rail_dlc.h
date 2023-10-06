// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_DLC_HELPER_H
#define RAIL_SDK_RAIL_DLC_HELPER_H

#include "rail/sdk/rail_dlc_define.h"

// @desc DLC refers to 'Downloadable Content'. IRailDlcHelper contains APIs related to DLC
// management, such as DLC download, installation, information retrieving etc.
// The default download path is [user_data]/[game_id]/dlc/downloading/[dlc_id]/[dlc_id].zip
// The default installation path is [user_data]/[game_id]/dlc/installed/[dlc_id]/dlc
// You may also configure customized paths with a configuration file. Executables for
// installation and uninstallation can also be configured. Please check wiki for details.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailDlcHelper {
  public:
    // @desc Check on Rail's server and see whether the current player owns the specified DLCs.
    // This will trigger the event kRailEventDlcQueryIsOwnedDlcsResult
    // @param dlc_ids IDs of the DLCs to check. If no ID is specified, ownership info for each of
    // all the DLCs will be returned.
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryIsOwnedDlcsOnServer(const RailArray<RailDlcID>& dlc_ids,
                        const RailString& user_data) = 0;

    // @desc Retrieve the DLC info asynchronously from Rail's server. Please note this is the
    // prerequisite for IsDlcInstalled, IsOwnedDlc, GetDlcCount and GetDlcInfo. You
    // will need to wait till the asynchronous result of AsyncCheckAllDlcsStateReady
    // successfully returns.
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncCheckAllDlcsStateReady(const RailString& user_data) = 0;

    // @desc Check whether the DLC of 'dlc_id' is installed and retrieve the installation path
    // It is recommended to call AsyncCheckAllDlcsStateReady before using IsDlcInstalled
    // @param dlc_id The ID of the DLC to check
    // @param installed_path The local installation path of the DLC
    // @return Returns kSuccess on success
    virtual bool IsDlcInstalled(RailDlcID dlc_id, RailString* installed_path = NULL) = 0;

    // @desc Check whether the current player owns the DLC.
    // @param dlc_id The ID of the DLC to check
    // @return True if the current player owns the specified DLC
    virtual bool IsOwnedDlc(RailDlcID dlc_id) = 0;

    // @desc Get the number of all the game's DLCs. Do not call this interface before the
    // asynchronous result of AsyncCheckAllDlcsStateReady successfully returns.
    // @return The number of all the game's DLCs
    virtual uint32_t GetDlcCount() = 0;

    // @desc Get DLC info. Do not call this interface before the asynchronous result of
    // AsyncCheckAllDlcsStateReady successfully returns.
    // @param index for the DLC
    // @param dlc_info Pointer to the DLC info retrieved. Please check rail_dlc_define.h for the
    // definition of RailDlcInfo.
    // @return False if 'index' is out of range or 'dlc_info' is null
    virtual bool GetDlcInfo(uint32_t index, RailDlcInfo* dlc_info) = 0;

    // @desc Download and install DLC if the current player owns the specified DLC. The installation
    // request will be sent to the platform client.
    // Usually DLC will be automatically installed if the player owns the DLC. However, under some
    // occasions you may want to let players install DLC from inside the game. For example, the
    // player uninstalled the DLC earlier but need to play the DLC now.
    // Firstly, it will trigger the event kRailEventDlcInstallStartResult
    // Secondly, it will trigger the event kRailEventDlcInstallProgress. If the DLC files have been
    // downloaded before, kRailEventDlcInstallProgress will not be triggered.
    // Thirdly, it will trigger the event kRailEventAppsDlcInstallFinished
    // If the dlc is already installed, none of the above events will be triggered.
    // @param dlc_id The ID of the DLC to check
    // @return False for any of the below occasions
    //   (a) The current player does not own the DLC
    //   (b) The DLC is already installed or removed
    //   (c) Fails to send the installation request to the platform client
    virtual bool AsyncInstallDlc(RailDlcID dlc_id, const RailString& user_data) = 0;

    // @desc Remove the DLC of 'clc_id'. Please note this will not really delete the DLC files.
    // It will only send the request to the platform's client, and the client will only mark
    // the DLC as uninstalled. If the DLC has its own uninstall.exe configured in the configuration
    // file, it will be executed. Please see wiki for details.
    // @param user_data Will be copied to the asynchronous result
    // @return False for any of the below occasions
    //   (a) The current player does not own the DLC
    //   (b) The DLC is already installed or removed
    //   (c) Fails to send the installation request to the platform client
    virtual bool AsyncRemoveDlc(RailDlcID dlc_id, const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_DLC_HELPER_H
