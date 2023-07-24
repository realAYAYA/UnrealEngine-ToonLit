// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/DisplayClusterClusterManager.h"
#include "Cluster/DisplayClusterClusterEvent.h"
#include "Cluster/DisplayClusterClusterEventHandler.h"

#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "Cluster/IDisplayClusterClusterEventListener.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlEditor.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlNull.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlPrimary.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSecondary.h"
#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlNull.h"
#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlPrimary.h"
#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlSecondary.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Dom/JsonObject.h"
#include "HAL/Event.h"

#include "Misc/DisplayClusterTypesConverter.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "Misc/App.h"
#include "Misc/QualifiedFrameTime.h"

#include "UObject/Interface.h"


FDisplayClusterClusterManager::FDisplayClusterClusterManager()
{
	// Sync objects
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::PreTick).Reserve(64);
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::Tick).Reserve(64);
	ObjectsToSync.Emplace(EDisplayClusterSyncGroup::PostTick).Reserve(64);

	// Sync objects - replication
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::PreTick);
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::Tick);
	ObjectsToSyncCache.Emplace(EDisplayClusterSyncGroup::PostTick);

	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::PreTick,  FPlatformProcess::GetSynchEventFromPool(true));
	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::Tick,     FPlatformProcess::GetSynchEventFromPool(true));
	ObjectsToSyncCacheReadySignals.Emplace(EDisplayClusterSyncGroup::PostTick, FPlatformProcess::GetSynchEventFromPool(true));

	// Set cluster event handlers. These are the entry points for any incoming cluster events.
	OnClusterEventJson.AddRaw(this,   &FDisplayClusterClusterManager::OnClusterEventJsonHandler);
	OnClusterEventBinary.AddRaw(this, &FDisplayClusterClusterManager::OnClusterEventBinaryHandler);

	// Set internal system events handler
	OnClusterEventJson.Add(FDisplayClusterClusterEventHandler::Get().GetJsonListenerDelegate());
}

FDisplayClusterClusterManager::~FDisplayClusterClusterManager()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Releasing cluster manager..."));

	// Trigger all data cache availability events to prevent client session threads to be deadlocked.
	SetInternalSyncObjectsReleaseState(true);

	// Also stop clients/servers
	if (ClusterNodeCtrl)
	{
		ClusterNodeCtrl->Shutdown();
	}

	for (TPair<EDisplayClusterSyncGroup, FEvent*>& GroupEventIt : ObjectsToSyncCacheReadySignals)
	{
		FPlatformProcess::ReturnSynchEventToPool(GroupEventIt.Value);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::Init(EDisplayClusterOperationMode OperationMode)
{
	CurrentOperationMode = OperationMode;
	return true;
}

void FDisplayClusterClusterManager::Release()
{
}

bool FDisplayClusterClusterManager::StartSession(UDisplayClusterConfigurationData* InConfigData, const FString& InNodeId)
{
	ClusterNodeId = InNodeId;

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Node ID: %s"), *ClusterNodeId);

	// Node name must be valid
	if (ClusterNodeId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node ID was not specified"));
		return false;
	}

	// Get configuration data
	const UDisplayClusterConfigurationData* ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get configuration data"));
		return false;
	}

	if (!ConfigData->Cluster->Nodes.Contains(ClusterNodeId))
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node '%s' not found in the configuration data"), *ClusterNodeId);
		return false;
	}

	// Save node IDs
	ConfigData->Cluster->Nodes.GenerateKeyArray(ClusterNodeIds);

	// Reset all internal sync objects
	SetInternalSyncObjectsReleaseState(false);

	// Instantiate cluster node controller
	ClusterNodeCtrl = CreateClusterNodeController();

	// Initialize the controller
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Initializing the controller..."));
	if (!ClusterNodeCtrl->Initialize())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't initialize a controller."));
		ClusterNodeCtrl = MakeUnique<FDisplayClusterClusterNodeCtrlNull>(FString("[CTRL-NULL]"), ClusterNodeId);
		return false;
	}

	// Instantiate failover node controller
	FailoverNodeCtrl = CreateFailoverNodeController();

	return true;
}

