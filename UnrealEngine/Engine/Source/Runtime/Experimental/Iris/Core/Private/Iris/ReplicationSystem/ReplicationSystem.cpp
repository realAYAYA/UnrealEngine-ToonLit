// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/Core/IrisDebugging.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisMemoryTracker.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/NetObjectReference.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationSystem/ChangeMaskCache.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"
#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/ReplicationReader.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationTypes.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/IrisObjectReferencePackageMap.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "UObject/UObjectGlobals.h"

namespace ReplicationSystemCVars
{
#if !UE_BUILD_SHIPPING
static bool bForcePruneBeforeUpdate = false;
static FAutoConsoleVariableRef CVarForcePruneBeforeUpdate(TEXT("net.Iris.ForcePruneBeforeUpdate"), bForcePruneBeforeUpdate, TEXT("Verify integrity of all tracked instances at the start of every update."));
#endif
}

namespace UE::Net::Private
{

class FReplicationSystemImpl
{
public:
	UReplicationSystem* ReplicationSystem;
	FReplicationSystemInternal ReplicationSystemInternal;
	uint64 IrisDebugHelperDummy = 0U;

	explicit FReplicationSystemImpl(UReplicationSystem* InReplicationSystem, const UReplicationSystem::FReplicationSystemParams& Params)
	: ReplicationSystem(InReplicationSystem)
	, ReplicationSystemInternal(FReplicationSystemInternalInitParams({ InReplicationSystem->GetId(), Params.MaxReplicatedObjectCount }))
	{
	}

	~FReplicationSystemImpl()
	{
	}

	void InitDefaultFilteringGroups()
	{
		FNetObjectGroupHandle LocalNotReplicatedGroupHande = ReplicationSystem->CreateGroup();
		check(LocalNotReplicatedGroupHande == NotReplicatedNetObjectGroupHandle);
		ReplicationSystem->AddGroupFilter(NotReplicatedNetObjectGroupHandle);
		
		// Setup SubObjectFiltering groups
		FNetObjectGroupHandle LocalNetGroupOwnerNetObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(UE::Net::NetGroupOwner);
		check(LocalNetGroupOwnerNetObjectGroupHandle == NetGroupOwnerNetObjectGroupHandle);

		FNetObjectGroupHandle LocalNetGroupReplayNetObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(UE::Net::NetGroupReplay);
		check(LocalNetGroupReplayNetObjectGroupHandle == NetGroupReplayNetObjectGroupHandle);
	}

	void Init(const UReplicationSystem::FReplicationSystemParams& Params)
	{
		LLM_SCOPE_BYTAG(Iris);

#if !UE_BUILD_SHIPPING
		IrisDebugHelperDummy = UE::Net::IrisDebugHelper::Init();
#endif

		const uint32 ReplicationSystemId = ReplicationSystem->GetId();

		FNetRefHandleManager& NetRefHandleManager = ReplicationSystemInternal.GetNetRefHandleManager();

		const uint32 MaxObjectCount =  NetRefHandleManager.GetMaxActiveObjectCount();

		// $IRIS TODO: Need object ID range. Currently abusing hardcoded values from FNetRefHandleManager
		FDirtyNetObjectTracker& DirtyNetObjectTracker = ReplicationSystemInternal.GetDirtyNetObjectTracker();
		FDirtyNetObjectTrackerInitParams DirtyNetObjectTrackerInitParams;
		{
			DirtyNetObjectTrackerInitParams.NetRefHandleManager = &NetRefHandleManager;
			DirtyNetObjectTrackerInitParams.ReplicationSystemId = ReplicationSystemId;
			DirtyNetObjectTrackerInitParams.MaxObjectCount = MaxObjectCount;
			DirtyNetObjectTrackerInitParams.NetObjectIndexRangeStart = 1;
			DirtyNetObjectTrackerInitParams.NetObjectIndexRangeEnd = MaxObjectCount - 1U;

			DirtyNetObjectTracker.Init(DirtyNetObjectTrackerInitParams);
		}

		FReplicationStateStorage& StateStorage = ReplicationSystemInternal.GetReplicationStateStorage();
		{
			FReplicationStateStorageInitParams InitParams;
			InitParams.ReplicationSystem = ReplicationSystem;
			InitParams.NetRefHandleManager = &NetRefHandleManager;
			InitParams.MaxObjectCount = MaxObjectCount;
			InitParams.MaxConnectionCount = ReplicationSystemInternal.GetConnections().GetMaxConnectionCount();
			InitParams.MaxDeltaCompressedObjectCount = Params.MaxDeltaCompressedObjectCount;
			StateStorage.Init(InitParams);
		}

		FNetObjectGroups& Groups = ReplicationSystemInternal.GetGroups();
		{
			FNetObjectGroupInitParams InitParams = {};
			InitParams.MaxObjectCount = MaxObjectCount;
			InitParams.MaxGroupCount = Params.MaxNetObjectGroupCount;

			Groups.Init(InitParams);
		}

		FReplicationStateDescriptorRegistry& Registry = ReplicationSystemInternal.GetReplicationStateDescriptorRegistry();
		{
			FReplicationStateDescriptorRegistryInitParams InitParams = {};
			InitParams.ProtocolManager = &ReplicationSystemInternal.GetReplicationProtocolManager();

			Registry.Init(InitParams);
		}		

		FNetCullDistanceOverrides& NetCullDistanceOverrides = ReplicationSystemInternal.GetNetCullDistanceOverrides();
		{
			FNetCullDistanceOverridesInitParams InitParams;
			InitParams.MaxObjectCount = MaxObjectCount;
			NetCullDistanceOverrides.Init(InitParams);
		}

		FWorldLocations& WorldLocations = ReplicationSystemInternal.GetWorldLocations();
		{
			FWorldLocationsInitParams InitParams;
			InitParams.MaxObjectCount = MaxObjectCount;
			WorldLocations.Init(InitParams);
		}
	
		FDeltaCompressionBaselineInvalidationTracker& DeltaCompressionBaselineInvalidationTracker = ReplicationSystemInternal.GetDeltaCompressionBaselineInvalidationTracker();
		FDeltaCompressionBaselineManager& DeltaCompressionBaselineManager = ReplicationSystemInternal.GetDeltaCompressionBaselineManager();
		{
			FDeltaCompressionBaselineInvalidationTrackerInitParams InitParams;
			InitParams.BaselineManager = &DeltaCompressionBaselineManager;
			InitParams.MaxObjectCount = MaxObjectCount;
			DeltaCompressionBaselineInvalidationTracker.Init(InitParams);
		}
		{
			FDeltaCompressionBaselineManagerInitParams InitParams;
			InitParams.BaselineInvalidationTracker = &DeltaCompressionBaselineInvalidationTracker;
			InitParams.Connections = &ReplicationSystemInternal.GetConnections();
			InitParams.NetRefHandleManager = &NetRefHandleManager;
			InitParams.ReplicationStateStorage = &StateStorage;
			InitParams.MaxObjectCount = MaxObjectCount;
			InitParams.MaxDeltaCompressedObjectCount = Params.MaxDeltaCompressedObjectCount;
			InitParams.ReplicationSystem = ReplicationSystem;
			DeltaCompressionBaselineManager.Init(InitParams);
		}

		FReplicationFiltering& ReplicationFiltering = ReplicationSystemInternal.GetFiltering();
		{
			FReplicationFilteringInitParams InitParams;
			InitParams.ReplicationSystem = ReplicationSystem;
			InitParams.Connections = &ReplicationSystemInternal.GetConnections();
			InitParams.NetRefHandleManager = &NetRefHandleManager;
			InitParams.Groups = &Groups;
			InitParams.BaselineInvalidationTracker = &ReplicationSystemInternal.GetDeltaCompressionBaselineInvalidationTracker();
			InitParams.MaxObjectCount = MaxObjectCount;
			InitParams.MaxGroupCount = Params.MaxNetObjectGroupCount;
			ReplicationFiltering.Init(InitParams);
		}

		InitDefaultFilteringGroups();

		FReplicationConditionals& ReplicationConditionals = ReplicationSystemInternal.GetConditionals();
		{
			FReplicationConditionalsInitParams InitParams = {};
			InitParams.NetRefHandleManager = &NetRefHandleManager;
			InitParams.ReplicationConnections = &ReplicationSystemInternal.GetConnections();
			InitParams.ReplicationFiltering = &ReplicationFiltering;
			InitParams.NetObjectGroups = &Groups;
			InitParams.BaselineInvalidationTracker = &ReplicationSystemInternal.GetDeltaCompressionBaselineInvalidationTracker();
			InitParams.MaxObjectCount = MaxObjectCount;
			InitParams.MaxConnectionCount = ReplicationSystemInternal.GetConnections().GetMaxConnectionCount();
			ReplicationConditionals.Init(InitParams);
		}

		FReplicationPrioritization& ReplicationPrioritization = ReplicationSystemInternal.GetPrioritization();
		{
			FReplicationPrioritizationInitParams InitParams;
			InitParams.ReplicationSystem = ReplicationSystem;
			InitParams.Connections = &ReplicationSystemInternal.GetConnections();
			InitParams.NetRefHandleManager = &NetRefHandleManager;
			InitParams.MaxObjectCount = MaxObjectCount;
			ReplicationPrioritization.Init(InitParams);
		}

		ReplicationSystemInternal.SetReplicationBridge(Params.ReplicationBridge);

		// Init replication bridge
		ReplicationSystemInternal.GetReplicationBridge()->Initialize(ReplicationSystem);

		ReplicationSystemInternal.GetObjectReferenceCache().Init(ReplicationSystem);

		// Init custom packagemap we use for capturing references for backwards compatible NetSerializers
		{
			UIrisObjectReferencePackageMap* ObjectReferencePackageMap = NewObject<UIrisObjectReferencePackageMap>();
			ObjectReferencePackageMap->AddToRoot();
			ReplicationSystemInternal.SetIrisObjectReferencePackageMap(ObjectReferencePackageMap);
		}

		FNetBlobManager& BlobManager = ReplicationSystemInternal.GetNetBlobManager();
		{
			FNetBlobManagerInitParams InitParams = {};
			InitParams.ReplicationSystem = ReplicationSystem;
			InitParams.bSendAttachmentsWithObject = ReplicationSystem->IsServer();
			BlobManager.Init(InitParams);
		}
	}

