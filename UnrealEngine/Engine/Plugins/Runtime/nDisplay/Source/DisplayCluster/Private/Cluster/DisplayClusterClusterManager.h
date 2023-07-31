// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/DisplayClusterClusterEvent.h"
#include "Misc/App.h"
#include "HAL/Event.h"

class ADisplayClusterSettings;
class FJsonObject;

/**
 * Cluster manager. Responsible for network communication and data replication.
 */
class FDisplayClusterClusterManager
	: public IPDisplayClusterClusterManager
{
public:
	FDisplayClusterClusterManager();
	virtual ~FDisplayClusterClusterManager();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(UDisplayClusterConfigurationData* InConfigData, const FString& NodeID) override;
	virtual void EndSession() override;
	virtual bool StartScene(UWorld* World) override;
	virtual void EndScene() override;
	virtual void StartFrame(uint64 FrameNum) override;
	virtual void EndFrame(uint64 FrameNum) override;
	virtual void PreTick(float DeltaSeconds) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostTick(float DeltaSeconds) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsPrimary()   const override;
	virtual bool IsSecondary() const override;
	virtual bool IsBackup()    const override;
	virtual EDisplayClusterNodeRole GetClusterRole() const override;

	virtual FString GetNodeId() const override
	{
		return ClusterNodeId;
	}

	virtual uint32 GetNodesAmount() const override
	{
		return static_cast<uint32>(ClusterNodeIds.Num());
	}

	virtual void GetNodeIds(TArray<FString>& OutNodeIds) const override;

	virtual bool DropClusterNode(const FString& NodeId) override;

	virtual void RegisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj, EDisplayClusterSyncGroup SyncGroup) override;
	virtual void UnregisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj) override;

	virtual void AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener>) override;
	virtual void RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener>) override;

	virtual void AddClusterEventJsonListener(const FOnClusterEventJsonListener& Listener) override;
	virtual void RemoveClusterEventJsonListener(const FOnClusterEventJsonListener& Listener) override;

	virtual void AddClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener) override;
	virtual void RemoveClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener) override;

	virtual void EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly) override;
	virtual void EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) override;

	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson&   Event, bool bPrimaryOnly) override;
	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual IDisplayClusterClusterNodeController*  GetClusterNodeController()  const override;
	virtual IDisplayClusterFailoverNodeController* GetFailoverNodeController() const override;

	// Sync time data
	virtual void SyncTimeData() override;
	virtual void ExportTimeData(      double& OutDeltaTime,      double& OutGameTime,      TOptional<FQualifiedFrameTime>& OutFrameTime) override;
	virtual void ImportTimeData(const double& InDeltaTime, const double& InGameTime, const TOptional<FQualifiedFrameTime>& InFrameTime) override;

	// Sync objects
	virtual void SyncObjects(EDisplayClusterSyncGroup SyncGroup) override;
	virtual void ExportObjectsData(const EDisplayClusterSyncGroup InSyncGroup,       TMap<FString, FString>& OutObjectsData) override;
	virtual void ImportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, const TMap<FString, FString>& InObjectsData) override;

	// Sync cluster events
	virtual void SyncEvents()  override;
	virtual void ExportEventsData(      TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents,      TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) override;
	virtual void ImportEventsData(const TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& InJsonEvents, const TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& InBinaryEvents) override;

	// Sync native input
	virtual void ExportNativeInputData(TMap<FString, FString>&  OutNativeInputData) override;
	virtual void ImportNativeInputData(TMap<FString, FString>&& InNativeInputData) override;

private:
	// Factory methods (never return nullptr)
	TUniquePtr<IDisplayClusterClusterNodeController>  CreateClusterNodeController() const;
	TUniquePtr<IDisplayClusterFailoverNodeController> CreateFailoverNodeController() const;

	// Cluster event handlers
	void OnClusterEventJsonHandler(const FDisplayClusterClusterEventJson& Event);
	void OnClusterEventBinaryHandler(const FDisplayClusterClusterEventBinary& Event);

	// Internal sync objects control
	void SetInternalSyncObjectsReleaseState(bool bRelease);

private:
	// Cluster controller
	TUniquePtr<IDisplayClusterClusterNodeController>  ClusterNodeCtrl;
	// Failover controller
	TUniquePtr<IDisplayClusterFailoverNodeController> FailoverNodeCtrl;

	// Custom list of cluster nodes
	TArray<FString> ClusterNodeIds;

	// Current operation mode
	EDisplayClusterOperationMode CurrentOperationMode;
	// Current node ID
	FString ClusterNodeId;
	// Current world
	UWorld* CurrentWorld;


	// Time data - replication
	FEventRef TimeDataCacheReadySignal{ EEventMode::ManualReset };
	double DeltaTimeCache = 0.f;
	double GameTimeCache  = 0.f;
	TOptional<FQualifiedFrameTime> FrameTimeCache;

	// Sync objects
	TMap<EDisplayClusterSyncGroup, TSet<IDisplayClusterClusterSyncObject*>> ObjectsToSync;
	mutable FCriticalSection ObjectsToSyncCS;
	// Sync objects - replication
	TMap<EDisplayClusterSyncGroup, FEvent*> ObjectsToSyncCacheReadySignals;
	TMap<EDisplayClusterSyncGroup, TMap<FString, FString>> ObjectsToSyncCache;

	// Native input - replication
	FEventRef NativeInputCacheReadySignal{ EEventMode::ManualReset };
	TMap<FString, FString> NativeInputCache;

	// JSON events
	TMap<bool, TMap<FString, TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>> ClusterEventsJson;
	TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>> ClusterEventsJsonNonDiscarded;
	mutable FCriticalSection     ClusterEventsJsonCS;
	FOnClusterEventJson          OnClusterEventJson;
	
	// Binary events
	TMap<bool, TMap<int32, TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>> ClusterEventsBinary;
	TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>> ClusterEventsBinaryNonDiscarded;
	mutable FCriticalSection   ClusterEventsBinaryCS;
	FOnClusterEventBinary      OnClusterEventBinary;

	// JSON/Binary events - replication
	FEventRef CachedEventsDataSignal{ EEventMode::ManualReset };
	TArray<TSharedPtr<FDisplayClusterClusterEventJson,   ESPMode::ThreadSafe>> JsonEventsCache;
	TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>> BinaryEventsCache;

	// Cluster event listeners
	mutable FCriticalSection ClusterEventListenersCS;
	TArray<TScriptInterface<IDisplayClusterClusterEventListener>> ClusterEventListeners;

	mutable FCriticalSection InternalsCS;
};
