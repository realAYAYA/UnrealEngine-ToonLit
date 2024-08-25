// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "Containers/ArrayView.h"
#include "Iris/IrisConfigInternal.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Net/Core/NetBitArray.h"
#include "Net/Core/NetHandle/NetHandleManager.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Trace/NetDebugName.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/ReplicationSystem/Prioritization/NetObjectPrioritizer.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/ReplicationFragmentInternal.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/ReplicationSystem/WorldLocations.h"
#include "ReplicationOperationsInternal.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/ReplicationSystem/ChangeMaskCache.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "ReplicationFragmentInternal.h"

DEFINE_LOG_CATEGORY(LogIrisBridge)

#define UE_LOG_REPLICATIONBRIDGE(Category, Format, ...)  UE_LOG(LogIrisBridge, Category, TEXT("ReplicationBridge(%u)::") Format, ReplicationSystem->GetId(), ##__VA_ARGS__)

/**
 * ReplicationBridge Implementation
 */
UReplicationBridge::UReplicationBridge()
: ReplicationSystem(nullptr)
, ReplicationProtocolManager(nullptr)
, ReplicationStateDescriptorRegistry(nullptr)
, NetRefHandleManager(nullptr)
, DestructionInfoProtocol(nullptr)
{
}

bool UReplicationBridge::WriteNetRefHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetRefHandle Handle)
{
	return true;
}

FReplicationBridgeCreateNetRefHandleResult UReplicationBridge::CreateNetRefHandleFromRemote(FNetRefHandle RootObjectOfSubObject, FNetRefHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context)
{
	return FReplicationBridgeCreateNetRefHandleResult();
};

void UReplicationBridge::PreSendUpdateSingleHandle(FNetRefHandle Handle)
{
}

void UReplicationBridge::PreSendUpdate()
{
}

void UReplicationBridge::OnStartPreSendUpdate()
{
}

void UReplicationBridge::OnPostSendUpdate()
{
}

void UReplicationBridge::UpdateInstancesWorldLocation()
{
}

void UReplicationBridge::SubObjectCreatedFromReplication(FNetRefHandle SubObjectRefHandle)
{
}

void UReplicationBridge::PostApplyInitialState(FNetRefHandle Handle)
{
}

void UReplicationBridge::DetachInstanceFromRemote(FNetRefHandle Handle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
}

void UReplicationBridge::DetachInstance(FNetRefHandle Handle)
{
}

void UReplicationBridge::PruneStaleObjects()
{
}

void UReplicationBridge::SetNetDriver(UNetDriver* NetDriver)
{
}

void UReplicationBridge::GetInitialDependencies(FNetRefHandle Handle, FNetDependencyInfoArray& OutDependencies) const
{
	return;
}

void UReplicationBridge::CallGetInitialDependencies(FNetRefHandle Handle, FNetDependencyInfoArray& OutDependencies) const
{
	using namespace UE::Net::Private;

	// if the Handle is static, the initial dependency is the handle itself
	if (Handle.IsStatic())
	{
		OutDependencies.Emplace(FObjectReferenceCache::MakeNetObjectReference(Handle));
		return;
	}
	else
	{
		GetInitialDependencies(Handle, OutDependencies);
	}
}

void UReplicationBridge::DetachSubObjectInstancesFromRemote(FNetRefHandle OwnerHandle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	using namespace UE::Net::Private;

	FInternalNetRefIndex OwnerInternalIndex = NetRefHandleManager->GetInternalIndex(OwnerHandle);
	if (OwnerInternalIndex)
	{
		for (const FInternalNetRefIndex SubObjectInternalIndex : NetRefHandleManager->GetChildSubObjects(OwnerInternalIndex))
		{
			FNetRefHandleManager::FReplicatedObjectData& SubObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(SubObjectInternalIndex);
			const FNetRefHandle SubObjectHandle = SubObjectData.RefHandle;
			SubObjectData.bTearOff = (DestroyReason == EReplicationBridgeDestroyInstanceReason::TearOff);
			SubObjectData.bPendingEndReplication = 1U;

			EReplicationBridgeDestroyInstanceFlags SubObjectDestroyFlags = DestroyFlags;
			// The subobject is allowed to be destroyed if both the owner and the subobject allows it.
			if (!SubObjectData.bAllowDestroyInstanceFromRemote)
			{
				EnumRemoveFlags(SubObjectDestroyFlags, EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote);
			}
			CallDetachInstanceFromRemote(SubObjectHandle, DestroyReason, SubObjectDestroyFlags);
		}
	}
}

void UReplicationBridge::DestroyNetObjectFromRemote(FNetRefHandle Handle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	using namespace UE::Net::Private;

	if (Handle.IsValid())
	{
		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("DestroyNetObjectFromRemote for %s | DestroyReason: %s | DestroyFlags: %s "), *NetRefHandleManager->PrintObjectFromNetRefHandle(Handle), LexToString(DestroyReason), LexToString(DestroyFlags) );

		FInternalNetRefIndex OwnerInternalIndex = NetRefHandleManager->GetInternalIndex(Handle);
		FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(OwnerInternalIndex);
		ObjectData.bTearOff = (DestroyReason == EReplicationBridgeDestroyInstanceReason::TearOff);
		ObjectData.bPendingEndReplication = 1U;

		// if a subobject owner is to be destroyed we want to detach all subobjects before doing so to ensure we execute expected callbacks
		// We keep tracking them internally
		DetachSubObjectInstancesFromRemote(Handle, DestroyReason, DestroyFlags);
		
		// Allow derived bridges to cleanup any instance info they have stored
		CallDetachInstanceFromRemote(Handle, DestroyReason, DestroyFlags);

		// Detach instance protocol
		InternalDetachInstanceFromNetRefHandle(Handle);
	
		// Destroy the NetRefHandle
		InternalDestroyNetObject(Handle);
	}
}