	void Deinit()
	{
		ReplicationSystemInternal.GetConnections().Deinit();

		// Reset replication bridge
		ReplicationSystemInternal.GetReplicationBridge()->Deinitialize();

		if (UIrisObjectReferencePackageMap* ObjectReferencePackageMap = ReplicationSystemInternal.GetIrisObjectReferencePackageMap())
		{
			ObjectReferencePackageMap->RemoveFromRoot();
			ObjectReferencePackageMap->MarkAsGarbage();
			ReplicationSystemInternal.SetIrisObjectReferencePackageMap(static_cast<UIrisObjectReferencePackageMap*>(nullptr));
		}
	}

	void UpdateDirtyObjectList()
	{
		ReplicationSystemInternal.GetDirtyNetObjectTracker().UpdateDirtyNetObjects();
	}

	void UpdateDirtyListPostPoll()
	{
		ReplicationSystemInternal.GetDirtyNetObjectTracker().UpdateAccumulatedDirtyList();
	}

	void UpdateWorldLocations()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_UpdateWorldLocations);

		ReplicationSystemInternal.GetReplicationBridge()->CallUpdateInstancesWorldLocation();
	}

	void UpdateFilterPrePoll()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_UpdateFilterPrePoll);
		LLM_SCOPE_BYTAG(Iris);

		FReplicationFiltering& Filtering = ReplicationSystemInternal.GetFiltering();
		Filtering.FilterPrePoll();
	}

	void UpdateFilterPostPoll()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_UpdateFilterPostPoll);
		LLM_SCOPE_BYTAG(Iris);

		FReplicationFiltering& Filtering = ReplicationSystemInternal.GetFiltering();
		Filtering.FilterPostPoll();

		// Iterate over all valid connections and propagate updated scopes
		FReplicationConnections& Connections = ReplicationSystemInternal.GetConnections();
		
		auto UpdateConnectionScope = [&Filtering, &Connections](uint32 ConnectionId)
		{
			FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);
			const FNetBitArrayView ObjectsInScope = Filtering.GetRelevantObjectsInScope(ConnectionId);
			Conn->ReplicationWriter->UpdateScope(ObjectsInScope);
		};

		const FNetBitArray& ValidConnections = Connections.GetValidConnections();
		ValidConnections.ForAllSetBits(UpdateConnectionScope);
	}

	// Can run at any time between scoping and replication.
	void UpdateConditionals()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_UpdateConditionals);

		FReplicationConditionals& Conditionals = ReplicationSystemInternal.GetConditionals();
		Conditionals.Update();
	}

	// Runs after filtering
	void UpdatePrioritization(const FNetBitArrayView& ReplicatingConnections)
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem::FImpl::UpdatePrioritization);
		LLM_SCOPE_BYTAG(Iris);

		const FNetBitArrayView RelevantObjects = ReplicationSystemInternal.GetNetRefHandleManager().GetRelevantObjectsInternalIndices();

		// Make a list of objects that were dirty and are also relevant
		FNetBitArray DirtyAndRelevantObjects(RelevantObjects.GetNumBits(), FNetBitArray::NoResetNoValidate);
		FNetBitArrayView DirtyAndRelevantObjectsView = MakeNetBitArrayView(DirtyAndRelevantObjects, FNetBitArray::NoResetNoValidate);

		const FNetBitArrayView AccumulatedDirtyObjects = ReplicationSystemInternal.GetDirtyNetObjectTracker().GetAccumulatedDirtyNetObjects();
		DirtyAndRelevantObjectsView.Set(RelevantObjects, FNetBitArray::AndOp, AccumulatedDirtyObjects);

		FReplicationPrioritization& Prioritization = ReplicationSystemInternal.GetPrioritization();
		Prioritization.Prioritize(ReplicatingConnections, DirtyAndRelevantObjectsView);
	}

	void PropagateDirtyChanges()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_PropagateDirtyChanges);

		FReplicationConnections& Connections = ReplicationSystemInternal.GetConnections();
		const FChangeMaskCache& UpdatedChangeMasks = ReplicationSystemInternal.GetChangeMaskCache();

		// Iterate over connections and propagate dirty changemasks
		auto UpdateDirtyChangeMasks = [&Connections, &UpdatedChangeMasks](uint32 ConnectionId)
		{
			FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);
			Conn->ReplicationWriter->UpdateDirtyChangeMasks(UpdatedChangeMasks);
		};
		const FNetBitArray& ValidConnections = Connections.GetValidConnections();
		ValidConnections.ForAllSetBits(UpdateDirtyChangeMasks);
	}

	void CopyDirtyStateData()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_CopyDirtyStateData);
		LLM_SCOPE_BYTAG(IrisState);

		FNetRefHandleManager& NetRefHandleManager = ReplicationSystemInternal.GetNetRefHandleManager();
		FChangeMaskCache& Cache = ReplicationSystemInternal.GetChangeMaskCache();

		uint32 CopiedObjectCount = 0;
		
		// Prepare cache
		constexpr uint32 ReservedIndexCount = 2048;
		constexpr uint32 ReservedStorageCount = 16536;
		Cache.PrepareCache(ReservedIndexCount, ReservedStorageCount);

		// We use this ChangeMaskWriter to capture changemasks for all copied objects
		FNetBitStreamWriter ChangeMaskWriter;

		// Setup context
		FNetSerializationContext SerializationContext;
		FInternalNetSerializationContext InternalContext(ReplicationSystem);

		SerializationContext.SetInternalContext(&InternalContext);

		// Copy the state data of objects that were dirty this frame.
		FNetBitArrayView DirtyObjectsToCopy = NetRefHandleManager.GetDirtyObjectsToCopy();

		auto CopyFunction = [&ChangeMaskWriter, &Cache, &NetRefHandleManager, &CopiedObjectCount, &SerializationContext](uint32 DirtyIndex)
		{
			CopiedObjectCount += FReplicationInstanceOperationsInternal::CopyObjectStateData(ChangeMaskWriter, Cache, NetRefHandleManager, SerializationContext, DirtyIndex);
		};

		DirtyObjectsToCopy.ForAllSetBits(CopyFunction);
		DirtyObjectsToCopy.Reset();

		const uint32 ReplicationSystemId = ReplicationSystem->GetId();
		UE_NET_TRACE_FRAME_STATSCOUNTER(ReplicationSystemId, ReplicationSystem.CopiedObjectCount, CopiedObjectCount, ENetTraceVerbosity::Trace);
	}

	void ResetObjectStateDirtiness()
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_ResetObjectStateDirtiness);

		FNetRefHandleManager& NetRefHandleManager = ReplicationSystemInternal.GetNetRefHandleManager();

		// Clean the objects that got polled this frame
		const FNetBitArrayView ObjectsToClean = NetRefHandleManager.GetPolledObjectsInternalIndices();

		// Reset object dirtyness
		ObjectsToClean.ForAllSetBits([&NetRefHandleManager](uint32 DirtyIndex)
		{
			FReplicationInstanceOperationsInternal::ResetObjectStateDirtiness(NetRefHandleManager, DirtyIndex);
		});

		// Reset cleaned objects in the tracker
		ReplicationSystemInternal.GetDirtyNetObjectTracker().ClearDirtyNetObjects(ObjectsToClean);
	}

	void ProcessNetObjectAttachmentSendQueue(FNetBlobManager::EProcessMode ProcessMode)
	{
		IRIS_PROFILER_SCOPE(FReplicationSystem_ProcessNetObjectAttachmentSendQueue);

		FNetBlobManager& NetBlobManager = ReplicationSystemInternal.GetNetBlobManager();
		NetBlobManager.ProcessNetObjectAttachmentSendQueue(ProcessMode);
	}

	void ResetNetObjectAttachmentSendQueue()
	{
		FNetBlobManager& NetBlobManager = ReplicationSystemInternal.GetNetBlobManager();
		NetBlobManager.ResetNetObjectAttachmentSendQueue();
	}

	// send data
	void SendUpdate()
	{
	}

	void AddConnection(uint32 ConnectionId)
	{
		LLM_SCOPE_BYTAG(IrisConnection);

		FReplicationConnections& Connections = ReplicationSystemInternal.GetConnections();
		{
			Connections.AddConnection(ConnectionId);

			FReplicationConnection* Connection = Connections.GetConnection(ConnectionId);

			FReplicationParameters Params;
			Params.ReplicationSystem = ReplicationSystem;
			Params.PacketSendWindowSize = 256;
			Params.ConnectionId = ConnectionId;
			Params.MaxActiveReplicatedObjectCount = ReplicationSystemInternal.GetNetRefHandleManager().GetMaxActiveObjectCount();
			/** 
			  * Currently we expect all objects to be replicated from server to client.
			  * That means we will have to support sending attachments such as RPCs from
			  * the client to the server, if the RPC is allowed to be sent in the first place.
			  */
			Params.bAllowSendingAttachmentsToObjectsNotInScope = !ReplicationSystem->IsServer();
			Params.bAllowReceivingAttachmentsFromRemoteObjectsNotInScope = !Params.bAllowSendingAttachmentsToObjectsNotInScope;
			// Delaying attachments with unresolved references on the server could cause massive queues of RPCs, potentially an OOM situation.
			Params.bAllowDelayingAttachmentsWithUnresolvedReferences = !ReplicationSystem->IsServer();

			Connection->ReplicationWriter = new FReplicationWriter();
			Connection->ReplicationReader = new FReplicationReader();

			Connection->ReplicationWriter->Init(Params);
			Connection->ReplicationReader->Init(Params);
		}

		{
			FReplicationConditionals& ReplicationConditionals = ReplicationSystemInternal.GetConditionals();
			ReplicationConditionals.AddConnection(ConnectionId);
		}

		{
			FReplicationFiltering& ReplicationFiltering = ReplicationSystemInternal.GetFiltering();
			ReplicationFiltering.AddConnection(ConnectionId);
		}

		{
			FReplicationPrioritization& ReplicationPrioritization = ReplicationSystemInternal.GetPrioritization();
			ReplicationPrioritization.AddConnection(ConnectionId);
		}

		{
			FDeltaCompressionBaselineManager& DC = ReplicationSystemInternal.GetDeltaCompressionBaselineManager();
			DC.AddConnection(ConnectionId);
		}
	}

	void RemoveConnection(uint32 ConnectionId)
	{
		{
			FDeltaCompressionBaselineManager& DC = ReplicationSystemInternal.GetDeltaCompressionBaselineManager();
			DC.RemoveConnection(ConnectionId);
		}

		{
			FReplicationPrioritization& ReplicationPrioritization = ReplicationSystemInternal.GetPrioritization();
			ReplicationPrioritization.RemoveConnection(ConnectionId);
		}

		{
			FReplicationFiltering& ReplicationFiltering = ReplicationSystemInternal.GetFiltering();
			ReplicationFiltering.RemoveConnection(ConnectionId);
		}

		{
			FReplicationConditionals& ReplicationConditionals = ReplicationSystemInternal.GetConditionals();
			ReplicationConditionals.RemoveConnection(ConnectionId);
		}

		FReplicationConnections& Connections = ReplicationSystemInternal.GetConnections();
		{
			Connections.RemoveConnection(ConnectionId);
		}
	}

	void UpdateUnresolvableReferenceTracking()
	{
		FReplicationConnections& Connections = ReplicationSystemInternal.GetConnections();
		auto UpdateUnresolvableReferenceTracking = [&Connections](uint32 ConnectionId)
		{
			FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);
			Conn->ReplicationReader->ProcessQueuedBatches();
			Conn->ReplicationReader->UpdateUnresolvableReferenceTracking();
		};
		const FNetBitArray& ValidConnections = Connections.GetValidConnections();
		ValidConnections.ForAllSetBits(UpdateUnresolvableReferenceTracking);
	}
};

}