void FDisplayClusterClusterManager::EndSession()
{
	// Trigger all data cache availability events to prevent
	// client session threads to be deadlocked.
	SetInternalSyncObjectsReleaseState(true);

	// Stop local clients/servers
	if (ClusterNodeCtrl)
	{
		ClusterNodeCtrl->Shutdown();
	}

	{
		FScopeLock Lock(&InternalsCS);

		ClusterNodeIds.Reset();
		ClusterNodeId.Empty();
	}
}

bool FDisplayClusterClusterManager::StartScene(UWorld* InWorld)
{
	check(InWorld);
	CurrentWorld = InWorld;

	return true;
}

void FDisplayClusterClusterManager::EndScene()
{
	{
		FScopeLock Lock(&ObjectsToSyncCS);
		for (auto& SyncGroupPair : ObjectsToSync)
		{
			SyncGroupPair.Value.Reset();
		}
	}

	{
		FScopeLock Lock(&ClusterEventListenersCS);
		ClusterEventListeners.Reset();
	}

	NativeInputCache.Reset();
	CurrentWorld = nullptr;
}

void FDisplayClusterClusterManager::StartFrame(uint64 FrameNum)
{
	// Even though this signal gets reset on EndFrame, it's still possible a client
	// will try to synchronize time data before the primary node finishes EndFrame
	// processing. Since time data replication step and EndFrame call don't have
	// any barriers between each other, it's theoretically possible a client will
	// get outdated time information which will break determinism. As a simple
	// solution that requires minimum resources, we do safe signal reset right
	// after WaitForFrameStart barrier, which is called after time data
	// synchronization. As a result, we're 100% sure the clients will always get
	// actual time data.
	TimeDataCacheReadySignal->Reset();
}

void FDisplayClusterClusterManager::EndFrame(uint64 FrameNum)
{
	// Reset all the synchronization objects
	SetInternalSyncObjectsReleaseState(false);

	// Reset cache containers
	JsonEventsCache.Reset();
	BinaryEventsCache.Reset();
	NativeInputCache.Reset();

	// Reset objects sync cache for all sync groups
	for (TPair<EDisplayClusterSyncGroup, TMap<FString, FString>>& It : ObjectsToSyncCache)
	{
		It.Value.Reset();
	}
}

void FDisplayClusterClusterManager::PreTick(float DeltaSeconds)
{
	// Sync cluster objects (PreTick)
	SyncObjects(EDisplayClusterSyncGroup::PreTick);

	// Sync cluster events
	SyncEvents();
}

void FDisplayClusterClusterManager::Tick(float DeltaSeconds)
{
	// Sync cluster objects (Tick)
	SyncObjects(EDisplayClusterSyncGroup::Tick);
}

void FDisplayClusterClusterManager::PostTick(float DeltaSeconds)
{
	// Sync cluster objects (PostTick)
	SyncObjects(EDisplayClusterSyncGroup::PostTick);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::IsPrimary() const
{
	FScopeLock Lock(&InternalsCS);
	return ClusterNodeCtrl ? ClusterNodeCtrl->GetClusterRole() == EDisplayClusterNodeRole::Primary : false;
}

bool FDisplayClusterClusterManager::IsSecondary() const
{
	FScopeLock Lock(&InternalsCS);
	return ClusterNodeCtrl ? ClusterNodeCtrl->GetClusterRole() == EDisplayClusterNodeRole::Secondary : false;
}

bool FDisplayClusterClusterManager::IsBackup() const
{
	FScopeLock Lock(&InternalsCS);
	return ClusterNodeCtrl ? ClusterNodeCtrl->GetClusterRole() == EDisplayClusterNodeRole::Backup : false;
}

EDisplayClusterNodeRole FDisplayClusterClusterManager::GetClusterRole() const
{
	FScopeLock Lock(&InternalsCS);
	return ClusterNodeCtrl ? ClusterNodeCtrl->GetClusterRole() : EDisplayClusterNodeRole::None;
}

void FDisplayClusterClusterManager::GetNodeIds(TArray<FString>& OutNodeIds) const
{
	FScopeLock Lock(&InternalsCS);
	OutNodeIds = ClusterNodeIds;
}

bool FDisplayClusterClusterManager::DropClusterNode(const FString& NodeId)
{
	FScopeLock Lock(&InternalsCS);

	// Pass request to the cluster controller
	return ClusterNodeCtrl ? ClusterNodeCtrl->DropClusterNode(NodeId) : false;
}

void FDisplayClusterClusterManager::RegisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj, EDisplayClusterSyncGroup SyncGroup)
{
	if (SyncObj)
	{
		FScopeLock Lock(&ObjectsToSyncCS);
		ObjectsToSync[SyncGroup].Add(SyncObj);
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Registered sync object: %s"), *SyncObj->GetSyncId());
	}
}

