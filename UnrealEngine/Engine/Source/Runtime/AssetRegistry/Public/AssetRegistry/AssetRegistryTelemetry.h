// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/ArrayView.h"
#include "Misc/Guid.h"

struct FFileChangeData;

namespace UE::Telemetry::AssetRegistry
{
    // Information about very early asset registry startup.
    struct FStartupTelemetry
    {
        static constexpr inline FGuid TelemetryID = FGuid(0x903732ce, 0x8bd04eb9, 0x92170fde, 0x0a1c1562);
        
        // Time spent in synchronous initialization
        double StartupDuration = 0.0;
        // Whether async gather operation was started at this time
        bool bStartedAsyncGather = false;
    };

    // Information about a synchronous scan that was executed, blocking the calling thread.
    struct FSynchronousScanTelemetry
    {
        static constexpr inline FGuid TelemetryID = FGuid(0x2b4b9f1a, 0xdcfd4958, 0xbe43ba9d, 0xae309392);
        
        // List of directories that were scanned
        TConstArrayView<FString> Directories;
        // List of specific files that were scanned
        TConstArrayView<FString> Files;
        // Flags controlling scan behavior
        UE::AssetRegistry::EScanFlags Flags = UE::AssetRegistry::EScanFlags::None;
        // Number of assets found by this scan
        int32 NumFoundAssets = 0;
        // Duration of synchronous scan
        double Duration = 0.0;
        // Whether the main background async gather was started at this time
        bool bInitialSearchStarted = false;
        // Whether the main background async gather was completed at this time
        bool bInitialSearchCompleted = false;
    };

    // Information about the initial asset registry scan triggered when launching the process. 
    // This is also a good time to gather information about the contents of the asset registry.
    struct FGatherTelemetry
    {
        static constexpr inline FGuid TelemetryID = FGuid(0xafcec052, 0x5d2c4850, 0xbfc6d11d, 0x3163ccd5);
        
        // Total wall clock time between start of search and completion.
        double TotalSearchDurationSeconds = 0.0;
        // Total work time (includes work done in parallel stages)
        double TotalWorkTimeSeconds = 0.0;
        // Time spent discovering asset files on disk
        double DiscoveryTimeSeconds = 0.0;
        // Time spent gathering asset data from files on disk (or cache)
        double GatherTimeSeconds = 0.0;
        // Time spent storing asset data in the asset registry for searching
        double StoreTimeSeconds = 0.0;
        // Number of directories read from cache
        int32 NumCachedDirectories = 0;
        // Number of directories scanned from disk
        int32 NumUncachedDirectories = 0;
        // Number of asset files read from cache
        int32 NumCachedAssetFiles = 0;
        // Number of asset files read loose from disk
        int32 NumUncachedAssetFiles = 0;
    };
    
    // Information about an asset registry update that was triggered by the directory watcher module. 
    struct FDirectoryWatcherUpdateTelemetry
    {
        static constexpr inline FGuid TelemetryID = FGuid(0xa1da56e1, 0xe1314918, 0xba6fdb24, 0xb8911ce0);
        
        // File change data from watcher
        TConstArrayView<FFileChangeData> Changes;
        // Total time spent in update handler
        double DurationSeconds = 0.0;
        // Whether the main background async gather was started at this time
        bool bInitialSearchStarted = false;
        // Whether the main background async gather was completed at this time
        bool bInitialSearchCompleted = false;
    };
}