UReplicationSystem::UReplicationSystem()
: Super()
, Impl(nullptr)
, Id(~0U)
, PIEInstanceID(INDEX_NONE)
, bIsServer(0)
, bAllowObjectReplication(0)
, bDoCollectGarbage(0)
{
}

void UReplicationSystem::Init(uint32 InId, const FReplicationSystemParams& Params)
{
	LLM_SCOPE_BYTAG(Iris);

	Id = InId;
	bIsServer = Params.bIsServer;
	bAllowObjectReplication = Params.bAllowObjectReplication;

	ReplicationBridge = Params.ReplicationBridge;

	Impl = MakePimpl<UE::Net::Private::FReplicationSystemImpl>(this, Params);
	Impl->Init(Params);

	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UReplicationSystem::PostGarbageCollection);
}

void UReplicationSystem::Shutdown()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);

	// Destroy impl
	Impl->Deinit();
	Impl.Reset();

	ReplicationBridge = nullptr;
}

UReplicationSystem::~UReplicationSystem()
{
}

UE::Net::Private::FReplicationSystemInternal* UReplicationSystem::GetReplicationSystemInternal()
{
	return &Impl->ReplicationSystemInternal;
}

const UE::Net::Private::FReplicationSystemInternal* UReplicationSystem::GetReplicationSystemInternal() const
{
	return &Impl->ReplicationSystemInternal;
}