void UReplicationBridge::CallPreSendUpdate(float DeltaSeconds)
{
	// Tear-off all handles pending tear-off
	TearOffHandlesPendingTearOff();

	PreSendUpdate();
}

void UReplicationBridge::CallPreSendUpdateSingleHandle(FNetRefHandle Handle)
{
	PreSendUpdateSingleHandle(Handle);
}

void UReplicationBridge::CallUpdateInstancesWorldLocation()
{
	UpdateInstancesWorldLocation();
}

void UReplicationBridge::CallDetachInstance(FNetRefHandle Handle)
{
	DetachInstance(Handle);
}

void UReplicationBridge::CallDetachInstanceFromRemote(FNetRefHandle Handle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	DetachInstanceFromRemote(Handle, DestroyReason, DestroyFlags);
}

void UReplicationBridge::CallPruneStaleObjects()
{
	PruneStaleObjects();
}

bool UReplicationBridge::CallWriteNetRefHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetRefHandle Handle)
{
	using namespace UE::Net;

	check(!Context.bIsDestructionInfo);

	return WriteNetRefHandleCreationInfo(Context, Handle);
}

bool UReplicationBridge::CallWriteNetRefHandleDestructionInfo(FReplicationBridgeSerializationContext& Context, FNetRefHandle Handle)
{
	using namespace UE::Net;

	check(Context.bIsDestructionInfo);
	UE_NET_TRACE_SCOPE(DestructionInfo, *Context.SerializationContext.GetBitStreamWriter(), Context.SerializationContext.GetTraceCollector(), ENetTraceVerbosity::Trace);

	const FDestructionInfo* Info = StaticObjectsPendingDestroy.Find(Handle);
	if (ensure(Info))
	{
		// Write destruction info
		WriteFullNetObjectReference(Context.SerializationContext, Info->StaticRef);
	}
	else
	{
		UE_LOG_REPLICATIONBRIDGE(Error, TEXT("Failed to write destructionInfo for %s"), *Handle.ToString());
		// Write invalid reference
		WriteFullNetObjectReference(Context.SerializationContext, FNetObjectReference());
	}

	return !Context.SerializationContext.HasErrorOrOverflow();
}

FReplicationBridgeCreateNetRefHandleResult UReplicationBridge::CallCreateNetRefHandleFromRemote(FNetRefHandle RootObjectOfSubObject, FNetRefHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context)
{
	check(!Context.bIsDestructionInfo);

	FReplicationBridgeCreateNetRefHandleResult CreateResult = CreateNetRefHandleFromRemote(RootObjectOfSubObject, WantedNetHandle, Context);

	// Track subobjects on clients
	if (CreateResult.NetRefHandle.IsValid() && RootObjectOfSubObject.IsValid())
	{
		NetRefHandleManager->AddSubObject(RootObjectOfSubObject, CreateResult.NetRefHandle);
	}

	return CreateResult;
}

bool UReplicationBridge::IsAllowedToDestroyInstance(const UObject* Instance) const
{
	return true;
}

void UReplicationBridge::ReadAndExecuteDestructionInfoFromRemote(FReplicationBridgeSerializationContext& Context)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	check(Context.bIsDestructionInfo);

	UE_NET_TRACE_SCOPE(DestructionInfo, *Context.SerializationContext.GetBitStreamReader(), Context.SerializationContext.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Destroy instance here, or defer to later?
	FNetObjectReference ReferenceToDestroy;
	ReadFullNetObjectReference(Context.SerializationContext, ReferenceToDestroy);

	// Destroy the reference
	// Resolve the reference in order to be able to destroy it
	if (const UObject* Instance = ObjectReferenceCache->ResolveObjectReference(ReferenceToDestroy, Context.SerializationContext.GetInternalContext()->ResolveContext))
	{
		const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(ReferenceToDestroy.GetRefHandle());
		if (InternalReplicationIndex != FNetRefHandleManager::InvalidInternalIndex)
		{
			NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).bPendingEndReplication = 1U;
		}

		constexpr EReplicationBridgeDestroyInstanceReason DestroyReason = EReplicationBridgeDestroyInstanceReason::Destroy;
		const EReplicationBridgeDestroyInstanceFlags DestroyFlags = IsAllowedToDestroyInstance(Instance) ? EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote : EReplicationBridgeDestroyInstanceFlags::None;

		// if a subobject owner is to be destroyed we want to detach all subobjects before doing so to ensure we execute expected callbacks
		// We keep tracking them internally
		DetachSubObjectInstancesFromRemote(ReferenceToDestroy.GetRefHandle(), DestroyReason, DestroyFlags);

		CallDetachInstanceFromRemote(ReferenceToDestroy.GetRefHandle(), DestroyReason, DestroyFlags);
	}
}

void UReplicationBridge::CallSubObjectCreatedFromReplication(FNetRefHandle SubObjectHandle)
{
	SubObjectCreatedFromReplication(SubObjectHandle);
}

void UReplicationBridge::CallPostApplyInitialState(FNetRefHandle Handle)
{
	PostApplyInitialState(Handle);
}

