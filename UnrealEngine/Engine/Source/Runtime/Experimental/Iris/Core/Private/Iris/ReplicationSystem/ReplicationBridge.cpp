// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "Iris/IrisConfigInternal.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Net/Core/NetBitArray.h"
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

#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
#define UE_LOG_REPLICATIONBRIDGE(Category, Format, ...)  UE_LOG(LogIrisBridge, Category, TEXT("ReplicationBridge(%u)::") Format, ReplicationSystem->GetId(), ##__VA_ARGS__)
#else
#define UE_LOG_REPLICATIONBRIDGE(Category, Format, ...)  UE_LOG(LogIrisBridge, Category, TEXT("ReplicationBridge::") Format, ##__VA_ARGS__)
#endif

/*
* ReplicationBridge Implementation
*/
UReplicationBridge::UReplicationBridge()
: ReplicationSystem(nullptr)
, ReplicationProtocolManager(nullptr)
, ReplicationStateDescriptorRegistry(nullptr)
, NetHandleManager(nullptr)
, DestructionInfoProtocol(nullptr)
{
}

bool UReplicationBridge::WriteNetHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetHandle Handle)
{
	return true;
}

UE::Net::FNetHandle UReplicationBridge::CreateNetHandleFromRemote(FNetHandle SubObjectOwnerNetHandle, FNetHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context)
{
	return FNetHandle();
};

void UReplicationBridge::PreSendUpdateSingleHandle(FNetHandle Handle)
{
}

void UReplicationBridge::PreSendUpdate()
{
}

void UReplicationBridge::UpdateInstancesWorldLocation()
{
}

void UReplicationBridge::PostApplyInitialState(FNetHandle Handle)
{
}

void UReplicationBridge::DetachInstanceFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance)
{
}

void UReplicationBridge::DetachInstance(FNetHandle Handle)
{
}

void UReplicationBridge::PruneStaleObjects()
{
}

void UReplicationBridge::SetNetDriver(UNetDriver* NetDriver)
{
}

void UReplicationBridge::GetInitialDependencies(FNetHandle Handle, FNetDependencyInfoArray& OutDependencies) const
{
	return;
}

void UReplicationBridge::CallGetInitialDependencies(FNetHandle Handle, FNetDependencyInfoArray& OutDependencies) const
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

void UReplicationBridge::DetachSubObjectInstancesFromRemote(FNetHandle OwnerHandle, bool bTearOff, bool bShouldDestroyInstance)
{
	using namespace UE::Net::Private;

	FInternalNetHandle OwnerInternalIndex = NetHandleManager->GetInternalIndex(OwnerHandle);
	if (OwnerInternalIndex)
	{
		for (const FInternalNetHandle SubObjectInternalIndex : NetHandleManager->GetChildSubObjects(OwnerInternalIndex))
		{
			FNetHandleManager::FReplicatedObjectData& SubObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(SubObjectInternalIndex);
			const FNetHandle SubObjectHandle = SubObjectData.Handle;
			SubObjectData.bTearOff = bTearOff;

			CallDetachInstanceFromRemote(SubObjectHandle, bTearOff, bShouldDestroyInstance);
		}
	}
}

void UReplicationBridge::DestroyNetObjectFromRemote(FNetHandle Handle, bool bTearOff, bool bDestroyInstance)
{
	using namespace UE::Net::Private;

	if (Handle.IsValid())
	{
		FInternalNetHandle OwnerInternalIndex = NetHandleManager->GetInternalIndex(Handle);
		FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(OwnerInternalIndex);
		ObjectData.bTearOff = bTearOff;

		// if the a subobject owner is to be destroyed we want to detach all subobjects before doing so to ensure we execute expected callbacks
		// We keep tracking them internally
		DetachSubObjectInstancesFromRemote(Handle, bTearOff, bDestroyInstance);
		
		// Allow derived bridges to cleanup any instance info they have stored
		CallDetachInstanceFromRemote(Handle, bTearOff, bDestroyInstance);

		// Detach instance protocol
		InternalDetachInstanceFromNetHandle(Handle);
	
		// Destroy the NetHandle
		UReplicationBridge::InternalDestroyNetObject(Handle);

		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("DestroyNetObjectFromRemote Remote %s"), *Handle.ToString());
	}
}