void UReplicationSystem::PreSendUpdate(float DeltaSeconds)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(FReplicationSystem_PreSendUpdate);

	ElapsedTime += DeltaSeconds;

#if !UE_BUILD_SHIPPING
	// Force a integrity check of all replicated instances
	if (bDoCollectGarbage || ReplicationSystemCVars::bForcePruneBeforeUpdate)
	{
		CollectGarbage();
	}
#endif

	FReplicationSystemInternal& InternalSys = Impl->ReplicationSystemInternal;

	// $IRIS TODO. There may be some throttling of connections to tick that we should take into account.
	const FNetBitArrayView& ReplicatingConnections = MakeNetBitArrayView(Impl->ReplicationSystemInternal.GetConnections().GetValidConnections());

#if UE_NET_IRIS_CSV_STATS && CSV_PROFILER
	{
		FNetSendStats& SendStats = InternalSys.GetSendStats();
		SendStats.Reset();
		SendStats.SetNumberOfReplicatingConnections(ReplicatingConnections.CountSetBits());
	}
#endif

	if (bAllowObjectReplication)
	{
		UE_NET_TRACE_FRAME_STATSCOUNTER(GetId(), ReplicationSystem.ReplicatedObjectCount, InternalSys.GetNetRefHandleManager().GetActiveObjectCount(), ENetTraceVerbosity::Verbose);

		// Refresh the dirty objects we were told about.
		Impl->UpdateDirtyObjectList();

		// Update world locations. We need this to happen before both filtering and prioritization.
		Impl->UpdateWorldLocations();

		// Filters to reduce the top-level scoped object list
		Impl->UpdateFilterPrePoll();

		// Invoke any operations we need to do before copying state data
		InternalSys.GetReplicationBridge()->CallPreSendUpdate(DeltaSeconds);

		// Finalize the dirty list with objects set dirty during the poll phase
		Impl->UpdateDirtyListPostPoll();

		// Update conditionals
		Impl->UpdateConditionals();

		// Copy dirty state data. We need this to happen before both filtering and prioritization
		Impl->CopyDirtyStateData();

		// We must process all attachments to objects going out of scope before we update the scope
		Impl->ProcessNetObjectAttachmentSendQueue(FNetBlobManager::EProcessMode::ProcessObjectsGoingOutOfScope);

		// Update filtering and scope for all connections
		Impl->UpdateFilterPostPoll();

		// Propagate dirty changes to all connections
		Impl->PropagateDirtyChanges();
	}

	// Forward attachments to the connections after scope update
	Impl->ProcessNetObjectAttachmentSendQueue(FNetBlobManager::EProcessMode::ProcessObjectsInScope);
	Impl->ResetNetObjectAttachmentSendQueue();

	if (bAllowObjectReplication)
	{
		// Update object priorities
		Impl->UpdatePrioritization(ReplicatingConnections);

		// Delta compression preparations before send
		{
			FDeltaCompressionBaselineManagerPreSendUpdateParams UpdateParams;
			UpdateParams.ChangeMaskCache = &InternalSys.GetChangeMaskCache();
			InternalSys.GetDeltaCompressionBaselineManager().PreSendUpdate(UpdateParams);
		}
	}

	// Destroy objects pending destroy
	{
		Impl->UpdateUnresolvableReferenceTracking();
		InternalSys.GetNetRefHandleManager().DestroyObjectsPendingDestroy();
	}
}