void FDisplayClusterClusterManager::UnregisterSyncObject(IDisplayClusterClusterSyncObject* SyncObj)
{
	if (SyncObj)
	{
		FScopeLock Lock(&ObjectsToSyncCS);

		for (auto& GroupPair : ObjectsToSync)
		{
			GroupPair.Value.Remove(SyncObj);
		}

		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Unregistered sync object: %s"), *SyncObj->GetSyncId());
	}
}

void FDisplayClusterClusterManager::AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	if (Listener.GetObject() && IsValidChecked(Listener.GetObject()) && !Listener.GetObject()->IsUnreachable())
	{
		ClusterEventListeners.Add(Listener);
	}
}

void FDisplayClusterClusterManager::RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	if (ClusterEventListeners.Contains(Listener))
	{
		ClusterEventListeners.Remove(Listener);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Cluster event listeners left: %d"), ClusterEventListeners.Num());
	}
}

void FDisplayClusterClusterManager::AddClusterEventJsonListener(const FOnClusterEventJsonListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventJson.Add(Listener);
}

void FDisplayClusterClusterManager::RemoveClusterEventJsonListener(const FOnClusterEventJsonListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventJson.Remove(Listener.GetHandle());
}

void FDisplayClusterClusterManager::AddClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventBinary.Add(Listener);
}

void FDisplayClusterClusterManager::RemoveClusterEventBinaryListener(const FOnClusterEventBinaryListener& Listener)
{
	FScopeLock Lock(&ClusterEventListenersCS);
	OnClusterEventBinary.Remove(Listener.GetHandle());
}

void FDisplayClusterClusterManager::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("JSON event emission request: %s:%s:%s"), *Event.Category, *Event.Type, *Event.Name);

	FScopeLock Lock(&ClusterEventsJsonCS);

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		// [Primary] Since we receive cluster events asynchronously, we push it to a primary events pool
		if (IsPrimary())
		{
			// Generate event ID
			const FString EventId = FString::Printf(TEXT("%s-%s-%s"), *Event.Category, *Event.Type, *Event.Name);
			// Make it shared ptr
			TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe> EventPtr = MakeShared<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>(Event);
			// Store event object
			if (EventPtr->bShouldDiscardOnRepeat)
			{
				ClusterEventsJson.FindOrAdd(EventPtr->bIsSystemEvent).Emplace(EventId, EventPtr);
			}
			else
			{
				ClusterEventsJsonNonDiscarded.Add(EventPtr);
			}
		}
		// [Secondary] Send event to the primary node
		else
		{
			// An event will be emitted from a secondary node if it's explicitly specified by bPrimaryOnly=false
			if (!bPrimaryOnly && ClusterNodeCtrl)
			{
				ClusterNodeCtrl->EmitClusterEventJson(Event);
			}
		}
	}
}

void FDisplayClusterClusterManager::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("BIN event emission request: %d"), Event.EventId);

	FScopeLock Lock(&ClusterEventsBinaryCS);

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		// [Primary] Since we receive cluster events asynchronously, we push it to a primary events pool
		if (IsPrimary())
		{
			// Make it shared ptr
			TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe> EventPtr = MakeShared<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>(Event);

			if (EventPtr->bShouldDiscardOnRepeat)
			{
				ClusterEventsBinary.FindOrAdd(EventPtr->bIsSystemEvent).Emplace(EventPtr->EventId, EventPtr);
			}
			else
			{
				ClusterEventsBinaryNonDiscarded.Add(EventPtr);
			}
		}
		// [Secondary] Send event to the primary node
		else
		{
			// An event will be emitted from a secondary node if it's explicitly specified by bPrimaryOnly=false
			if (!bPrimaryOnly && ClusterNodeCtrl)
			{
				ClusterNodeCtrl->EmitClusterEventBinary(Event);
			}
		}
	}
}