UReplicationBridge::~UReplicationBridge()
{
}

void UReplicationBridge::Initialize(UReplicationSystem* InReplicationSystem)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	Private::FReplicationSystemInternal* ReplicationSystemInternal = InReplicationSystem->GetReplicationSystemInternal();

	ReplicationSystem = InReplicationSystem;
	ReplicationProtocolManager = &ReplicationSystemInternal->GetReplicationProtocolManager();
	ReplicationStateDescriptorRegistry = &ReplicationSystemInternal->GetReplicationStateDescriptorRegistry();
	NetRefHandleManager = &ReplicationSystemInternal->GetNetRefHandleManager();
	ObjectReferenceCache = &ReplicationSystemInternal->GetObjectReferenceCache();
	Groups = &ReplicationSystemInternal->GetGroups();

	// Create destruction info protocol
	{
		const FReplicationFragments RegisteredFragments;
		FCreateReplicationProtocolParameters CreateProtocolParams {.bValidateProtocolId = false, .TypeStatsIndex =  GetReplicationSystem()->GetReplicationSystemInternal()->GetNetTypeStats().GetOrCreateTypeStats(FName("DestructionInfo"))};
		DestructionInfoProtocol = ReplicationProtocolManager->CreateReplicationProtocol(FReplicationProtocolManager::CalculateProtocolIdentifier(RegisteredFragments), RegisteredFragments, TEXT("InternalDestructionInfo"), CreateProtocolParams);
	}
}

void UReplicationBridge::Deinitialize()
{
	// Just set the protocol to null, it will be destroyed with the protocolmanager
	DestructionInfoProtocol = nullptr;

	ReplicationSystem = nullptr;
	ReplicationProtocolManager = nullptr;
	ReplicationStateDescriptorRegistry = nullptr;
	NetRefHandleManager = nullptr;
	ObjectReferenceCache = nullptr;
	Groups = nullptr;
}

UE::Net::FNetRefHandle UReplicationBridge::InternalCreateNetObject(FNetRefHandle AllocatedHandle, FNetHandle GlobalHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol)
{
	check(AllocatedHandle.IsValid() && AllocatedHandle.IsCompleteHandle());

	FNetRefHandle Handle = NetRefHandleManager->CreateNetObject(AllocatedHandle, GlobalHandle, ReplicationProtocol);

	if (Handle.IsValid())
	{
		UE_NET_TRACE_NETHANDLE_CREATED(Handle, ReplicationProtocol->DebugName, ReplicationProtocol->ProtocolIdentifier, 0/*Local*/);
	}

	return Handle;
}

UE::Net::FNetRefHandle UReplicationBridge::InternalCreateNetObject(FNetRefHandle AllocatedHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol)
{
	return InternalCreateNetObject(AllocatedHandle, FNetHandle(), ReplicationProtocol);
}

UE::Net::FNetRefHandle UReplicationBridge::InternalCreateNetObjectFromRemote(FNetRefHandle WantedNetHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol)
{
	FNetRefHandle Handle = NetRefHandleManager->CreateNetObjectFromRemote(WantedNetHandle, ReplicationProtocol);

	if (Handle.IsValid())
	{
		UE_NET_TRACE_NETHANDLE_CREATED(Handle, ReplicationProtocol->DebugName, ReplicationProtocol->ProtocolIdentifier, 1/*Remote*/);
	}

	return Handle;
}

void UReplicationBridge::InternalAttachInstanceToNetRefHandle(FNetRefHandle RefHandle, bool bBindInstanceProtocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance, FNetHandle NetHandle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const uint32 ReplicationSystemId = RefHandle.GetReplicationSystemId();
	const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(RefHandle);

	NetRefHandleManager->AttachInstanceProtocol(InternalReplicationIndex, InstanceProtocol, Instance);
	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("InternalAttachInstanceToNetHandle Attached: %s %s to ( InternalIndex: %u )"), *Instance->GetName(), *RefHandle.ToString(), InternalReplicationIndex);

	// Bind instance protocol to dirty state tracking
	if (bBindInstanceProtocol)
	{
		FReplicationInstanceOperationsInternal::BindInstanceProtocol(NetHandle, InstanceProtocol, NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).Protocol);
		ForceNetUpdate(ReplicationSystemId, InternalReplicationIndex);
	}
}

void UReplicationBridge::InternalDetachInstanceFromNetRefHandle(FNetRefHandle RefHandle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(RefHandle);

	if (FReplicationInstanceProtocol* InstanceProtocol = const_cast<FReplicationInstanceProtocol*>(NetRefHandleManager->DetachInstanceProtocol(InternalReplicationIndex)))
	{
		if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::IsBound))
		{
			FReplicationInstanceOperationsInternal::UnbindInstanceProtocol(InstanceProtocol, NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).Protocol);
		}
		ReplicationProtocolManager->DestroyInstanceProtocol(InstanceProtocol);
	}
}