void UReplicationSystem::SendUpdate()
{
	Impl->SendUpdate();
}

void UReplicationSystem::PostSendUpdate()
{
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(FReplicationSystem_PostSendUpdate);
	
	FReplicationSystemInternal& InternalSys = Impl->ReplicationSystemInternal;

	Impl->ResetObjectStateDirtiness();

	InternalSys.GetChangeMaskCache().ResetCache();

	// Store the state of the previous frames scopable objects
	InternalSys.GetNetRefHandleManager().SetPrevFrameScopableInternalIndicesToCurrent();

	// Update handles pending tear-off
	InternalSys.GetReplicationBridge()->UpdateHandlesPendingTearOff();

	// Reset baseline invalidation
	InternalSys.GetDeltaCompressionBaselineInvalidationTracker().PostSendUpdate();

	if (bAllowObjectReplication)
	{
		FDeltaCompressionBaselineManagerPostSendUpdateParams UpdateParams;
		InternalSys.GetDeltaCompressionBaselineManager().PostSendUpdate(UpdateParams);
	}

#if UE_NET_IRIS_CSV_STATS && CSV_PROFILER
	{
		UE::Net::FNetSendStats& SendStats = InternalSys.GetSendStats();
		SendStats.ReportCsvStats();
	}
#endif
}

void UReplicationSystem::PostGarbageCollection()
{
	bDoCollectGarbage = 1U;
}

void UReplicationSystem::CollectGarbage()
{
	IRIS_PROFILER_SCOPE(ReplicationSystem_CollectGarbage);

	// Prune stale object instances before descriptors and protocols are pruned
	Impl->ReplicationSystemInternal.GetReplicationBridge()->CallPruneStaleObjects();
	Impl->ReplicationSystemInternal.GetReplicationStateDescriptorRegistry().PruneStaleDescriptors();

	bDoCollectGarbage = 0;
}

void UReplicationSystem::ResetGameWorldState()
{
	Impl->ReplicationSystemInternal.GetReplicationBridge()->RemoveDestructionInfosForGroup(UE::Net::InvalidNetObjectGroupHandle);
}

void UReplicationSystem::NotifyStreamingLevelUnload(const UObject* Level)
{
	Impl->ReplicationSystemInternal.GetReplicationBridge()->NotifyStreamingLevelUnload(Level);
}

void UReplicationSystem::AddConnection(uint32 ConnectionId)
{
	Impl->AddConnection(ConnectionId);
}

void UReplicationSystem::RemoveConnection(uint32 ConnectionId)
{
	Impl->RemoveConnection(ConnectionId);
}

bool UReplicationSystem::IsValidConnection(uint32 ConnectionId) const
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	return Connections.GetConnection(ConnectionId) != nullptr;
}

void UReplicationSystem::SetReplicationEnabledForConnection(uint32 ConnectionId, bool bReplicationEnabled)
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	UE::Net::Private::FReplicationConnection* Connection = Connections.GetConnection(ConnectionId);

	check(Connection);

	Connection->ReplicationWriter->SetReplicationEnabled(bReplicationEnabled);
}

void UReplicationSystem::SetReplicationView(uint32 ConnectionId, const UE::Net::FReplicationView& View)
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	Connections.SetReplicationView(ConnectionId, View);
}

void UReplicationSystem::SetStaticPriority(FNetRefHandle Handle, float Priority)
{
	const UE::Net::Private::FInternalNetRefIndex ObjectInternalIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle);
	if (ObjectInternalIndex == UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	return Impl->ReplicationSystemInternal.GetPrioritization().SetStaticPriority(ObjectInternalIndex, Priority);
}

bool UReplicationSystem::SetPrioritizer(FNetRefHandle Handle, FNetObjectPrioritizerHandle Prioritizer)
{
	using namespace UE::Net::Private;

	if (!Handle.IsValid())
	{
		return false;
	}

	FReplicationSystemInternal& ReplicationSystemInternal = Impl->ReplicationSystemInternal;
	const FInternalNetRefIndex ObjectInternalIndex = ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	return ReplicationSystemInternal.GetPrioritization().SetPrioritizer(ObjectInternalIndex, Prioritizer);
}

FNetObjectPrioritizerHandle UReplicationSystem::GetPrioritizerHandle(const FName PrioritizerName) const
{
	return Impl->ReplicationSystemInternal.GetPrioritization().GetPrioritizerHandle(PrioritizerName);
}

UNetObjectPrioritizer* UReplicationSystem::GetPrioritizer(const FName PrioritizerName) const
{
	return Impl->ReplicationSystemInternal.GetPrioritization().GetPrioritizer(PrioritizerName);
}

const UE::Net::FStringTokenStore* UReplicationSystem::GetStringTokenStore() const
{
	return &Impl->ReplicationSystemInternal.GetStringTokenStore();
}

UE::Net::FStringTokenStore* UReplicationSystem::GetStringTokenStore()
{
	return &Impl->ReplicationSystemInternal.GetStringTokenStore();
}

bool UReplicationSystem::RegisterNetBlobHandler(UNetBlobHandler* Handler)
{
	UE::Net::Private::FNetBlobManager& NetBlobManager = Impl->ReplicationSystemInternal.GetNetBlobManager();
	return NetBlobManager.RegisterNetBlobHandler(Handler);
}