void FDisplayClusterClusterManager::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		if (ClusterNodeCtrl)
		{
			if (IsPrimary() || !bPrimaryOnly)
			{
				UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("JSON event emission request: recipient=%s:%u, event=%s:%s:%s"), *Address, Port, *Event.Category, *Event.Type, *Event.Name);
				ClusterNodeCtrl->SendClusterEventTo(Address, Port, Event, bPrimaryOnly);
			}
		}
	}
}

void FDisplayClusterClusterManager::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		if (ClusterNodeCtrl)
		{
			if (IsPrimary() || !bPrimaryOnly)
			{
				UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("BIN event emission request: recipient=%s:%u, event=%d"), *Address, Port, Event.EventId);
				ClusterNodeCtrl->SendClusterEventTo(Address, Port, Event, bPrimaryOnly);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
IDisplayClusterClusterNodeController* FDisplayClusterClusterManager::GetClusterNodeController() const
{
	FScopeLock Lock(&InternalsCS);
	return ClusterNodeCtrl.Get();
}

IDisplayClusterFailoverNodeController* FDisplayClusterClusterManager::GetFailoverNodeController() const
{
	FScopeLock Lock(&InternalsCS);
	return FailoverNodeCtrl.Get();
}


void FDisplayClusterClusterManager::SyncTimeData()
{
	if (ClusterNodeCtrl)
	{
		double DeltaTime = 0.0f;
		double GameTime = 0.0f;
		TOptional<FQualifiedFrameTime> FrameTime;

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (time)..."));
		ClusterNodeCtrl->GetTimeData(DeltaTime, GameTime, FrameTime);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Delta=%lf, Game=%lf, Frame=%lf"), DeltaTime, GameTime, FrameTime.IsSet() ? FrameTime.GetValue().AsSeconds() : 0.f);

		// Apply new time data (including primary node)
		ImportTimeData(DeltaTime, GameTime, FrameTime);
	}
}

void FDisplayClusterClusterManager::ExportTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	if (IsInGameThread())
	{
		// Cache data so it will be the same for all requests within current frame
		DeltaTimeCache = FApp::GetDeltaTime();
		GameTimeCache  = FApp::GetGameTime();
		FrameTimeCache = FApp::GetCurrentFrameTime();

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Time data cache: Delta=%lf, Game=%lf, Frame=%lf"), DeltaTimeCache, GameTimeCache, FrameTimeCache.IsSet() ? FrameTimeCache.GetValue().AsSeconds() : 0.f);

		TimeDataCacheReadySignal->Trigger();
	}

	// Wait until data is available
	TimeDataCacheReadySignal->Wait();

	// Return cached values
	OutDeltaTime = DeltaTimeCache;
	OutGameTime  = GameTimeCache;
	OutFrameTime = FrameTimeCache;
}

void FDisplayClusterClusterManager::ImportTimeData(const double& InDeltaTime, const double& InGameTime, const TOptional<FQualifiedFrameTime>& InFrameTime)
{
	// Compute new 'current' and 'last' time on the local platform timeline
	const double NewCurrentTime = FPlatformTime::Seconds();
	const double NewLastTime = NewCurrentTime - InDeltaTime;

	// Store new data
	FApp::SetCurrentTime(NewLastTime);
	FApp::UpdateLastTime();
	FApp::SetCurrentTime(NewCurrentTime);
	FApp::SetDeltaTime(InDeltaTime);
	FApp::SetGameTime(InGameTime);
	FApp::SetIdleTime(0);
	FApp::SetIdleTimeOvershoot(0);

	if (InFrameTime.IsSet())
	{
		FApp::SetCurrentFrameTime(InFrameTime.GetValue());
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("DisplayCluster timecode: %s | %s"), *FTimecode::FromFrameNumber(InFrameTime->Time.GetFrame(), InFrameTime->Rate).ToString(), *InFrameTime->Rate.ToPrettyText().ToString());
	}
	else
	{
		FApp::InvalidateCurrentFrameTime();
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("DisplayCluster timecode: Invalid"));
	}
}