void UReplicationBridge::InternalDestroyNetObject(FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (const FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager->GetInternalIndex(Handle))
	{
		FReplicationSystemInternal* ReplicationSystemInternal = ReplicationSystem->GetReplicationSystemInternal();

		FNetCullDistanceOverrides& NetCullDistanceOverrides = ReplicationSystemInternal->GetNetCullDistanceOverrides();
		NetCullDistanceOverrides.ClearCullDistanceSqr(ObjectInternalIndex);

		FWorldLocations& WorldLocations = ReplicationSystemInternal->GetWorldLocations();
		WorldLocations.RemoveObjectInfoCache(ObjectInternalIndex);

		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectInternalIndex);

		if (ObjectData.bTearOff && NetRefHandleManager->GetNetObjectRefCount(ObjectInternalIndex) > 0U)
		{
			// We need to explicitly notify all ReplicationWriters that we are destroying objects pending tearoff
			// The handle will automatically be removed from HandlesPendingTearOff after the next update

			FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();

			auto NotifyDestroyedObjectPendingTearOff = [&Connections, &ObjectInternalIndex](uint32 ConnectionId)
			{
				FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);			
				Conn->ReplicationWriter->NotifyDestroyedObjectPendingTearOff(ObjectInternalIndex);
			};

			const FNetBitArray& ValidConnections = Connections.GetValidConnections();
			ValidConnections.ForAllSetBits(NotifyDestroyedObjectPendingTearOff);					
		}
	}

	NetRefHandleManager->DestroyNetObject(Handle);
}

void UReplicationBridge::DestroyLocalNetHandle(FNetRefHandle Handle, EEndReplicationFlags EndReplicationFlags)
{
	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("DestroyLocalNetHandle for %s | EndReplicationFlags: %s"), *NetRefHandleManager->PrintObjectFromNetRefHandle(Handle), *LexToString(EndReplicationFlags));

	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId))
	{
		const UE::Net::Private::FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(Handle);

		if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::DestroyNetHandle))
		{
			DestroyGlobalNetHandle(InternalReplicationIndex);
		}

		if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::ClearNetPushId))
		{
			ClearNetPushIds(InternalReplicationIndex);
		}
	}

	// Detach instance protocol
	InternalDetachInstanceFromNetRefHandle(Handle);

	// Allow derived bridges to cleanup any instance info they have stored
	DetachInstance(Handle);

	// If the object is in any groups we need to remove it to make sure that we update filtering
	GetReplicationSystem()->RemoveFromAllGroups(Handle);

	// If we have any attached SubObjects, tag them for destroy as well
	InternalDestroySubObjects(Handle, EndReplicationFlags);

	// Tell ReplicationSystem to destroy the handle
	InternalDestroyNetObject(Handle);
}

void UReplicationBridge::InternalAddSubObject(FNetRefHandle OwnerHandle, FNetRefHandle SubObjectHandle, FNetRefHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder)
{
	using namespace UE::Net::Private;

	EAddSubObjectFlags AddSubObjectFlags = EAddSubObjectFlags::Default;
	AddSubObjectFlags |= InsertionOrder == ESubObjectInsertionOrder::None ? EAddSubObjectFlags::None : EAddSubObjectFlags::ReplicateWithSubObject;

	if (NetRefHandleManager->AddSubObject(OwnerHandle, SubObjectHandle, InsertRelativeToSubObjectHandle, AddSubObjectFlags))
	{
		// If the subobject is new we need to update it immediately to pick it up for replication with its new parent
		ForceNetUpdate(ReplicationSystem->GetId(), NetRefHandleManager->GetInternalIndex(SubObjectHandle));

		// We set the priority of subobjects to be static as they will be prioritized with owner
		ReplicationSystem->SetStaticPriority(SubObjectHandle, 1.0f);
	}
}

void UReplicationBridge::InternalDestroySubObjects(FNetRefHandle OwnerHandle, EEndReplicationFlags Flags)
{
	using namespace UE::Net::Private;

	// Destroy SubObjects
	FInternalNetRefIndex OwnerInternalIndex = NetRefHandleManager->GetInternalIndex(OwnerHandle);
	if (OwnerInternalIndex)
	{
		for (FInternalNetRefIndex SubObjectInternalIndex : NetRefHandleManager->GetChildSubObjects(OwnerInternalIndex))
		{
			FNetRefHandleManager::FReplicatedObjectData& SubObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(SubObjectInternalIndex);

			const FNetRefHandle SubObjectHandle = SubObjectData.RefHandle;
			const bool bDestroySubObjectWithOwner = SubObjectData.bDestroySubObjectWithOwner;
				
			// Tag subobject for destroy. The check against the scope is needed since the subobjects array might contain subobjects already pending destroy.
			if (bDestroySubObjectWithOwner && NetRefHandleManager->IsScopableIndex(SubObjectInternalIndex))
			{
				SubObjectData.bPendingEndReplication = 1U;
				UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("InternalDestroySubObjects %s"), *NetRefHandleManager->PrintObjectFromNetRefHandle(SubObjectHandle));
				DestroyLocalNetHandle(SubObjectHandle, Flags);
			}
		}
	}
}