void UReplicationBridge::CallPreSendUpdate(float DeltaSeconds)
{
	// Tear-off all handles pending tear-off
	TearOffHandlesPendingTearOff();

	PreSendUpdate();
}

void UReplicationBridge::CallPreSendUpdateSingleHandle(FNetHandle Handle)
{
	PreSendUpdateSingleHandle(Handle);
}

void UReplicationBridge::CallUpdateInstancesWorldLocation()
{
	UpdateInstancesWorldLocation();
}

void UReplicationBridge::CallDetachInstance(FNetHandle Handle)
{
	DetachInstance(Handle);
}

void UReplicationBridge::CallDetachInstanceFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance)
{
	DetachInstanceFromRemote(Handle, bTearOff, bShouldDestroyInstance);
}

void UReplicationBridge::CallPruneStaleObjects()
{
	PruneStaleObjects();
}

bool UReplicationBridge::CallWriteNetHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetHandle Handle)
{
	using namespace UE::Net;

	// Special path for destruction info
	if (Context.bIsDestructionInfo)
	{
		UE_NET_TRACE_SCOPE(DestructionInfo, *Context.SerializationContext.GetBitStreamWriter(), Context.SerializationContext.GetTraceCollector(), ENetTraceVerbosity::Trace);

		const FDestructionInfo* Info = StaticObjectsPendingDestroy.Find(Handle);
		if (ensureAlwaysMsgf(Info, TEXT("Failed to write destructionInfo for %s"), *Handle.ToString()))
		{
			// Write destruction info
			WriteFullNetObjectReference(Context.SerializationContext, Info->StaticRef);
		}
		else
		{
			// Write invalid reference
			WriteFullNetObjectReference(Context.SerializationContext, FNetObjectReference());
		}
		return !Context.SerializationContext.HasErrorOrOverflow();	
	}
	else
	{
		return WriteNetHandleCreationInfo(Context, Handle);
	}
}

UE::Net::FNetHandle UReplicationBridge::CallCreateNetHandleFromRemote(FNetHandle SubObjectOwnerHandle, FNetHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context)
{
	using namespace UE::Net;

	check(!Context.bIsDestructionInfo);

	FNetHandle ResultingHandle = CreateNetHandleFromRemote(SubObjectOwnerHandle, WantedNetHandle, Context);

	// Track subobjects on clients
	if (ResultingHandle.IsValid() && SubObjectOwnerHandle.IsValid())
	{
		NetHandleManager->AddSubObject(SubObjectOwnerHandle, ResultingHandle);
	}

	return ResultingHandle;
}