void FDisplayClusterClusterManager::SyncObjects(EDisplayClusterSyncGroup InSyncGroup)
{
	if (ClusterNodeCtrl)
	{
		TMap<FString, FString> ObjectsData;

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (objects)..."));
		ClusterNodeCtrl->GetObjectsData(InSyncGroup, ObjectsData);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d records (objects)."), ObjectsData.Num());

		// No need to import data on the primary node
		if (!IsPrimary())
		{
			// Perform data load (objects state update)
			ImportObjectsData(InSyncGroup, ObjectsData);
		}
	}
}

void FDisplayClusterClusterManager::ExportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	// Clean output buffer just in case
	OutObjectsData.Reset();

	if(IsInGameThread())
	{
		FScopeLock Lock(&ObjectsToSyncCS);

		// Cache data for requested sync group
		if (TMap<FString, FString>* GroupCache = ObjectsToSyncCache.Find(InSyncGroup))
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Exporting sync data for sync group: %u, items to sync: %d"), (uint8)InSyncGroup, GroupCache->Num());

			for (IDisplayClusterClusterSyncObject* SyncObj : ObjectsToSync[InSyncGroup])
			{
				if (SyncObj && SyncObj->IsActive() && SyncObj->IsDirty())
				{
					UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Adding object to sync: %s"), *SyncObj->GetSyncId());

					const FString SyncId   = SyncObj->GetSyncId();
					const FString SyncData = SyncObj->SerializeToString();

					UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Sync object: %s - %s"), *SyncId, *SyncData);

					// Cache the object
					GroupCache->Emplace(SyncId, SyncData);

					SyncObj->ClearDirty();
				}
			}
		}

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Objects data cache contains %d records"), ObjectsToSyncCache[InSyncGroup].Num());

		// Notify data is available
		ObjectsToSyncCacheReadySignals[InSyncGroup]->Trigger();
	}

	// Wait until primary node provides data
	ObjectsToSyncCacheReadySignals[InSyncGroup]->Wait();
	// Return cached value
	OutObjectsData = ObjectsToSyncCache[InSyncGroup];
}

void FDisplayClusterClusterManager::ImportObjectsData(const EDisplayClusterSyncGroup InSyncGroup, const TMap<FString, FString>& InObjectsData)
{
	if (InObjectsData.Num() > 0)
	{
		for (auto It = InObjectsData.CreateConstIterator(); It; ++It)
		{
			UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("sync-data: %s=%s"), *It->Key, *It->Value);
		}

		FScopeLock Lock(&ObjectsToSyncCS);

		for (IDisplayClusterClusterSyncObject* SyncObj : ObjectsToSync[InSyncGroup])
		{
			if (SyncObj && SyncObj->IsActive())
			{
				const FString SyncId = SyncObj->GetSyncId();
				if (!InObjectsData.Contains(SyncId))
				{
					UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("%s has nothing to update"), *SyncId);
					continue;
				}

				if (SyncObj->DeserializeFromString(InObjectsData[SyncId]))
				{
					UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Synchronized object: %s"), *SyncId);
				}
				else
				{
					UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't apply sync data for sync object %s"), *SyncId);
				}
			}
		}
	}
}

void FDisplayClusterClusterManager::SyncEvents()
{
	if (ClusterNodeCtrl)
	{
		TArray<TSharedPtr<FDisplayClusterClusterEventJson,   ESPMode::ThreadSafe>> JsonEvents;
		TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>> BinaryEvents;

		// Get events data from a provider
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (events)..."));
		ClusterNodeCtrl->GetEventsData(JsonEvents, BinaryEvents);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available events: json=%d binary=%d"), JsonEvents.Num(), BinaryEvents.Num());

		// Import and process them
		ImportEventsData(JsonEvents, BinaryEvents);
	}
}