bool UReplicationSystem::QueueNetObjectAttachment(uint32 ConnectionId, const UE::Net::FNetObjectReference& TargetRef, const TRefCountPtr<UE::Net::FNetObjectAttachment>& Attachment)
{
	UE::Net::Private::FNetBlobManager& NetBlobManager = Impl->ReplicationSystemInternal.GetNetBlobManager();
	return NetBlobManager.QueueNetObjectAttachment(ConnectionId, TargetRef, Attachment);
}

bool UReplicationSystem::SendRPC(const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters)
{
	UE::Net::Private::FNetBlobManager& NetBlobManager = Impl->ReplicationSystemInternal.GetNetBlobManager();
	return NetBlobManager.SendRPC(Object, SubObject, Function, Parameters);
}

bool UReplicationSystem::SendRPC(uint32 ConnectionId, const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters)
{
	UE::Net::Private::FNetBlobManager& NetBlobManager = Impl->ReplicationSystemInternal.GetNetBlobManager();
	return NetBlobManager.SendRPC(ConnectionId, Object, SubObject, Function, Parameters);
}

void UReplicationSystem::InitDataStreams(uint32 ConnectionId, UDataStreamManager* DataStreamManager)
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	Connections.InitDataStreams(GetId(), ConnectionId, DataStreamManager);
}

void UReplicationSystem::SetConnectionUserData(uint32 ConnectionId, UObject* InUserData)
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	UE::Net::Private::FReplicationConnection* Connection = Connections.GetConnection(ConnectionId);
	
	check(Connection);

	Connection->UserData = InUserData;
}

UObject* UReplicationSystem::GetConnectionUserData(uint32 ConnectionId) const
{
	UE::Net::Private::FReplicationConnections& Connections = Impl->ReplicationSystemInternal.GetConnections();
	if (!ensureMsgf(Connections.IsValidConnection(ConnectionId), TEXT("Invalid ConnectionId %u passed to UReplicationSystem::GetConnectionUserData."), ConnectionId))
	{
		return nullptr;
	}

	if (UE::Net::Private::FReplicationConnection* Connection = Connections.GetConnection(ConnectionId))
	{
		return Connection->UserData.Get();
	}
	return nullptr;
}

UReplicationBridge* UReplicationSystem::GetReplicationBridge() const
{
	return Impl->ReplicationSystemInternal.GetReplicationBridge();
}

bool UReplicationSystem::IsValidHandle(FNetRefHandle Handle) const
{
	return Handle.IsValid() && Impl->ReplicationSystemInternal.GetNetRefHandleManager().IsValidNetRefHandle(Handle);
}

const UE::Net::FReplicationProtocol* UReplicationSystem::GetReplicationProtocol(FNetRefHandle Handle) const
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();

	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return nullptr;
	}

	return NetRefHandleManager.GetReplicatedObjectDataNoCheck(ObjectInternalIndex).Protocol;
}

void UReplicationSystem::SetOwningNetConnection(FNetRefHandle Handle, uint32 ConnectionId)
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();
	Filtering.SetOwningConnection(ObjectInternalIndex, ConnectionId);
}

uint32 UReplicationSystem::GetOwningNetConnection(FNetRefHandle Handle) const
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return UE::Net::InvalidConnectionId;
	}

	const FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();
	return Filtering.GetOwningConnection(ObjectInternalIndex);
}

bool UReplicationSystem::SetFilter(FNetRefHandle Handle, UE::Net::FNetObjectFilterHandle Filter)
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();
	return Filtering.SetFilter(ObjectInternalIndex, Filter);
}

UE::Net::FNetObjectFilterHandle UReplicationSystem::GetFilterHandle(const FName FilterName) const
{
	return Impl->ReplicationSystemInternal.GetFiltering().GetFilterHandle(FilterName);
}

UNetObjectFilter* UReplicationSystem::GetFilter(const FName FilterName) const
{
	return Impl->ReplicationSystemInternal.GetFiltering().GetFilter(FilterName);
}

FName UReplicationSystem::GetFilterName(UE::Net::FNetObjectFilterHandle Filter) const
{
	return Impl->ReplicationSystemInternal.GetFiltering().GetFilterName(Filter);
}

bool UReplicationSystem::SetConnectionFilter(FNetRefHandle Handle, const TBitArray<>& Connections, UE::Net::ENetFilterStatus ReplicationStatus)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();
	return Filtering.SetConnectionFilter(ObjectInternalIndex, MakeNetBitArrayView(Connections.GetData(), Connections.Num()), ReplicationStatus);
}

UE::Net::FNetObjectGroupHandle UReplicationSystem::GetOrCreateSubObjectFilter(FName GroupName)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();

	FNetObjectGroupHandle GroupHandle = Groups.GetNamedGroupHandle(GroupName);
	if (GroupHandle != InvalidNetObjectGroupHandle)
	{
		check(Filtering.IsSubObjectFilterGroup(GroupHandle));
		return GroupHandle;
	}

	GroupHandle = Groups.CreateNamedGroup(GroupName);
	if (GroupHandle != InvalidNetObjectGroupHandle)
	{
		Filtering.AddSubObjectFilter(GroupHandle);
	}
	return GroupHandle;
}

UE::Net::FNetObjectGroupHandle UReplicationSystem::GetSubObjectFilterGroupHandle(FName GroupName) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();

	FNetObjectGroupHandle GroupHandle = Groups.GetNamedGroupHandle(GroupName);
	if (GroupHandle != InvalidNetObjectGroupHandle)
	{
		if (ensureAlwaysMsgf(Filtering.IsSubObjectFilterGroup(GroupHandle), TEXT("UReplicationSystem::GetSubObjectFilterGroupHandle Trying to lookup NetObjectGroupHandle for NetGroup %s that is not a subobject filter"), *GroupName.ToString()))
		{
			return GroupHandle;
		}
	}
	return InvalidNetObjectGroupHandle;
}

void UReplicationSystem::SetSubObjectFilterStatus(FName GroupName, uint32 ConnectionId, UE::Net::ENetFilterStatus ReplicationStatus)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (UE::Net::IsSpecialNetConditionGroup(GroupName))
	{
		ensureAlwaysMsgf(false, TEXT("UReplicationSystem::SetSubObjectFilterStatus Cannot SetSubObjectFilterStatus for special NetGroup %s"), *GroupName.ToString());
		return;
	}

	FNetObjectGroupHandle GroupHandle = GetSubObjectFilterGroupHandle(GroupName);
	if (GroupHandle != InvalidNetObjectGroupHandle)
	{
		FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();
		Filtering.SetSubObjectFilterStatus(GroupHandle, ConnectionId, ReplicationStatus);
	}
}