void UReplicationBridge::ReadAndExecuteDestructionInfoFromRemote(FReplicationBridgeSerializationContext& Context)
{
	using namespace UE::Net;

	check(Context.bIsDestructionInfo);

	UE_NET_TRACE_SCOPE(DestructionInfo, *Context.SerializationContext.GetBitStreamReader(), Context.SerializationContext.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Destroy instance here, or defer to later?
	FNetObjectReference ReferenceToDestroy;
	ReadFullNetObjectReference(Context.SerializationContext, ReferenceToDestroy);

	// Destroy the reference
	// Resolve the reference in order to be able to destroy it
	if (ObjectReferenceCache->ResolveObjectReference(ReferenceToDestroy, Context.SerializationContext.GetInternalContext()->ResolveContext) != nullptr)
	{
		// Need to forward this as we do not really know how to destroy the actual instance
		const bool bTearOff = false;
		CallDetachInstanceFromRemote(ReferenceToDestroy.GetRefHandle(), bTearOff, true);
	}
}

void UReplicationBridge::CallPostApplyInitialState(FNetHandle Handle)
{
	PostApplyInitialState(Handle);
}

UReplicationBridge::~UReplicationBridge()
{
}

void UReplicationBridge::Initialize(UReplicationSystem* InReplicationSystem)
{
	using namespace UE::Net;

	Private::FReplicationSystemInternal* ReplicationSystemInternal = InReplicationSystem->GetReplicationSystemInternal();

	ReplicationSystem = InReplicationSystem;
	ReplicationProtocolManager = &ReplicationSystemInternal->GetReplicationProtocolManager();
	ReplicationStateDescriptorRegistry = &ReplicationSystemInternal->GetReplicationStateDescriptorRegistry();
	NetHandleManager = &ReplicationSystemInternal->GetNetHandleManager();
	ObjectReferenceCache = &ReplicationSystemInternal->GetObjectReferenceCache();
	Groups = &ReplicationSystemInternal->GetGroups();

	// Create destruction info protocol
	{
		const FReplicationFragments RegisteredFragments;
		DestructionInfoProtocol = ReplicationProtocolManager->CreateReplicationProtocol(ReplicationProtocolManager->CalculateProtocolIdentifier(RegisteredFragments), RegisteredFragments, TEXT("InternalDestructionInfo"), false);
	}
}

void UReplicationBridge::Deinitialize()
{
	// Just set the protocol to null, it will be destroyed with the protocolmanager
	DestructionInfoProtocol = nullptr;

	ReplicationSystem = nullptr;
	ReplicationProtocolManager = nullptr;
	ReplicationStateDescriptorRegistry = nullptr;
	NetHandleManager = nullptr;
	ObjectReferenceCache = nullptr;
	Groups = nullptr;
}

UE::Net::FNetHandle UReplicationBridge::InternalCreateNetObject(FNetHandle AllocatedHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol)
{
	check(AllocatedHandle.IsValid() && AllocatedHandle.IsCompleteHandle());

	FNetHandle Handle = NetHandleManager->CreateNetObject(AllocatedHandle, ReplicationProtocol);
	
	if (Handle.IsValid())
	{
		UE_NET_TRACE_NETHANDLE_CREATED(Handle, ReplicationProtocol->DebugName, ReplicationProtocol->ProtocolIdentifier, 0/*Local*/);
	}

	return Handle;
}

UE::Net::FNetHandle UReplicationBridge::InternalCreateNetObjectFromRemote(FNetHandle WantedNetHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol)
{
	FNetHandle Handle = NetHandleManager->CreateNetObjectFromRemote(WantedNetHandle, ReplicationProtocol);

	if (Handle.IsValid())
	{
		UE_NET_TRACE_NETHANDLE_CREATED(Handle, ReplicationProtocol->DebugName, ReplicationProtocol->ProtocolIdentifier, 1/*Remote*/);
	}

	return Handle;
}

void UReplicationBridge::InternalAttachInstanceToNetHandle(FNetHandle Handle, bool bBindInstanceProtocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const uint32 ReplicationSystemId = Handle.GetReplicationSystemId();
	const uint32 InternalReplicationIndex = NetHandleManager->GetInternalIndex(Handle);

	NetHandleManager->AttachInstanceProtocol(InternalReplicationIndex, InstanceProtocol, Instance);
	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("InternalAttachInstanceToNetHandle Attached: %s %s to ( InternalIndex: %u )"), *Instance->GetName(), *Handle.ToString(), InternalReplicationIndex);

	// Bind instance protocol to dirty state tracking
	if (bBindInstanceProtocol)
	{
		// Set push ID only if any state supports it. If no state supports it then we might crash if setting the ID.
#if WITH_PUSH_MODEL
		if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
		{
			const FNetPushObjectHandle PushHandle(InternalReplicationIndex, ReplicationSystemId);
			TArrayView<const FReplicationFragment* const> Fragments(InstanceProtocol->Fragments, InstanceProtocol->FragmentCount);
			SetNetPushIdOnFragments(Fragments, PushHandle);
		}
#endif

		FReplicationInstanceOperationsInternal::BindInstanceProtocol(ReplicationSystemId, InternalReplicationIndex, InstanceProtocol, NetHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).Protocol);
		MarkNetObjectStateDirty(Handle.GetReplicationSystemId(), InternalReplicationIndex);
	}
}