void FDisplayClusterClusterManager::ExportEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents)
{
	// Clear output containers just in case
	OutJsonEvents.Reset();
	OutBinaryEvents.Reset();

	if (IsInGameThread())
	{
		// Export JSON events
		{
			FScopeLock Lock(&ClusterEventsJsonCS);

			// Export all system and non-system json events that have 'discard on repeat' flag
			for (const TPair<bool, TMap<FString, TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>>& It : ClusterEventsJson)
			{
				TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>> JsonEventsToExport;
				It.Value.GenerateValueArray(JsonEventsToExport);
				JsonEventsCache.Append(MoveTemp(JsonEventsToExport));
			}

			// Clear original containers
			ClusterEventsJson.Reset();

			// Export all json events that don't have 'discard on repeat' flag
			JsonEventsCache.Append(MoveTemp(ClusterEventsJsonNonDiscarded));
		}

		// Export binary events
		{
			FScopeLock Lock(&ClusterEventsBinaryCS);

			// Export all binary events that have 'discard on repeat' flag
			for (const TPair<bool, TMap<int32, TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>>& It : ClusterEventsBinary)
			{
				TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>> BinaryEventsToExport;
				It.Value.GenerateValueArray(BinaryEventsToExport);
				BinaryEventsCache.Append(MoveTemp(BinaryEventsToExport));
			}

			// Clear original containers
			ClusterEventsBinary.Reset();

			// Export all binary events that don't have 'discard on repeat' flag
			BinaryEventsCache.Append(MoveTemp(ClusterEventsBinaryNonDiscarded));
		}

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Cluster events data cache contains: json=%d, binary=%d"), JsonEventsCache.Num(), BinaryEventsCache.Num());

		// Notify data is available
		CachedEventsDataSignal->Trigger();
	}

	// Wait until data is available
	CachedEventsDataSignal->Wait();

	// Return cached value
	OutJsonEvents   = JsonEventsCache;
	OutBinaryEvents = BinaryEventsCache;
}

void FDisplayClusterClusterManager::ImportEventsData(const TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& InJsonEvents, const TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& InBinaryEvents)
{
	// Process and fire all JSON events
	if (InJsonEvents.Num() > 0)
	{
		FScopeLock LockListeners(&ClusterEventListenersCS);

		for (const TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>& Event : InJsonEvents)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Processing json event %s|%s|%s|s%d|d%d..."), *Event->Category, *Event->Type, *Event->Name, Event->bIsSystemEvent ? 1 : 0, Event->bShouldDiscardOnRepeat ? 1 : 0);
			// Fire event
			OnClusterEventJson.Broadcast(*Event);
		}
	}

	// Process and fire all binary events
	if (InBinaryEvents.Num() > 0)
	{
		FScopeLock LockListeners(&ClusterEventListenersCS);

		for (const TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>& Event : InBinaryEvents)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Processing binary event %d..."), Event->EventId);
			// Fire event
			OnClusterEventBinary.Broadcast(*Event);
		}
	}
}

void FDisplayClusterClusterManager::ImportNativeInputData(TMap<FString, FString>&& InNativeInputData)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Caching native input data, %d records available"), InNativeInputData.Num());

	// Cache input data
	NativeInputCache = InNativeInputData;

	UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Native input data cache:"));

	int32 Idx = 0;
	for (auto It = NativeInputCache.CreateConstIterator(); It; ++It, ++Idx)
	{
		UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Native input data cache: %d - %s - %s"), Idx, *It->Key, *It->Value);
	}

	// Notify the data is available
	NativeInputCacheReadySignal->Trigger();
}