void UReplicationSystem::RemoveSubObjectFilter(FName GroupName)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();

	FNetObjectGroupHandle GroupHandle = GetSubObjectFilterGroupHandle(GroupName);
	if (GroupHandle != InvalidNetObjectGroupHandle)
	{
		Filtering.RemoveSubObjectFilter(GroupHandle);
		Groups.DestroyGroup(GroupHandle);
	}
}

UE::Net::FNetObjectGroupHandle UReplicationSystem::CreateGroup()
{
	LLM_SCOPE_BYTAG(Iris);

	return Impl->ReplicationSystemInternal.GetGroups().CreateGroup();
}

void UReplicationSystem::AddToGroup(FNetObjectGroupHandle GroupHandle, FNetRefHandle Handle)
{
	LLM_SCOPE_BYTAG(Iris);

	using namespace UE::Net::Private;

	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	

	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);

	if (GroupHandle && ObjectInternalIndex)
	{
		Groups.AddToGroup(GroupHandle, ObjectInternalIndex);
		Filtering.NotifyObjectAddedToGroup(GroupHandle, ObjectInternalIndex);
	}
}

void UReplicationSystem::RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FNetRefHandle Handle)
{	
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();

	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (GroupHandle && ObjectInternalIndex)
	{
		FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
		FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	

		Groups.RemoveFromGroup(GroupHandle, ObjectInternalIndex);
		Filtering.NotifyObjectRemovedFromGroup(GroupHandle, ObjectInternalIndex);
	}
}

void UReplicationSystem::RemoveFromAllGroups(FNetRefHandle Handle)
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();

	uint32 NumGroupMemberShips;
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (const FNetObjectGroupHandle* GroupHandles = Groups.GetGroupMemberships(ObjectInternalIndex, NumGroupMemberShips))
	{
		// We copy the membership array as it is modified during removal
		TArray<FNetObjectGroupHandle> CopiedGroupHandles(MakeArrayView(GroupHandles, NumGroupMemberShips));
		for (FNetObjectGroupHandle GroupHandle : MakeArrayView(CopiedGroupHandles.GetData(), CopiedGroupHandles.Num()))
		{
			Groups.RemoveFromGroup(GroupHandle, ObjectInternalIndex);
			Filtering.NotifyObjectRemovedFromGroup(GroupHandle, ObjectInternalIndex);
		}
	}	
}

bool UReplicationSystem::IsInGroup(FNetObjectGroupHandle GroupHandle, FNetRefHandle Handle) const
{
	using namespace UE::Net::Private;

	const FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();
	const FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();

	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);

	return Groups.Contains(GroupHandle, ObjectInternalIndex);
}

bool UReplicationSystem::IsValidGroup(FNetObjectGroupHandle GroupHandle) const
{
	const UE::Net::Private::FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();

	return GroupHandle && Groups.IsValidGroup(GroupHandle);
}

void UReplicationSystem::DestroyGroup(FNetObjectGroupHandle GroupHandle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	// We do not allow client code to remove reserved groups
	if (IsReservedNetObjectGroupHandle(GroupHandle))
	{
		check(false);
		return;
	}

	FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	FNetObjectGroups& Groups = Impl->ReplicationSystemInternal.GetGroups();

	Filtering.RemoveGroupFilter(GroupHandle);
	Filtering.RemoveSubObjectFilter(GroupHandle);

	Groups.DestroyGroup(GroupHandle);
}

void UReplicationSystem::AddGroupFilter(FNetObjectGroupHandle GroupHandle)
{
	UE::Net::Private::FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	Filtering.AddGroupFilter(GroupHandle);
}

void UReplicationSystem::RemoveGroupFilter(FNetObjectGroupHandle GroupHandle)
{
	UE::Net::Private::FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	Filtering.RemoveGroupFilter(GroupHandle);
}

void UReplicationSystem::SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, UE::Net::ENetFilterStatus ReplicationStatus)
{
	UE::Net::Private::FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	Filtering.SetGroupFilterStatus(GroupHandle, ConnectionId, ReplicationStatus);
}

void UReplicationSystem::SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, const UE::Net::FNetBitArray& Connections, UE::Net::ENetFilterStatus ReplicationStatus)
{
	UE::Net::Private::FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	Filtering.SetGroupFilterStatus(GroupHandle, UE::Net::MakeNetBitArrayView(Connections), ReplicationStatus);
}

void UReplicationSystem::SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, UE::Net::ENetFilterStatus ReplicationStatus)
{
	UE::Net::Private::FReplicationFiltering& Filtering = Impl->ReplicationSystemInternal.GetFiltering();	
	Filtering.SetGroupFilterStatus(GroupHandle, ReplicationStatus);
}

bool UReplicationSystem::SetReplicationConditionConnectionFilter(FNetRefHandle Handle, UE::Net::EReplicationCondition Condition, uint32 ConnectionId, bool bEnable)
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	FReplicationConditionals& Conditionals = Impl->ReplicationSystemInternal.GetConditionals();
	return Conditionals.SetConditionConnectionFilter(ObjectInternalIndex, Condition, ConnectionId, bEnable);
}

bool UReplicationSystem::SetReplicationCondition(FNetRefHandle Handle, UE::Net::EReplicationCondition Condition, bool bEnable)
{
	using namespace UE::Net::Private;

	FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	FReplicationConditionals& Conditionals = Impl->ReplicationSystemInternal.GetConditionals();
	return Conditionals.SetCondition(ObjectInternalIndex, Condition, bEnable);
}

void UReplicationSystem::SetDeltaCompressionStatus(FNetRefHandle Handle, UE::Net::ENetObjectDeltaCompressionStatus Status)
{
	using namespace UE::Net::Private;

	FReplicationSystemInternal& InternalSys = Impl->ReplicationSystemInternal;
	FNetRefHandleManager& NetRefHandleManager = InternalSys.GetNetRefHandleManager();
	const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager.GetInternalIndex(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	FDeltaCompressionBaselineManager& DC = InternalSys.GetDeltaCompressionBaselineManager();
	return DC.SetDeltaCompressionStatus(ObjectInternalIndex, Status);
}

void UReplicationSystem::SetIsNetTemporary(FNetRefHandle Handle)
{
	UE::Net::Private::FNetRefHandleManager& NetRefHandleManager = Impl->ReplicationSystemInternal.GetNetRefHandleManager();
	if (ensure(NetRefHandleManager.IsLocalNetRefHandle(Handle)))
	{
		// Set the object to not propagate changed states
		NetRefHandleManager.SetShouldPropagateChangedStates(Handle, false);
	}
}

void UReplicationSystem::TearOffNextUpdate(FNetRefHandle Handle)
{
	constexpr EEndReplicationFlags DestroyFlags = EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId;
	Impl->ReplicationSystemInternal.GetReplicationBridge()->TearOff(Handle, DestroyFlags, false);
}

void UReplicationSystem::ForceNetUpdate(FNetRefHandle Handle)
{
	if (const uint32 InternalObjectIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle))
	{
		UE::Net::Private::ForceNetUpdate(GetId(), InternalObjectIndex);
	}
}