void UReplicationBridge::EndReplication(FNetRefHandle Handle, EEndReplicationFlags EndReplicationFlags, FEndReplicationParameters* Parameters)
{
	using namespace UE::Net::Private;

	if (!IsReplicatedHandle(Handle))
	{
		return;
	}

	const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(Handle);
	if (NetRefHandleManager->IsLocal(InternalReplicationIndex))
	{
		if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::TearOff))
		{
			// Add handle to list of objects pending tear off and indicate that it should be destroyed during next update
			// We need to do this to cover the case where the torn off object not yet has been added to the scope.
			constexpr bool bIsImmediate = true;
			TearOff(Handle, EndReplicationFlags, bIsImmediate);

			// We do however copy the final state data and mark object to stop propagating state changes
			// Note: Current implementation keeps the mapping between instance and handle alive so that we can handle the case where we both start replication 
			// and tear the object off during the same frame
			InternalTearOff(Handle);
		}
		else 
		{
			const bool bShouldCreateDestructionInfo = Handle.IsStatic() && EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::Destroy);		
			if (bShouldCreateDestructionInfo && Parameters)
			{
				// Add static destruction info for the Handle, should we just copy group memberships?
				// need to copy cached reference as well so that we can "keep" it around even after pointer reuse
				InternalAddDestructionInfo(Handle, *Parameters);
			}

			// If the flush flag is set, we have to explicitly copy the final state and propagate pending changes as we do not expect the source object to be kept alive after the call to EndReplication
			if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::Flush))
			{
				InternalFlushStateData(Handle);
			}

			NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).bPendingEndReplication = 1U;
			DestroyLocalNetHandle(Handle, EndReplicationFlags);	
		}
	}
	else
	{
		if (InternalReplicationIndex != FNetRefHandleManager::InvalidInternalIndex && EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::SkipPendingEndReplicationValidation))
		{
			NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).bPendingEndReplication = 1U;
		}
		// If we get a call to end replication on the client, we need to detach the instance as it might be garbage collected
		InternalDetachInstanceFromNetRefHandle(Handle);
	}
}

void UReplicationBridge::RemoveDestructionInfosForGroup(UE::Net::FNetObjectGroupHandle GroupHandle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FWorldLocations& WorldLocations = GetReplicationSystem()->GetReplicationSystemInternal()->GetWorldLocations();

	if (GroupHandle.IsValid())
	{
		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("RemoveDestructionInfosForGroup GroupIndex: %u"), GroupHandle.GetGroupIndex());

		FNetObjectGroup* Group = Groups->GetGroup(GroupHandle);
		check(Group);

		TArray<FNetRefHandle, TInlineAllocator<384>> ObjectsToRemove;	
		TArray<FInternalNetRefIndex, TInlineAllocator<384>> ObjectIndicesToRemove;	
		for (uint32 InternalObjectIndex : MakeArrayView(Group->Members))
		{
			if (NetRefHandleManager->GetIsDestroyedStartupObject(InternalObjectIndex))
			{
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);
				if (StaticObjectsPendingDestroy.Remove(ObjectData.RefHandle))
				{
					ObjectsToRemove.Add(ObjectData.RefHandle);
					ObjectIndicesToRemove.Add(InternalObjectIndex);
				}
			}			
		}

		for (FNetRefHandle Handle : MakeArrayView(ObjectsToRemove))
		{
			NetRefHandleManager->DestroyNetObject(Handle);
		}

		for (FInternalNetRefIndex InternalReplicationIndex : MakeArrayView(ObjectIndicesToRemove))
		{
			WorldLocations.RemoveObjectInfoCache(InternalReplicationIndex);
		}
	}
	else
	{
		// We should remove all destruction infos and objects
		for (const auto& It : StaticObjectsPendingDestroy)
		{
			NetRefHandleManager->DestroyNetObject(It.Key);
		}

		// Remove from WorldLocations
		for (const auto& It : StaticObjectsPendingDestroy)
		{
			WorldLocations.RemoveObjectInfoCache(It.Value.InternalReplicationIndex);
		}

		StaticObjectsPendingDestroy.Empty();
	}
}

void UReplicationBridge::TearOffHandlesPendingTearOff()
{
	// Initiate tear off
	for (FTearOffInfo Info : MakeArrayView(HandlesPendingTearOff))
	{
		InternalTearOff(Info.Handle);
	}
}

void UReplicationBridge::UpdateHandlesPendingTearOff()
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	TArray<FTearOffInfo, TInlineAllocator<32>> ObjectsStillPendingTearOff;
	for (FTearOffInfo Info : MakeArrayView(HandlesPendingTearOff))
	{
		if (FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager->GetInternalIndex(Info.Handle))
		{
			// Immediate tear-off or object that no longer are referenced by any connections are destroyed
			if (NetRefHandleManager->GetNetObjectRefCount(ObjectInternalIndex) == 0U || Info.bIsImmediate)
			{
				FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectInternalIndex);
				ObjectData.bPendingEndReplication = 1U;
				DestroyLocalNetHandle(Info.Handle, Info.DestroyFlags);
			}
			else
			{
				// If the object is still in scope remove it from scope as objects being torn-off should not be added to new connections
				if (NetRefHandleManager->IsScopableIndex(ObjectInternalIndex))
				{
					// Mark object and subobjects as no longer scopable, and that we should not propagate changed states
					NetRefHandleManager->RemoveFromScope(ObjectInternalIndex);
					for (FInternalNetRefIndex SubObjectIndex : NetRefHandleManager->GetSubObjects(ObjectInternalIndex))
					{
						NetRefHandleManager->RemoveFromScope(SubObjectIndex);
					}
				}
			
				// Keep object in the pending tear-off list until the object is no longer referenced by any ReplicationWriter
				constexpr bool bIsImmediate = false;
				ObjectsStillPendingTearOff.Add(FTearOffInfo(Info.Handle, Info.DestroyFlags, bIsImmediate));
			}
		}
	}

	HandlesPendingTearOff.Reset();
	HandlesPendingTearOff.Insert(ObjectsStillPendingTearOff.GetData(), ObjectsStillPendingTearOff.Num(), 0);
}