void UReplicationBridge::InternalDetachInstanceFromNetHandle(FNetHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const uint32 InternalReplicationIndex = NetHandleManager->GetInternalIndex(Handle);

	if (FReplicationInstanceProtocol* InstanceProtocol = const_cast<FReplicationInstanceProtocol*>(NetHandleManager->DetachInstanceProtocol(InternalReplicationIndex)))
	{
		if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::IsBound))
		{
#if WITH_PUSH_MODEL
			if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
			{
				TArrayView<const FReplicationFragment* const> Fragments(InstanceProtocol->Fragments, InstanceProtocol->FragmentCount);
				ClearNetPushIdOnFragments(Fragments);
			}
#endif
			FReplicationInstanceOperationsInternal::UnbindInstanceProtocol(InstanceProtocol, NetHandleManager->GetReplicatedObjectDataNoCheck(InternalReplicationIndex).Protocol);
		}
		ReplicationProtocolManager->DestroyInstanceProtocol(InstanceProtocol);
	}
}

void UReplicationBridge::InternalDestroyNetObject(FNetHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (const FInternalNetHandle ObjectInternalIndex = NetHandleManager->GetInternalIndex(Handle))
	{
		const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(ObjectInternalIndex);

		if (ObjectData.bTearOff && NetHandleManager->GetNetObjectRefCount(ObjectInternalIndex) > 0U)
		{
			// We need to explicitly notify all ReplicationWriters that we are destroying objects pending tearoff
			// The handle will automatically be removed from HandlesPendingTearOff after the next update

			FReplicationConnections& Connections = ReplicationSystem->GetReplicationSystemInternal()->GetConnections();

			auto NotifyDestroyedObjectPendingTearOff = [&Connections, &ObjectInternalIndex](uint32 ConnectionId)
			{
				FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);			
				Conn->ReplicationWriter->NotifyDestroyedObjectPendingTearOff(ObjectInternalIndex);
			};

			const FNetBitArray& ValidConnections = Connections.GetValidConnections();
			ValidConnections.ForAllSetBits(NotifyDestroyedObjectPendingTearOff);					
		}
	}

	NetHandleManager->DestroyNetObject(Handle);
}

void UReplicationBridge::DestroyLocalNetHandle(FNetHandle Handle)
{
	// Detach instance protocol
	InternalDetachInstanceFromNetHandle(Handle);

	// Allow derived bridges to cleanup any instance info they have stored
	DetachInstance(Handle);

	// If the object is in any groups we need to remove it to make sure that we update filtering
	GetReplicationSystem()->RemoveFromAllGroups(Handle);

	// If we have any attached SubObjects, tag them for destroy as well
	InternalDestroySubObjects(Handle);

	// Tell ReplicationSystem to destroy the NetHandle
	InternalDestroyNetObject(Handle);

	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("DestroyLocalNetHandle Local %s"), *Handle.ToString());
}

UE::Net::FNetHandle UReplicationBridge::InternalGetSubObjectOwner(FNetHandle SubObjectHandle) const
{
	using namespace UE::Net::Private;

	const FInternalNetHandle InternalHandle = NetHandleManager->GetInternalIndex(SubObjectHandle);
	if (InternalHandle != FNetHandleManager::InvalidInternalIndex)
	{
		const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(InternalHandle);
		const FInternalNetHandle OwnerInternalHandle = ObjectData.SubObjectRootIndex;

		if (OwnerInternalHandle != FNetHandleManager::InvalidInternalIndex)
		{
			return NetHandleManager->GetReplicatedObjectDataNoCheck(OwnerInternalHandle).Handle;
		}
	}

	return FNetHandle();
}