void FDisplayClusterClusterManager::ExportNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	// If we are on the primary node, that means we got network request for input data.
	// So, provide with data once it's cached.
	if (IsPrimary())
	{
		// Wait for data cache to be ready
		NativeInputCacheReadySignal->Wait();
		// Export data from cache
		OutNativeInputData = NativeInputCache;
	}
	// Otherwise we need to send network request to download input data
	else
	{
		if (ClusterNodeCtrl)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading native input data..."));
			ClusterNodeCtrl->GetNativeInputData(OutNativeInputData);
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d native input records."), OutNativeInputData.Num());

			int32 Idx = 0;
			for (auto It = OutNativeInputData.CreateConstIterator(); It; ++It, ++Idx)
			{
				UE_LOG(LogDisplayClusterCluster, VeryVerbose, TEXT("Native input data: %d - %s - %s"), Idx, *It->Key, *It->Value);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
TUniquePtr<IDisplayClusterClusterNodeController> FDisplayClusterClusterManager::CreateClusterNodeController() const
{
	// Instantiate appropriate controller depending on operation mode and cluster role
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		// Primary or secondary
		if (ClusterNodeId.Equals(GDisplayCluster->GetPrivateConfigMgr()->GetPrimaryNodeId(), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating primary node controller..."));
			return MakeUnique<FDisplayClusterClusterNodeCtrlPrimary>(FString("[CTRL-M]"), ClusterNodeId);
		}
		else
		{
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating secondary node controller..."));
			return MakeUnique<FDisplayClusterClusterNodeCtrlSecondary>(FString("[CTRL-S]"), ClusterNodeId);
		}
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating editor node controller..."));
		return MakeUnique<FDisplayClusterClusterNodeCtrlEditor>(FString("[CTRL-EDTR]"), FString("editor"));
	}

	// Including CurrentOperationMode == EDisplayClusterOperationMode::Disabled
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Controller is not required"));
	return MakeUnique<FDisplayClusterClusterNodeCtrlNull>(FString("[CTRL-NULL]"), ClusterNodeId);
}

TUniquePtr<IDisplayClusterFailoverNodeController> FDisplayClusterClusterManager::CreateFailoverNodeController() const
{
	// Instantiate appropriate controller depending on operation mode and cluster role
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		const EDisplayClusterNodeRole ClusterRole = GetClusterRole();
		switch (ClusterRole)
		{
		case EDisplayClusterNodeRole::Primary:
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating failover primary controller..."));
			return MakeUnique<FDisplayClusterFailoverNodeCtrlPrimary>();

		case EDisplayClusterNodeRole::Secondary:
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating failover secondary controller..."));
			return MakeUnique<FDisplayClusterFailoverNodeCtrlSecondary>();
		}
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating failover null controller..."));
	return MakeUnique<FDisplayClusterFailoverNodeCtrlNull>();
}

void FDisplayClusterClusterManager::OnClusterEventJsonHandler(const FDisplayClusterClusterEventJson& Event)
{
	FScopeLock Lock(&ClusterEventListenersCS);

	decltype(ClusterEventListeners) InvalidListeners;

	for (auto Listener : ClusterEventListeners)
	{
		if (!Listener.GetObject() || !IsValidChecked(Listener.GetObject()) || Listener.GetObject()->IsUnreachable()) // Note: .GetInterface() is always returning null when intefrace is added to class in the Blueprint.
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Will remove invalid cluster event listener"));
			InvalidListeners.Add(Listener);
			continue;
		}
		Listener->Execute_OnClusterEventJson(Listener.GetObject(), Event);
	}

	for (auto& InvalidListener : InvalidListeners)
	{
		ClusterEventListeners.Remove(InvalidListener);
	}
}

void FDisplayClusterClusterManager::OnClusterEventBinaryHandler(const FDisplayClusterClusterEventBinary& Event)
{
	FScopeLock Lock(&ClusterEventListenersCS);

	decltype(ClusterEventListeners) InvalidListeners;

	for (auto Listener : ClusterEventListeners)
	{
		if (!Listener.GetObject() || !IsValidChecked(Listener.GetObject()) || Listener.GetObject()->IsUnreachable()) // Note: .GetInterface() is always returning null when intefrace is added to class in the Blueprint.
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Will remove invalid cluster event listener"));
			InvalidListeners.Add(Listener);
			continue;
		}

		Listener->Execute_OnClusterEventBinary(Listener.GetObject(), Event);
	}

	for (auto& InvalidListener : InvalidListeners)
	{
		ClusterEventListeners.Remove(InvalidListener);
	}
}

void FDisplayClusterClusterManager::SetInternalSyncObjectsReleaseState(bool bRelease)
{
	if (bRelease)
	{
		// Set all events signaled
		TimeDataCacheReadySignal->Trigger();
		CachedEventsDataSignal->Trigger();
		NativeInputCacheReadySignal->Trigger();

		for (TPair<EDisplayClusterSyncGroup, FEvent*>& It : ObjectsToSyncCacheReadySignals)
		{
			It.Value->Trigger();
		}
	}
	else
	{
		// Reset all cache events
		TimeDataCacheReadySignal->Reset();
		CachedEventsDataSignal->Reset();
		NativeInputCacheReadySignal->Reset();

		// Reset events for all sync groups
		for (TPair<EDisplayClusterSyncGroup, FEvent*>& It : ObjectsToSyncCacheReadySignals)
		{
			It.Value->Reset();
		}
	}
}