void UReplicationBridge::TearOff(FNetRefHandle Handle, EEndReplicationFlags DestroyFlags, bool bIsImmediate)
{
	if (!HandlesPendingTearOff.FindByPredicate([&](const FTearOffInfo& Info){ return Info.Handle == Handle; }))
	{
		HandlesPendingTearOff.Add(FTearOffInfo(Handle, DestroyFlags, bIsImmediate));
	}	
}

void UReplicationBridge::InternalFlushStateData(UE::Net::FNetSerializationContext& SerializationContext, UE::Net::Private::FChangeMaskCache& ChangeMaskCache, UE::Net::FNetBitStreamWriter& ChangeMaskWriter, uint32 InternalObjectIndex)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (InternalObjectIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);

	// Copy state data, if object already is torn off there is nothing to do
	if (ObjectData.bTearOff)
	{
		return;
	}

	for (FInternalNetRefIndex SubObjectInternalIndex : NetRefHandleManager->GetChildSubObjects(InternalObjectIndex))
	{
		InternalFlushStateData(SerializationContext, ChangeMaskCache, ChangeMaskWriter, SubObjectInternalIndex);
	}

	if (EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll | EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
	{
		CallPreSendUpdateSingleHandle(ObjectData.RefHandle);
	} 

	FReplicationInstanceOperationsInternal::QuantizeObjectStateData(ChangeMaskWriter, ChangeMaskCache, *NetRefHandleManager, SerializationContext, InternalObjectIndex);

	// Clear the quantize flag since it was done directly here.
	NetRefHandleManager->GetDirtyObjectsToQuantize().ClearBit(InternalObjectIndex);

	// $IRIS TODO:  Should we also clear the DirtyTracker flags for this flushed object ?
}

void UReplicationBridge::InternalFlushStateData(FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(InternalFlushStateData);

	const uint32 InternalObjectIndex = NetRefHandleManager->GetInternalIndex(Handle);
	if (InternalObjectIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);

	FChangeMaskCache ChangeMaskCache;
	FNetBitStreamWriter ChangeMaskWriter;

	// Setup context
	FNetSerializationContext SerializationContext;
	FInternalNetSerializationContext InternalContext(ReplicationSystem);
	SerializationContext.SetInternalContext(&InternalContext);
	SerializationContext.SetNetStatsContext(ReplicationSystem->GetReplicationSystemInternal()->GetNetTypeStats().GetNetStatsContext());

	InternalFlushStateData(SerializationContext, ChangeMaskCache, ChangeMaskWriter, InternalObjectIndex);

	// Iterate over connections and propagate dirty changemasks to all connections already scoping this object
	if (ChangeMaskCache.Indices.Num() > 0)
	{
		FReplicationConnections& Connections = ReplicationSystem->GetReplicationSystemInternal()->GetConnections();

		auto&& UpdateDirtyChangeMasks = [&Connections, &ChangeMaskCache](uint32 ConnectionId)
		{
			FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);			

			const bool bMarkForTearOff = false;
			Conn->ReplicationWriter->ForceUpdateDirtyChangeMasks(ChangeMaskCache, FReplicationWriter::FlushFlags_FlushState, bMarkForTearOff);
		};
		const FNetBitArray& ValidConnections = Connections.GetValidConnections();
		ValidConnections.ForAllSetBits(UpdateDirtyChangeMasks);		
	}
}

void UReplicationBridge::InternalTearOff(FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(InternalTearOff);

	const uint32 InternalObjectIndex = NetRefHandleManager->GetInternalIndex(Handle);

	if (InternalObjectIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return;
	}

	FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);

	// Copy state data and tear off now
	if (!ObjectData.bTearOff)
	{
		// Mark subobjects for TearOff as well.
		for (FInternalNetRefIndex SubObjectInternalIndex : NetRefHandleManager->GetChildSubObjects(InternalObjectIndex))
		{
			InternalTearOff(NetRefHandleManager->GetNetRefHandleFromInternalIndex(SubObjectInternalIndex));
		}	

		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("TearOff %s"), *Handle.ToString());

		// Force copy of final state data as we will detach the object after scope update
		FChangeMaskCache ChangeMaskCache;
		FNetBitStreamWriter ChangeMaskWriter;

		// Setup context
		FNetSerializationContext SerializationContext;
		FInternalNetSerializationContext InternalContext(ReplicationSystem);
		SerializationContext.SetInternalContext(&InternalContext);
		SerializationContext.SetNetStatsContext(ReplicationSystem->GetReplicationSystemInternal()->GetNetTypeStats().GetNetStatsContext());

		if (ObjectData.InstanceProtocol && EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll | EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
		{
			CallPreSendUpdateSingleHandle(Handle);
		} 

		if (ObjectData.InstanceProtocol && ObjectData.Protocol->InternalTotalSize > 0U)
		{
			FReplicationInstanceOperationsInternal::QuantizeObjectStateData(ChangeMaskWriter, ChangeMaskCache, *NetRefHandleManager, SerializationContext, InternalObjectIndex);
		}
		else
		{
			// Nothing to copy, but we must still propagate the tear-off state.
			FChangeMaskCache::FCachedInfo& Info = ChangeMaskCache.AddEmptyChangeMaskForObject(InternalObjectIndex);
			// If we are a subobject we must also mark owner as dirty.
			const uint32 SubObjectOwnerIndex = ObjectData.SubObjectRootIndex;
			if (SubObjectOwnerIndex != FNetRefHandleManager::InvalidInternalIndex) 
			{
				ChangeMaskCache.AddSubObjectOwnerDirty(SubObjectOwnerIndex);
			}			
		}

		// Propagate changes to all connections that we currently have in scope
		FReplicationConnections& Connections = ReplicationSystem->GetReplicationSystemInternal()->GetConnections();

		// Iterate over connections and propagate dirty changemasks to all connections already scoping this object
		auto UpdateDirtyChangeMasks = [&Connections, &ChangeMaskCache](uint32 ConnectionId)
		{
			FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);
			const bool bMarkForTearOff = true;
			Conn->ReplicationWriter->ForceUpdateDirtyChangeMasks(ChangeMaskCache, FReplicationWriter::FlushFlags_None, bMarkForTearOff);
		};
		const FNetBitArray& ValidConnections = Connections.GetValidConnections();
		ValidConnections.ForAllSetBits(UpdateDirtyChangeMasks);		

		// Mark object as being torn-off and that we should no longer propagate state changes
		ObjectData.bTearOff = 1U;
		ObjectData.bShouldPropagateChangedStates = 0U;
	}
}