void UReplicationBridge::InternalAddSubObject(FNetHandle OwnerHandle, FNetHandle SubObjectHandle, FNetHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder)
{
	using namespace UE::Net::Private;

	EAddSubObjectFlags AddSubObjectFlags = EAddSubObjectFlags::Default;
	AddSubObjectFlags |= InsertionOrder == ESubObjectInsertionOrder::None ? EAddSubObjectFlags::None : EAddSubObjectFlags::ReplicateWithSubObject;

	if (NetHandleManager->AddSubObject(OwnerHandle, SubObjectHandle, InsertRelativeToSubObjectHandle, AddSubObjectFlags))
	{
		// If the subobject is not new we need to mark it as dirty to pick it up for replication with its new parent
		MarkNetObjectStateDirty(ReplicationSystem->GetId(), NetHandleManager->GetInternalIndex(SubObjectHandle));

		// We set the priority of subobjects to be static as they will be prioritized with owner
		ReplicationSystem->SetStaticPriority(SubObjectHandle, 1.f);
	}
}

void UReplicationBridge::InternalDestroySubObjects(FNetHandle OwnerHandle)
{
	using namespace UE::Net::Private;

	// Destroy SubObjects
	FInternalNetHandle OwnerInternalIndex = NetHandleManager->GetInternalIndex(OwnerHandle);
	if (OwnerInternalIndex)
	{
		for (FInternalNetHandle SubObjectInternalIndex : NetHandleManager->GetChildSubObjects(OwnerInternalIndex))
		{
			const FNetHandleManager::FReplicatedObjectData& SubObjectData = NetHandleManager->GetReplicatedObjectData(SubObjectInternalIndex);
			const FNetHandle SubObjectHandle = SubObjectData.Handle;
			const bool bDestroySubObjectWithOwner = SubObjectData.bDestroySubObjectWithOwner;
				
			// Tag subobject for destroy, the check against the scope is needed since the subobjects array might contain subobjects already pending destroy
			if (bDestroySubObjectWithOwner && NetHandleManager->IsScopableIndex(SubObjectInternalIndex))
			{
				UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("InternalDestroySubObjects %s - SubObject %s"), *OwnerHandle.ToString(), *SubObjectHandle.ToString());
				DestroyLocalNetHandle(SubObjectHandle);
			}
		}
	}
}

void UReplicationBridge::EndReplication(FNetHandle Handle, EEndReplicationFlags EndReplicationFlags, FEndReplicationParameters* Parameters)
{
	if (!IsReplicatedHandle(Handle))
	{
		return;
	}

	if (NetHandleManager->IsLocalNetHandle(Handle))
	{
		if (EnumHasAnyFlags(EndReplicationFlags, EEndReplicationFlags::TearOff))
		{
			// Add handle to list of objects pending tear off and indicate that it should be destroyed during next update
			// We need to do this to cover the case where the torn off object not yet has been added to scope,
			TearOff(Handle, true);

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

			DestroyLocalNetHandle(Handle);	
		}
	}
	else
	{
		// If we get a call to end replication on the client, we need to detach the instance as it might be garbage collected
		InternalDetachInstanceFromNetHandle(Handle);
	}
}