void UReplicationSystem::MarkDirty(FNetRefHandle Handle)
{
	if (const uint32 InternalObjectIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle))
	{
		 UE::Net::Private::MarkNetObjectStateDirty(GetId(), InternalObjectIndex);
	}
}

uint32 UReplicationSystem::GetMaxConnectionCount() const
{
	return Impl->ReplicationSystemInternal.GetConnections().GetMaxConnectionCount();
}

void UReplicationSystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UReplicationSystem* This = CastChecked<UReplicationSystem>(InThis);
	if (This->Impl.IsValid())
	{
		This->Impl->ReplicationSystemInternal.GetNetRefHandleManager().AddReferencedObjects(Collector);
		This->Impl->ReplicationSystemInternal.GetObjectReferenceCache().AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects(InThis, Collector);
}

const UE::Net::FNetCullDistanceOverrides& UReplicationSystem::GetNetCullDistanceOverrides() const
{
	return Impl->ReplicationSystemInternal.GetNetCullDistanceOverrides();
}

const UE::Net::FWorldLocations& UReplicationSystem::GetWorldLocations() const
{
	return Impl->ReplicationSystemInternal.GetWorldLocations();
}

void UReplicationSystem::SetCullDistanceSqrOverride(FNetRefHandle Handle, float DistSqr)
{
	const UE::Net::Private::FInternalNetRefIndex ObjectInternalIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle);
	if (ObjectInternalIndex == UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	return Impl->ReplicationSystemInternal.GetNetCullDistanceOverrides().SetCullDistanceSqr(ObjectInternalIndex, DistSqr);
}

void UReplicationSystem::ClearCullDistanceSqrOverride(FNetRefHandle Handle)
{
	const UE::Net::Private::FInternalNetRefIndex ObjectInternalIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle);
	if (ObjectInternalIndex == UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	Impl->ReplicationSystemInternal.GetNetCullDistanceOverrides().ClearCullDistanceSqr(ObjectInternalIndex);
}

float UReplicationSystem::GetCullDistanceSqrOverride(FNetRefHandle Handle, float DefaultValue) const
{
	const UE::Net::Private::FInternalNetRefIndex ObjectInternalIndex = Impl->ReplicationSystemInternal.GetNetRefHandleManager().GetInternalIndex(Handle);
	if (ObjectInternalIndex == UE::Net::Private::FNetRefHandleManager::InvalidInternalIndex)
	{
		return DefaultValue;
	}

	return Impl->ReplicationSystemInternal.GetNetCullDistanceOverrides().GetCullDistanceSqr(ObjectInternalIndex, DefaultValue);
}

namespace UE::Net
{

FReplicationSystemCreatedDelegate& FReplicationSystemFactory::GetReplicationSystemCreatedDelegate()
{
	static FReplicationSystemCreatedDelegate Delegate;
	return Delegate;
}

FReplicationSystemDestroyedDelegate& FReplicationSystemFactory::GetReplicationSystemDestroyedDelegate()
{
	static FReplicationSystemDestroyedDelegate Delegate;
	return Delegate;
}

// FReplicationSystemFactory
UReplicationSystem* FReplicationSystemFactory::ReplicationSystems[MaxReplicationSystemCount];
uint32 FReplicationSystemFactory::MaxReplicationSystemId = 0;

UReplicationSystem* FReplicationSystemFactory::CreateReplicationSystem(const UReplicationSystem::FReplicationSystemParams& Params)
{
	if (!Params.ReplicationBridge)
	{
		UE_LOG(LogIris, Error, TEXT("Cannot create ReplicationSystem without a ReplicationBridge"));
		return nullptr;
	}

	for (uint32 It = 0, EndIt = MaxReplicationSystemCount; It != EndIt; ++It)
	{
		if (ReplicationSystems[It] != nullptr)
		{
			continue;
		}

		UReplicationSystem* ReplicationSystem = NewObject<UReplicationSystem>();
		ReplicationSystems[It] = ReplicationSystem;
		ReplicationSystem->AddToRoot();

		const uint32 ReplicationSystemId = It;
		if (ReplicationSystemId > MaxReplicationSystemId)
		{
			MaxReplicationSystemId = ReplicationSystemId;
		}

		ReplicationSystem->Init(ReplicationSystemId, Params);

		if (GetReplicationSystemCreatedDelegate().IsBound())
		{
			GetReplicationSystemCreatedDelegate().Broadcast(ReplicationSystem);
		}

		return ReplicationSystem;
	}

	LowLevelFatalError(TEXT("Too many ReplicationSystems have already been created (%u)"), MaxReplicationSystemCount);
	return nullptr;
}

void FReplicationSystemFactory::DestroyReplicationSystem(UReplicationSystem* System)
{
	if (System == nullptr)
	{
		return;
	}

	const uint32 Id = System->GetId();
	if (Id < MaxReplicationSystemCount)
	{
		ReplicationSystems[Id] = nullptr;

		uint32 NewMaxReplicationSystemId = 0;
		for (uint32 It = 0, EndIt = MaxReplicationSystemCount; It != EndIt; ++It)
		{
			if (ReplicationSystems[It] != nullptr)
			{
				NewMaxReplicationSystemId = Id;
			}
		}
		MaxReplicationSystemId = NewMaxReplicationSystemId;
	}

	if (GetReplicationSystemDestroyedDelegate().IsBound())
	{
		GetReplicationSystemDestroyedDelegate().Broadcast(System);
	}

	System->Shutdown();
	System->RemoveFromRoot();
	System->MarkAsGarbage();
}

TArrayView<UReplicationSystem*> FReplicationSystemFactory::GetAllReplicationSystems()
{
	return MakeArrayView(ReplicationSystems, MaxReplicationSystemId + 1);
}

}