UE::Net::FNetRefHandle UReplicationBridge::InternalAddDestructionInfo(FNetRefHandle Handle, const FEndReplicationParameters& Parameters)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FReplicationProtocolManager* ProtocolManager = GetReplicationProtocolManager();

	// Create handle for DestructionInfo to be able to scope destruction infos
	const FNetObjectGroupHandle LevelGroupHandle = GetLevelGroup(Parameters.Level);
	FNetRefHandle DestructionInfoHandle = NetRefHandleManager->CreateHandleForDestructionInfo(Handle, DestructionInfoProtocol);

	if (LevelGroupHandle.IsValid())
	{
		GetReplicationSystem()->AddToGroup(LevelGroupHandle, DestructionInfoHandle);
	}

	const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(DestructionInfoHandle);

	// We also need to store the actual data we send to destroy static references when they are scoped
	FDestructionInfo PersistentDestructionInfo;
	PersistentDestructionInfo.LevelGroupHandle = LevelGroupHandle;
	PersistentDestructionInfo.StaticRef = FObjectReferenceCache::MakeNetObjectReference(Handle);
	PersistentDestructionInfo.InternalReplicationIndex = InternalReplicationIndex;

	StaticObjectsPendingDestroy.Add(DestructionInfoHandle, PersistentDestructionInfo);

	// If we use distance based Prioritization for destruction infos we need to populate the quantized state used for prioritization.
	const bool bUseDynamicPrioritization = Parameters.bUseDistanceBasedPrioritization;
	if (bUseDynamicPrioritization)
	{
		// Use WorldLocations to feed the location of the destruction info so that it can be prioritized properly.
		FWorldLocations& WorldLocations = GetReplicationSystem()->GetReplicationSystemInternal()->GetWorldLocations();
		WorldLocations.InitObjectInfoCache(InternalReplicationIndex);
		WorldLocations.UpdateWorldLocation(InternalReplicationIndex, Parameters.Location);

		GetReplicationSystem()->SetPrioritizer(DestructionInfoHandle, DefaultSpatialNetObjectPrioritizerHandle);
	}

	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("UReplicationBridge::InternalAddDestructionInfo %s (InternalIndex: %u ) for %s GroupIndex: %u"), *DestructionInfoHandle.ToString(), InternalReplicationIndex,  *PrintObjectFromNetRefHandle(Handle), LevelGroupHandle.GetGroupIndex());
	
	return DestructionInfoHandle;
}

bool UReplicationBridge::IsReplicatedHandle(FNetRefHandle Handle) const
{
	return Handle.IsValid() && ReplicationSystem->IsValidHandle(Handle);
}

void UReplicationBridge::SetNetPushIdOnFragments(const TArrayView<const UE::Net::FReplicationFragment*const>& Fragments, const UE::Net::Private::FNetPushObjectHandle& PushHandle)
{
#if WITH_PUSH_MODEL
	using namespace UE::Net;

	constexpr uint32 MaxFragmentOwnerCount = 1U;
	UObject* FragmentOwners[MaxFragmentOwnerCount] = {};
	FReplicationStateOwnerCollector FragmentOwnerCollector(FragmentOwners, MaxFragmentOwnerCount);
	UObject* Instance = nullptr;
	for (const FReplicationFragment* Fragment : Fragments)
	{
		if (!EnumHasAnyFlags(Fragment->GetTraits(), EReplicationFragmentTraits::HasPushBasedDirtiness))
		{
			continue;
		}

		FragmentOwnerCollector.Reset();
		Fragment->CollectOwner(&FragmentOwnerCollector);
		UObject* FragmentOwner = (FragmentOwnerCollector.GetOwnerCount() > 0 ? FragmentOwnerCollector.GetOwners()[0] : static_cast<UObject*>(nullptr));
		if (FragmentOwner != nullptr && FragmentOwner != Instance)
		{
			Instance = FragmentOwner;
			UE_NET_IRIS_SET_PUSH_ID(Instance, PushHandle);
		}
	}
#endif
}