void UReplicationBridge::RemoveDestructionInfosForGroup(UE::Net::FNetObjectGroupHandle GroupHandle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FWorldLocations& WorldLocations = GetReplicationSystem()->GetReplicationSystemInternal()->GetWorldLocations();

	if (GroupHandle)
	{
		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("RemoveDestructionInfosForGroup GroupIndex: %u"), GroupHandle);

		FNetObjectGroup* Group = Groups->GetGroup(GroupHandle);
		check(Group);

		TArray<FNetHandle, TInlineAllocator<384>> ObjectsToRemove;	
		TArray<FInternalNetHandle, TInlineAllocator<384>> ObjectIndicesToRemove;	
		for (uint32 InternalObjectIndex : MakeArrayView(Group->Members))
		{
			if (NetHandleManager->GetIsDestroyedStartupObject(InternalObjectIndex))
			{
				const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);
				if (StaticObjectsPendingDestroy.Remove(ObjectData.Handle))
				{
					ObjectsToRemove.Add(ObjectData.Handle);
					ObjectIndicesToRemove.Add(InternalObjectIndex);
				}
			}			
		}

		for (FNetHandle Handle : MakeArrayView(ObjectsToRemove))
		{
			NetHandleManager->DestroyNetObject(Handle);
		}

		for (FInternalNetHandle InternalReplicationIndex : MakeArrayView(ObjectIndicesToRemove))
		{
			constexpr bool bHasWorldLocation = false;
			WorldLocations.SetHasWorldLocation(InternalReplicationIndex, bHasWorldLocation);
		}
	}
	else
	{
		// We should remove all destruction infos and objects
		for (const auto& It : StaticObjectsPendingDestroy)
		{
			NetHandleManager->DestroyNetObject(It.Key);
		}

		// Remove from WorldLocations
		for (const auto& It : StaticObjectsPendingDestroy)
		{
			constexpr bool bHasWorldLocation = false;
			WorldLocations.SetHasWorldLocation(It.Value.InternalReplicationIndex, bHasWorldLocation);
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
	TArray<FTearOffInfo, TInlineAllocator<32>> ObjectsStillPendingTearOff;
	for (FTearOffInfo Info : MakeArrayView(HandlesPendingTearOff))
	{
		if (uint32 ObjectInternalIndex = NetHandleManager->GetInternalIndex(Info.Handle))
		{
			// Immediate tear-off or object that no longer are referenced by any connections are destroyed
			if (NetHandleManager->GetNetObjectRefCount(ObjectInternalIndex) == 0U || Info.bIsImmediate)
			{
				DestroyLocalNetHandle(Info.Handle);
			}
			else
			{
				// If the object is still in scope remove it from scope as objects being torn-off should not be added to new connections
				if (NetHandleManager->IsScopableIndex(ObjectInternalIndex))
				{
					// Mark object as no longer scopable, and that we should not propagate changed states
					NetHandleManager->RemoveFromScope(ObjectInternalIndex);
				}
			
				// Keep object in the pending tear-off list until the object is no longer referenced by any ReplicationWriter
				ObjectsStillPendingTearOff.Add(FTearOffInfo(Info.Handle, false));
			}
		}
	}

	HandlesPendingTearOff.Reset();
	HandlesPendingTearOff.Insert(ObjectsStillPendingTearOff.GetData(), ObjectsStillPendingTearOff.Num(), 0);
}

void UReplicationBridge::TearOff(FNetHandle Handle, bool bIsImmediate)
{
	if (!HandlesPendingTearOff.FindByPredicate([&](const FTearOffInfo& Info){ return Info.Handle == Handle; }))
	{
		HandlesPendingTearOff.Add(FTearOffInfo(Handle, bIsImmediate));
	}	
}

void UReplicationBridge::InternalTearOff(FNetHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(InternalTearOff);

	const uint32 InternalObjectIndex = NetHandleManager->GetInternalIndex(Handle);

	if (!InternalObjectIndex)
	{
		return;
	}

	FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex);

	// Copy state data and tear off now
	if (!ObjectData.bTearOff)
	{
		// Mark subobjects for TearOff as well.
		for (FInternalNetHandle SubObjectInternalIndex : NetHandleManager->GetChildSubObjects(InternalObjectIndex))
		{
			InternalTearOff(NetHandleManager->GetNetHandleFromInternalIndex(SubObjectInternalIndex));
		}	

		UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("TearOff %s"), *Handle.ToString());

		// Force copy of final state data as we will detach the object after scope update
		FChangeMaskCache ChangeMaskCache;
		FNetBitStreamWriter ChangeMaskWriter;

		// Setup context
		FNetSerializationContext SerializationContext;
		FInternalNetSerializationContext InternalContext(ReplicationSystem);
		SerializationContext.SetInternalContext(&InternalContext);

		if (EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll | EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
		{
			CallPreSendUpdateSingleHandle(Handle);
		} 

		if (FReplicationInstanceOperationsInternal::CopyObjectStateData(ChangeMaskWriter, ChangeMaskCache, *NetHandleManager, SerializationContext, InternalObjectIndex))
		{
			// Propagate changes to all connections that we currently have in scope
			FReplicationConnections& Connections = ReplicationSystem->GetReplicationSystemInternal()->GetConnections();

			// Iterate over connections and propagate dirty changemasks to all connections already scoping this object
			auto UpdateDirtyChangeMasks = [&Connections, &ChangeMaskCache](uint32 ConnectionId)
			{
				FReplicationConnection* Conn = Connections.GetConnection(ConnectionId);			
				Conn->ReplicationWriter->TearOffAndUpdateDirtyChangeMasks(ChangeMaskCache);
			};
			const FNetBitArray& ValidConnections = Connections.GetValidConnections();
			ValidConnections.ForAllSetBits(UpdateDirtyChangeMasks);		
		}

		// Mark object as being torn-off and that we should no longer propagate state changes
		ObjectData.bTearOff = 1U;
		ObjectData.bShouldPropagateChangedStates = 0U;
	}
}

