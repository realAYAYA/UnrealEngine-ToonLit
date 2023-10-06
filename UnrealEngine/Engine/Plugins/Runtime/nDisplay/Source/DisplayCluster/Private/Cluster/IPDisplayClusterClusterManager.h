// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/IDisplayClusterClusterManager.h"
#include "IPDisplayClusterManager.h"

#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"

class IDisplayClusterClusterNodeController;
class IDisplayClusterFailoverNodeController;
class FJsonObject;
struct FQualifiedFrameTime;


/**
 * Cluster manager private interface
 */
class IPDisplayClusterClusterManager :
	public IDisplayClusterClusterManager,
	public IPDisplayClusterManager
{
public:
	virtual ~IPDisplayClusterClusterManager() = default;

public:
	virtual IDisplayClusterClusterNodeController*  GetClusterNodeController() const = 0;
	virtual IDisplayClusterFailoverNodeController* GetFailoverNodeController() const = 0;

	// Time data sync
	virtual void SyncTimeData() = 0;
	virtual void ExportTimeData(      double& OutDeltaTime,      double& OutGameTime,      TOptional<FQualifiedFrameTime>& OutFrameTime) = 0;
	virtual void ImportTimeData(const double& InDeltaTime, const double& InGameTime, const TOptional<FQualifiedFrameTime>& InFrameTime) = 0;

	// Objects sync
	virtual void SyncObjects(EDisplayClusterSyncGroup SyncGroup) = 0;
	virtual void ExportObjectsData(const EDisplayClusterSyncGroup InSyncGroup,       TMap<FString, FString>& OutObjectsData) = 0;
	virtual void ImportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, const TMap<FString, FString>& InObjectsData) = 0;

	// Cluster events sync
	virtual void SyncEvents() = 0;
	virtual void ExportEventsData(      TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents,      TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) = 0;
	virtual void ImportEventsData(const TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& InJsonEvents, const TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& InBinaryEvents) = 0;

	// Native input sync
	virtual void ExportNativeInputData(TMap<FString, FString>&  OutNativeInputData) = 0;
	virtual void ImportNativeInputData(TMap<FString, FString>&& InNativeInputData) = 0;
};