void UReplicationBridge::ClearNetPushIdOnFragments(const TArrayView<const UE::Net::FReplicationFragment*const>& Fragments)
{
#if WITH_PUSH_MODEL
	using namespace UE::Net;

	constexpr uint32 MaxFragmentOwnerCount = 1U;
	UObject* FragmentOwners[MaxFragmentOwnerCount] = {};
	FReplicationStateOwnerCollector FragmentOwnerCollector(FragmentOwners, MaxFragmentOwnerCount);
	UObject* Instance = nullptr;
	for (const FReplicationFragment* Fragment : Fragments)
	{
		if (!EnumHasAnyFlags(Fragment->GetTraits(), EReplicationFragmentTraits::HasPushBasedDirtiness))
		{
			continue;
		}

		FragmentOwnerCollector.Reset();
		Fragment->CollectOwner(&FragmentOwnerCollector);
		UObject* FragmentOwner = (FragmentOwnerCollector.GetOwnerCount() > 0 ? FragmentOwnerCollector.GetOwners()[0] : static_cast<UObject*>(nullptr));
		if (FragmentOwner != nullptr && FragmentOwner != Instance)
		{
			Instance = FragmentOwner;
			if (IsValid(FragmentOwner))
			{
				UE_NET_IRIS_CLEAR_PUSH_ID(FragmentOwner);
			}
		}
	}
#endif
}

void UReplicationBridge::NotifyStreamingLevelUnload(const UObject* Level)
{
	// Destroy group associated with level
	UE::Net::FNetObjectGroupHandle LevelGroupHandle;
	if (LevelGroups.RemoveAndCopyValue(FObjectKey(Level), LevelGroupHandle))
	{
		RemoveDestructionInfosForGroup(LevelGroupHandle);
		ReplicationSystem->DestroyGroup(LevelGroupHandle);
	}
}

UE::Net::FNetObjectGroupHandle UReplicationBridge::CreateLevelGroup(const UObject* Level)
{
	using namespace UE::Net;

	FNetObjectGroupHandle LevelGroupHandle = ReplicationSystem->CreateGroup();
	if (ensure(LevelGroupHandle.IsValid()))
	{
		ReplicationSystem->AddExclusionFilterGroup(LevelGroupHandle);
		LevelGroups.Emplace(FObjectKey(Level), LevelGroupHandle);
	}

	return LevelGroupHandle;
}

UE::Net::FNetObjectGroupHandle UReplicationBridge::GetLevelGroup(const UObject* Level) const
{
	const UE::Net::FNetObjectGroupHandle* LevelGroupHandle = LevelGroups.Find(FObjectKey(Level));
	return (LevelGroupHandle != nullptr ? *LevelGroupHandle : UE::Net::FNetObjectGroupHandle());
}

void UReplicationBridge::DestroyGlobalNetHandle(UE::Net::Private::FInternalNetRefIndex InternalReplicationIndex)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex);
	if (ObjectData.NetHandle.IsValid())
	{
		FNetHandleDestroyer::DestroyNetHandle(ObjectData.NetHandle);
	}
}

void UReplicationBridge::ClearNetPushIds(UE::Net::Private::FInternalNetRefIndex InternalReplicationIndex)
{
#if WITH_PUSH_MODEL
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex);
	if (const FReplicationInstanceProtocol* InstanceProtocol = ObjectData.InstanceProtocol)
	{
		if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
		{
			TArrayView<const FReplicationFragment* const> Fragments(InstanceProtocol->Fragments, InstanceProtocol->FragmentCount);
			ClearNetPushIdOnFragments(Fragments);
		}
	}
#endif
}

FString UReplicationBridge::PrintObjectFromNetRefHandle(FNetRefHandle RefHandle) const
{
	return NetRefHandleManager->PrintObjectFromNetRefHandle(RefHandle);
}

const TCHAR* LexToString(EReplicationBridgeDestroyInstanceReason Reason)
{
	switch (Reason)
	{
	case EReplicationBridgeDestroyInstanceReason::DoNotDestroy:
	{
		return TEXT("DoNotDestroy");
	}
	case EReplicationBridgeDestroyInstanceReason::TearOff:
	{
		return TEXT("TearOff");
	}
	case EReplicationBridgeDestroyInstanceReason::Destroy:
	{
		return TEXT("Destroy");
	}
	default:
	{
		return TEXT("[Invalid]");
	}
	}
}

const TCHAR* LexToString(EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	switch (DestroyFlags)
	{
	case EReplicationBridgeDestroyInstanceFlags::None:
	{
		return TEXT("None");
	}
	case EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote:
	{
		return TEXT("AllowDestroyInstanceFromRemote");
	}
	default:
	{
		return TEXT("[Invalid]");
	}
	}
}

FString LexToString(EEndReplicationFlags EndReplicationFlags)
{
	
	if (EndReplicationFlags == EEndReplicationFlags::None)
	{
		return TEXT("None");
	}

	FString Flags;

	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::Destroy))
	{
		Flags += TEXT("Destroy");
	}
	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::TearOff))
	{
		if (!Flags.IsEmpty()) { Flags += TEXT(','); }
		Flags += TEXT("TearOff");
	}
	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::Flush))
	{
		if (!Flags.IsEmpty()) { Flags += TEXT(','); }
		Flags += TEXT("Flush");
	}
	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::DestroyNetHandle))
	{
		if (!Flags.IsEmpty()) { Flags += TEXT(','); };
		Flags += TEXT("DestroyNetHandle");
	}
	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::ClearNetPushId))
	{
		if (!Flags.IsEmpty()) { Flags += TEXT(','); };
		Flags += TEXT("ClearNetPushId");
	}
	if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::SkipPendingEndReplicationValidation))
	{
		if (!Flags.IsEmpty()) { Flags += TEXT(','); };
		Flags += TEXT("SkipPendingEndReplicationValidation");
	}
	
	return Flags;
}