UE::Net::FNetHandle UReplicationBridge::InternalAddDestructionInfo(FNetHandle Handle, const FEndReplicationParameters& Parameters)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FReplicationProtocolManager* ProtocolManager = GetReplicationProtocolManager();

	// Create DestructionInfo NetHandle used to scope destruction infos
	const FNetObjectGroupHandle LevelGroupHandle = GetLevelGroup(Parameters.Level);
	FNetHandle DestructionInfoHandle = NetHandleManager->CreateHandleForDestructionInfo(Handle, DestructionInfoProtocol);
	GetReplicationSystem()->AddToGroup(LevelGroupHandle, DestructionInfoHandle);

	const FInternalNetHandle InternalReplicationIndex = NetHandleManager->GetInternalIndex(DestructionInfoHandle);

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
		WorldLocations.SetHasWorldLocation(InternalReplicationIndex, true);
		WorldLocations.SetWorldLocation(InternalReplicationIndex, Parameters.Location);

		GetReplicationSystem()->SetPrioritizer(DestructionInfoHandle, DefaultSpatialNetObjectPrioritizerHandle);
	}

	UE_LOG_REPLICATIONBRIDGE(Verbose, TEXT("UReplicationBridge::InternalAddDestructionInfo %s ( InternalIndex: %u ) for %s GroupIndex: %u"), *DestructionInfoHandle.ToString(), InternalReplicationIndex,  *Handle.ToString(), LevelGroupHandle);
	
	return DestructionInfoHandle;
}

bool UReplicationBridge::IsReplicatedHandle(FNetHandle Handle) const
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
	if (ensure(LevelGroupHandle != InvalidNetObjectGroupHandle))
	{
		ReplicationSystem->AddGroupFilter(LevelGroupHandle);
		LevelGroups.Emplace(FObjectKey(Level), LevelGroupHandle);
	}

	return LevelGroupHandle;
}

UE::Net::FNetObjectGroupHandle UReplicationBridge::GetLevelGroup(const UObject* Level) const
{
	const UE::Net::FNetObjectGroupHandle* LevelGroupHandle = LevelGroups.Find(FObjectKey(Level));
	return (LevelGroupHandle != nullptr ? *LevelGroupHandle : UE::Net::InvalidNetObjectGroupHandle);
}
