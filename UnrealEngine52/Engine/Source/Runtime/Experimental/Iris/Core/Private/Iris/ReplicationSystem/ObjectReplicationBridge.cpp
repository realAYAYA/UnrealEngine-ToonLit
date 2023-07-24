// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

#include "HAL/IConsoleManager.h"
#include "Iris/IrisConfigInternal.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisMemoryTracker.h"
#include "Iris/Core/IrisProfiler.h"

#include "Net/Core/NetHandle/NetHandleManager.h"
#include "Net/Core/PropertyConditions/PropertyConditionsDelegates.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Trace/NetDebugName.h"

#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/ReplicationSystem/ObjectPollFrequencyLimiter.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridgeConfig.h"
#include "Iris/ReplicationSystem/Prioritization/NetObjectPrioritizer.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/ReplicationFragmentInternal.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamUtil.h"

#define UE_LOG_OBJECTREPLICATIONBRIDGE(Category, Format, ...)  UE_LOG(LogIrisBridge, Category, TEXT("ObjectReplicationBridge(%u)::") Format, GetReplicationSystem()->GetId(), ##__VA_ARGS__)

static bool bUseFrequencyBasedPolling = true;
static FAutoConsoleVariableRef CVarUseFrequencyBasedPolling(
		TEXT("net.Iris.UseFrequencyBasedPolling"),
		bUseFrequencyBasedPolling,
		TEXT("Whether to use frequency based polling or not. Default is true.")
		);

static bool bUseDormancyToFilterPolling = true;
static FAutoConsoleVariableRef CVarUseDormancyToFilterPolling(
		TEXT("net.Iris.UseDormancyToFilterPolling"),
		bUseDormancyToFilterPolling,
		TEXT("Whether we should use dormancy to filter out objects that we should not poll. Default is true.")
		);

static bool bAllowPollPeriodOverrides = true;
static FAutoConsoleVariableRef CVarAllowPollPeriodOverrides(
		TEXT("net.Iris.AllowPollPeriodOverrides"),
		bAllowPollPeriodOverrides,
		TEXT("Whether we allow poll period overrides set in ObjectReplicationBridgeConfig. Default is true.")
		);

static bool bEnableFilterMappings = true;
static FAutoConsoleVariableRef CVarEnableFilterMappings(
		TEXT("net.Iris.EnableFilterMappings"),
		bEnableFilterMappings,
		TEXT("Whether we honor filter mappings set in ObjectReplicationBridgeConfig. If filter mappings are enabled then objects may also be assigned the default spatial filter even if there aren't any specific mappings. Default is true.")
		);

UObjectReplicationBridge::FCreateNetRefHandleParams UObjectReplicationBridge::DefaultCreateNetRefHandleParams =
{
	0U, // bCanReceive
	0U, // bNeedsPreUpdate
	0U, // bNeedsWorldLocationUpdate
	1U, // bAllowDynamicFilter
	0.0f, // StaticPriority
	0U, // PollFramePeriod
};

namespace UE::Net::Private
{
	void CallRegisterReplicationFragments(UObject* Object, FFragmentRegistrationContext& Context, EFragmentRegistrationFlags RegistrationFlags)
	{
#if UE_WITH_IRIS
		Object->RegisterReplicationFragments(Context, RegistrationFlags);
#endif
	}

	struct FReplicationInstanceProtocolDeleter
	{
		void operator()(FReplicationInstanceProtocol* InstanceProtocol)
		{ 
			if (InstanceProtocol != nullptr)
			{
				FReplicationProtocolManager::DestroyInstanceProtocol(InstanceProtocol);
			}
		}
	};
	typedef TUniquePtr<FReplicationInstanceProtocol, FReplicationInstanceProtocolDeleter> FReplicationInstanceProtocolPtr;
}

UObjectReplicationBridge::UObjectReplicationBridge()
: Super()
, PollFrequencyLimiter(new UE::Net::Private::FObjectPollFrequencyLimiter())
, DefaultSpatialFilterHandle(UE::Net::InvalidNetObjectFilterHandle)
{
	SetShouldUseDefaultSpatialFilterFunction([](const UClass*){ return false; });
	SetShouldSubclassUseSameFilterFunction([](const UClass*,const UClass*){ return true; });
}

UObjectReplicationBridge::~UObjectReplicationBridge()
{
	delete PollFrequencyLimiter;
}

void UObjectReplicationBridge::Initialize(UReplicationSystem* InReplicationSystem)
{
	LLM_SCOPE_BYTAG(Iris);

	Super::Initialize(InReplicationSystem);

	const uint32 MaxActiveObjectCount = NetRefHandleManager->GetMaxActiveObjectCount();
	PollFrequencyLimiter->Init(MaxActiveObjectCount);
	ObjectsWithObjectReferences.Init(MaxActiveObjectCount);
	GarbageCollectionAffectedObjects.Init(MaxActiveObjectCount);

	LoadConfig();

	// Hookup delegate when a property custom conditions is changed
	OnCustomConditionChangedHandle = UE::Net::Private::FPropertyConditionDelegates::GetOnPropertyCustomConditionChangedDelegate().AddLambda([this](const UObject* Owner, uint16 RepIndex, bool bEnable)
	{
		using namespace UE::Net;
		using namespace UE::Net::Private;

		const FNetRefHandle RefHandle = this->GetReplicatedRefHandle(Owner);
		if (RefHandle.IsValid())
		{
			FReplicationSystemInternal* ReplicationSystemInternal = this->GetReplicationSystem()->GetReplicationSystemInternal();
			const FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();
			FReplicationConditionals& Conditionals = ReplicationSystemInternal->GetConditionals();

			Conditionals.SetPropertyCustomCondition(LocalNetRefHandleManager.GetInternalIndex(RefHandle), Owner, RepIndex, bEnable);
		}
	});
}

void UObjectReplicationBridge::Deinitialize()
{
	UE::Net::Private::FPropertyConditionDelegates::GetOnPropertyCustomConditionChangedDelegate().Remove(OnCustomConditionChangedHandle);
	OnCustomConditionChangedHandle.Reset();
	PollFrequencyLimiter->Deinit();
	Super::Deinitialize();
}

UObject* UObjectReplicationBridge::GetObjectFromReferenceHandle(FNetRefHandle RefHandle) const
{
	return GetObjectReferenceCache()->GetObjectFromReferenceHandle(RefHandle);
}

UObject* UObjectReplicationBridge::ResolveObjectReference(const UE::Net::FNetObjectReference& Reference, const UE::Net::FNetObjectResolveContext& ResolveContext)
{
	return GetObjectReferenceCache()->ResolveObjectReference(Reference, ResolveContext);
}

UE::Net::FNetObjectReference UObjectReplicationBridge::GetOrCreateObjectReference(const UObject* Instance) const
{
	return GetObjectReferenceCache()->GetOrCreateObjectReference(Instance);
}

UE::Net::FNetObjectReference UObjectReplicationBridge::GetOrCreateObjectReference(const FString& Path, const UObject* Outer) const
{
	return GetObjectReferenceCache()->GetOrCreateObjectReference(Path, Outer);
}

void UObjectReplicationBridge::AddStaticDestructionInfo(const FString& ObjectPath, const UObject* Outer, const FEndReplicationParameters& Parameters)
{
	UE::Net::FNetObjectReference ObjectRef = GetOrCreateObjectReference(ObjectPath, Outer);
	if (ObjectRef.IsValid())
	{
		InternalAddDestructionInfo(ObjectRef.GetRefHandle(), Parameters);
	}
}

UObject* UObjectReplicationBridge::GetReplicatedObject(FNetRefHandle Handle) const
{
	return IsReplicatedHandle(Handle) ? GetObjectFromReferenceHandle(Handle) : nullptr;
};

UE::Net::FNetRefHandle UObjectReplicationBridge::GetReplicatedRefHandle(const UObject* Object) const
{
	FNetRefHandle Handle = GetObjectReferenceCache()->GetObjectReferenceHandleFromObject(Object);

	return IsReplicatedHandle(Handle) ? Handle : FNetRefHandle();
}

UE::Net::FNetRefHandle UObjectReplicationBridge::GetReplicatedRefHandle(FNetHandle Handle) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	// If the object is replicated by the owning ReplicationSystem the internal handle should be valid.
	FInternalNetRefIndex ObjectInternalIndex = NetRefHandleManager->GetInternalIndexFromNetHandle(Handle);
	if (ObjectInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
	{
		return FNetRefHandle();
	}

	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectInternalIndex);
	return ObjectData.RefHandle;
}

UE::Net::FNetRefHandle UObjectReplicationBridge::BeginReplication(UObject* Instance, const FCreateNetRefHandleParams& Params)
{
	LLM_SCOPE_BYTAG(IrisState);

	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetRefHandle AllocatedRefHandle = ObjectReferenceCache->CreateObjectReferenceHandle(Instance);

	// If we failed to assign a handle, or if the Handle already is replicating, just return the handle
	if (!AllocatedRefHandle.IsValid())
	{
		return FNetRefHandle();
	}

	if (IsReplicatedHandle(AllocatedRefHandle))
	{
		return AllocatedRefHandle;
	}

	// Register fragments
	EReplicationFragmentTraits Traits = EReplicationFragmentTraits::CanReplicate;
	Traits |= Params.bCanReceive ? EReplicationFragmentTraits::CanReceive : EReplicationFragmentTraits::None;
	Traits |= Params.bNeedsPreUpdate ? EReplicationFragmentTraits::NeedsPreSendUpdate : EReplicationFragmentTraits::None;
	Traits |= Params.bNeedsWorldLocationUpdate ? EReplicationFragmentTraits::NeedsWorldLocationUpdate : EReplicationFragmentTraits::None;
	
	FFragmentRegistrationContext FragmentRegistrationContext(GetReplicationStateDescriptorRegistry(), Traits);

	// For everything derived from UObject we can call the virtual function RegisterReplicationFragments	
	CallRegisterReplicationFragments(Instance, FragmentRegistrationContext, EFragmentRegistrationFlags::None);

	const FReplicationFragments& RegisteredFragments = FFragmentRegistrationContextPrivateAccessor::GetReplicationFragments(FragmentRegistrationContext);

	// We currently identify protocols by local archetype or CDO pointer and verified the protocol id received from server and the hash of the default state
	const UObject* ArchetypeOrCDOUsedAsKey = Instance->GetArchetype();

	// Create Protocols
	FReplicationProtocolManager* ProtocolManager = GetReplicationProtocolManager();

	FReplicationInstanceProtocolPtr InstanceProtocol(ProtocolManager->CreateInstanceProtocol(RegisteredFragments));
		
	FReplicationProtocolIdentifier ProtocolIdentifier = ProtocolManager->CalculateProtocolIdentifier(RegisteredFragments);
	const FReplicationProtocol* ReplicationProtocol = ProtocolManager->GetReplicationProtocol(ProtocolIdentifier, ArchetypeOrCDOUsedAsKey);
	if (!ReplicationProtocol)
	{
		ReplicationProtocol = ProtocolManager->CreateReplicationProtocol(ArchetypeOrCDOUsedAsKey, ProtocolIdentifier, RegisteredFragments, *Instance->GetClass()->GetName(), false);
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	else
	{
		const bool bIsValidProtocol = ProtocolManager->ValidateReplicationProtocol(ReplicationProtocol, RegisteredFragments);
		if (!bIsValidProtocol)
		{
			UE_LOG_OBJECTREPLICATIONBRIDGE(Error, TEXT("BeginReplication Found invalid protocol ProtocolId:0x%" UINT64_x_FMT " for Object named %s"), ReplicationProtocol->ProtocolIdentifier, *Instance->GetName());
			return FNetRefHandle();
		}
	}
#endif
		
	if (ReplicationProtocol)
	{
		// Create NetHandle and bind instance
		FNetHandle NetHandle = FNetHandleManager::GetOrCreateNetHandle(Instance);
		FNetRefHandle RefHandle = InternalCreateNetObject(AllocatedRefHandle, NetHandle, ReplicationProtocol);
		if (RefHandle.IsValid())
		{
			// Attach the instance and bind the instance protocol to dirty tracking
			constexpr bool bBindInstanceProtocol = true;
			InternalAttachInstanceToNetRefHandle(RefHandle, bBindInstanceProtocol, InstanceProtocol.Get(), Instance, NetHandle);
#if WITH_PUSH_MODEL
			SetNetPushIdOnInstance(InstanceProtocol.Get(), NetHandle);
#endif

			const FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(RefHandle);

			// Initialize conditionals
			FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
			ReplicationSystemInternal->GetConditionals().InitPropertyCustomConditions(InternalReplicationIndex);

			// Keep track of handles with object references for garbage collection's sake.
			ObjectsWithObjectReferences.SetBitValue(InternalReplicationIndex, EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::HasObjectReference));

			// Set poll frame period
			uint8 PollFramePeriod = Params.PollFramePeriod;
			FindPollInfo(Instance->GetClass(), PollFramePeriod);
			SetPollFramePeriod(InternalReplicationIndex, PollFramePeriod);

			UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("BeginReplication Created %s with ProtocolId:0x%" UINT64_x_FMT " for Object named %s"), *RefHandle.ToString(), ReplicationProtocol->ProtocolIdentifier, *Instance->GetName());

			{
				FWorldLocations& WorldLocations = ReplicationSystem->GetReplicationSystemInternal()->GetWorldLocations();
				WorldLocations.SetHasWorldLocation(InternalReplicationIndex, Params.bNeedsWorldLocationUpdate);
			}

			// Set prioritizer
			if (Params.StaticPriority > 0.0f)
			{
				ReplicationSystem->SetStaticPriority(RefHandle, Params.StaticPriority);
			}
			else
			{
				const FNetObjectPrioritizerHandle PrioritizerHandle = GetPrioritizer(Instance->GetClass());
				if (PrioritizerHandle != InvalidNetObjectPrioritizerHandle)
				{
					ReplicationSystem->SetPrioritizer(RefHandle, PrioritizerHandle);
				}
				else if (Params.bNeedsWorldLocationUpdate || HasRepTag(ReplicationProtocol, RepTag_WorldLocation))
				{
					ReplicationSystem->SetPrioritizer(RefHandle, DefaultSpatialNetObjectPrioritizerHandle);
				}
			}

			if (bEnableFilterMappings && Params.bAllowDynamicFilter)
			{
				const FNetObjectFilterHandle FilterHandle = GetDynamicFilter(Instance->GetClass());
				if (FilterHandle != InvalidNetObjectFilterHandle)
				{
					ReplicationSystem->SetFilter(RefHandle, FilterHandle);
				}
			}

			if (ShouldClassBeDeltaCompressed(Instance->GetClass()))
			{
				ReplicationSystem->SetDeltaCompressionStatus(RefHandle, ENetObjectDeltaCompressionStatus::Allow);
			}

			// Release instance protocol from the uniquePtr as it is now successfully bound to the handle
			InstanceProtocol.Release();

			return RefHandle;
		}
		UE_LOG_OBJECTREPLICATIONBRIDGE(Warning, TEXT("BeginReplication Failed to create NetRefHandle with ProtocolId:0x%" UINT64_x_FMT " for Object named %s"), (ReplicationProtocol != nullptr ? ReplicationProtocol->ProtocolIdentifier : FReplicationProtocolIdentifier(0)), *Instance->GetName());
	}

	// If we get here, it means that we failed to assign an internal handle for the object. We've probably run out of handles which currently is a fatal error.
	UE_LOG(LogIris, Error, TEXT("UObjectReplicationBridge::BeginReplication - Failed to create NetRefHandle for object %s"), ToCStr(Instance->GetPathName()));

	return FNetRefHandle();
}

UE::Net::FNetRefHandle UObjectReplicationBridge::BeginReplication(FNetRefHandle OwnerRefHandle, UObject* Instance, FNetRefHandle InsertRelativeToSubObjectRefHandle, ESubObjectInsertionOrder InsertionOrder, const FCreateNetRefHandleParams& Params)
{
	LLM_SCOPE_BYTAG(IrisState);

	using namespace UE::Net;
	using namespace UE::Net::Private;

	const FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	const FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();

	// Owner must be replicated
	check(IsReplicatedHandle(OwnerRefHandle));
	// Verify assumptions
	check(!Instance->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject));

	FNetRefHandle SubObjectRefHandle = GetReplicatedRefHandle(Instance);
	if (SubObjectRefHandle.IsValid())
	{
		// Verify that the existing object is a subobject of the owner
		check(OwnerRefHandle == LocalNetRefHandleManager.GetSubObjectOwner(SubObjectRefHandle));
		return SubObjectRefHandle;
	}
	else
	{
		FCreateNetRefHandleParams SubObjectCreateParams = Params;
		// The filtering system ignores subobjects so let's not waste cycles figuring out which filter to use.
		SubObjectCreateParams.bAllowDynamicFilter = 0U;
		SubObjectRefHandle = BeginReplication(Instance, SubObjectCreateParams);
	}

	if (SubObjectRefHandle.IsValid())
	{
		// Add subobject
		UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("BeginReplication Added SubObject %s to Owner %s RelativeToSubObjectHandle %s"), *SubObjectRefHandle.ToString(), *OwnerRefHandle.ToString(), *InsertRelativeToSubObjectRefHandle.ToString());

		InternalAddSubObject(OwnerRefHandle, SubObjectRefHandle, InsertRelativeToSubObjectRefHandle, InsertionOrder);

		// SubObjects should always poll with owner
		SetPollWithObject(OwnerRefHandle, SubObjectRefHandle);

		// Copy pending dormancy from owner
		SetObjectWantsToBeDormant(SubObjectRefHandle, GetObjectWantsToBeDormant(OwnerRefHandle));
	
		return SubObjectRefHandle;
	}

	return FNetRefHandle();
}

void UObjectReplicationBridge::SetSubObjectNetCondition(FNetRefHandle SubObjectRefHandle, ELifetimeCondition Condition)
{
	using namespace UE::Net::Private;

	// We assume that we can store the condition in an int8;
	static_assert(ELifetimeCondition::COND_Max <= 127);

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();

	const FInternalNetRefIndex SubObjectInternalIndex = LocalNetRefHandleManager.GetInternalIndex(SubObjectRefHandle);
	if (LocalNetRefHandleManager.SetSubObjectNetCondition(SubObjectInternalIndex, (int8)Condition))
	{
		UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("SetSubObjectNetCondition for SubObject %s Condition %s"), *SubObjectRefHandle.ToString(), *UEnum::GetValueAsString<ELifetimeCondition>(Condition));
		MarkNetObjectStateDirty(ReplicationSystem->GetId(), SubObjectInternalIndex);
	}
	else
	{
		UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("Failed to SetSubObjectNetCondition for SubObject %s Condition %s"), *SubObjectRefHandle.ToString(), *UEnum::GetValueAsString<ELifetimeCondition>(Condition));
	}
}

void UObjectReplicationBridge::AddDependentObject(FNetRefHandle ParentHandle, FNetRefHandle DependentHandle, UE::Net::EDependentObjectSchedulingHint SchedulingHint)
{
	using namespace UE::Net::Private;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();

	if (LocalNetRefHandleManager.AddDependentObject(ParentHandle, DependentHandle, SchedulingHint))
	{
		FReplicationFiltering& Filtering = ReplicationSystemInternal->GetFiltering();
		const FInternalNetRefIndex DependentInternalIndex = LocalNetRefHandleManager.GetInternalIndex(DependentHandle);
		Filtering.NotifyAddedDependentObject(DependentInternalIndex);

		UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("AddDependentObject Added dependent object %s to parent %s"), *DependentHandle.ToString(), *ParentHandle.ToString());
	}
	else
	{
		UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("AddDependentObject Failed to add dependent object %s to parent %s"), *DependentHandle.ToString(), *ParentHandle.ToString());
	}
}

void UObjectReplicationBridge::RemoveDependentObject(FNetRefHandle ParentHandle, FNetRefHandle DependentHandle)
{
	using namespace UE::Net::Private;

	UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("RemoveDependentObject Removing dependent object %s from parent %s"), *DependentHandle.ToString(), *ParentHandle.ToString());

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();

	// Remove dependent object
	FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();
	LocalNetRefHandleManager.RemoveDependentObject(ParentHandle, DependentHandle);

	const FInternalNetRefIndex DependentInternalIndex = LocalNetRefHandleManager.GetInternalIndex(DependentHandle);
	if (DependentInternalIndex != FNetRefHandleManager::InvalidInternalIndex)
	{
		FReplicationFiltering& Filtering = ReplicationSystemInternal->GetFiltering();
		Filtering.NotifyRemovedDependentObject(DependentInternalIndex);
	}
}

bool UObjectReplicationBridge::WriteNetRefHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetRefHandle Handle)
{
	// Write ProtocolId
	const UE::Net::FReplicationProtocol* Protocol = GetReplicationSystem()->GetReplicationProtocol(Handle);
	WriteUint64(Context.SerializationContext.GetBitStreamWriter(), Protocol->ProtocolIdentifier);

	// Write Type header
	return WriteCreationHeader(Context.SerializationContext, Handle);
}

void UObjectReplicationBridge::EndReplication(UObject* Instance, EEndReplicationFlags EndReplicationFlags, FEndReplicationParameters* Parameters)
{
	const FNetRefHandle Handle = GetReplicatedRefHandle(Instance);
	UReplicationBridge::EndReplication(Handle, EndReplicationFlags, Parameters);
}

void UObjectReplicationBridge::DetachInstanceFromRemote(FNetRefHandle Handle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("DetachInstanceFromRemote %s DestroyReason: %s DestroyFlags: %u"), *Handle.ToString(), ToCStr(LexToString(DestroyReason)), unsigned(DestroyFlags));
	
	UObject* Instance = GetObjectFromReferenceHandle(Handle);
	
	UnregisterInstance(Handle);

	// Destroy instance if requested
	if (Instance && DestroyReason != EReplicationBridgeDestroyInstanceReason::DoNotDestroy)
	{
		DestroyInstanceFromRemote(Instance, DestroyReason, DestroyFlags);
	}

	// $IRIS TODO: Cleanup any pending creation data if we have not yet instantiated the instance.
}

void UObjectReplicationBridge::DetachInstance(FNetRefHandle RefHandle)
{
	UnregisterInstance(RefHandle);
}

void UObjectReplicationBridge::UnregisterInstance(FNetRefHandle RefHandle)
{
	if (RefHandle.IsDynamic())
	{
		UObject* Instance = GetObjectFromReferenceHandle(RefHandle);
		GetObjectReferenceCache()->RemoveReference(RefHandle, Instance);
	}
}

void UObjectReplicationBridge::RegisterRemoteInstance(FNetRefHandle RefHandle, UObject* Instance, const UE::Net::FReplicationProtocol* Protocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, const FCreationHeader* Header, uint32 ConnectionId)
{
	// Attach the instance protocol and instance to the handle
	constexpr bool bBindInstanceProtocol = false;
	InternalAttachInstanceToNetRefHandle(RefHandle, bBindInstanceProtocol, InstanceProtocol, Instance, FNetHandle());

	// Dynamic references needs to be promoted to find the instantiated object
	if (RefHandle.IsDynamic())
	{
		GetObjectReferenceCache()->AddRemoteReference(RefHandle, Instance);
	}

	UE_LOG_OBJECTREPLICATIONBRIDGE(Verbose, TEXT("RegisterRemoteInstance %s %s with ProtocolId:0x%" UINT64_x_FMT), *RefHandle.ToString(), ToCStr(Instance->GetName()), Protocol->ProtocolIdentifier);
}

FReplicationBridgeCreateNetRefHandleResult UObjectReplicationBridge::CreateNetRefHandleFromRemote(FNetRefHandle SubObjectOwnerNetHandle, FNetRefHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context)
{
	LLM_SCOPE_BYTAG(IrisState);

	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetBitStreamReader* Reader = Context.SerializationContext.GetBitStreamReader();

	FReplicationProtocolIdentifier ReceivedProtocolId = ReadUint64(Context.SerializationContext.GetBitStreamReader());

	// Read creation header
	TUniquePtr<FCreationHeader> Header(ReadCreationHeader(Context.SerializationContext));
	if (Context.SerializationContext.HasErrorOrOverflow())
	{
		return FReplicationBridgeCreateNetRefHandleResult();
	}

	// Currently remote objects can only receive replicated data
	FFragmentRegistrationContext FragmentRegistrationContext(GetReplicationStateDescriptorRegistry(), EReplicationFragmentTraits::CanReceive);
	FReplicationProtocolManager* ProtocolManager = GetReplicationProtocolManager();

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bVerifyExistingProtocol = false;
#else
	const bool bVerifyExistingProtocol = true;
#endif

	FReplicationBridgeCreateNetRefHandleResult CreateResult;

	// Currently we need to always instantiate remote objects, moving forward we want to make this optional so that can be deferred until it is time to apply received state data.
	// https://jira.it.epicgames.com/browse/UE-127369	
	FObjectReplicationBridgeInstantiateResult InstantiateResult = BeginInstantiateFromRemote(SubObjectOwnerNetHandle, Context.SerializationContext.GetInternalContext()->ResolveContext, Header.Get());
	UObject* InstancePtr = InstantiateResult.Object;
	if (!ensureAlwaysMsgf(InstancePtr, TEXT("Failed to instantiate Handle: %s"), *WantedNetHandle.ToString()))
	{
		return CreateResult;
	}

	CreateResult.Flags |= InstantiateResult.Flags;

	// Register all fragments
	CallRegisterReplicationFragments(InstancePtr, FragmentRegistrationContext, EFragmentRegistrationFlags::None);

	const FReplicationFragments& RegisteredFragments = FFragmentRegistrationContextPrivateAccessor::GetReplicationFragments(FragmentRegistrationContext);

	// We currently identify protocols by local archetype or CDO pointer and verified the protocol id received from server
	// We also should verify the default state that we use for delta compression
	const UObject* ArchetypeOrCDOUsedAsKey = InstancePtr->GetArchetype();

	// Create Protocols
	FReplicationInstanceProtocolPtr InstanceProtocol(ProtocolManager->CreateInstanceProtocol(RegisteredFragments));

	// See if the protocol already is known
	const FReplicationProtocol* ReplicationProtocol = ProtocolManager->GetReplicationProtocol(ReceivedProtocolId, ArchetypeOrCDOUsedAsKey);
	if (!ReplicationProtocol)
	{
		ReplicationProtocol = ProtocolManager->CreateReplicationProtocol(ArchetypeOrCDOUsedAsKey, ReceivedProtocolId, RegisteredFragments, *(InstancePtr->GetClass()->GetName()), true);
	}
	else
	{
		if (!ensureAlways(!bVerifyExistingProtocol || ProtocolManager->ValidateReplicationProtocol(ReplicationProtocol, RegisteredFragments)))
		{
			return CreateResult;
		}
	}

	if (ReplicationProtocol)
	{
		// Create NetHandle
		FNetRefHandle Handle = InternalCreateNetObjectFromRemote(WantedNetHandle, ReplicationProtocol);
		CreateResult.NetRefHandle = Handle;
		if (Handle.IsValid())
		{
			RegisterRemoteInstance(Handle, InstancePtr, ReplicationProtocol, InstanceProtocol.Get(), Header.Get(), Context.ConnectionId);

			// Release instance protocol from the uniquePtr as it is now successfully bound to the handle
			InstanceProtocol.Release();

			// Now it is safe to issue OnActorChannelOpen callback
			ensureAlwaysMsgf(OnInstantiatedFromRemote(InstancePtr, Header.Get(), Context.ConnectionId), TEXT("Failed to invoke OnInstantiatedFromRemote for Instance named %s %s"), *InstancePtr->GetName(), *Handle.ToString());
		}
	}

	return CreateResult;
}

void UObjectReplicationBridge::PostApplyInitialState(FNetRefHandle Handle)
{
	EndInstantiateFromRemote(Handle);
}

void UObjectReplicationBridge::PreSendUpdateSingleHandle(FNetRefHandle RefHandle)
{
	PreUpdateAndPollImpl(RefHandle);
}

void UObjectReplicationBridge::PreSendUpdate()
{
	IRIS_PROFILER_SCOPE(UObjectReplicationBridge_OnPreSendUpdate);

	// Invalid/default handle means update all objects
	FNetRefHandle RefHandle;
	PreUpdateAndPollImpl(RefHandle);
}


void UObjectReplicationBridge::PruneStaleObjects()
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(UObjectReplicationBridge_PruneStaleObjects);

	// Mark all objects with object references as potentially affected by GC
	GarbageCollectionAffectedObjects = ObjectsWithObjectReferences;

	FNetRefHandleManager& LocalNetRefHandleManager = GetReplicationSystem()->GetReplicationSystemInternal()->GetNetRefHandleManager();
	const TArray<UObject*>& ReplicatedInstances = LocalNetRefHandleManager.GetReplicatedInstances();

	TArray<FNetRefHandle> StaleObjects;

	// Detect stale references and try to kill/report them
	auto DetectStaleObjectsFunc = [&LocalNetRefHandleManager, &StaleObjects, &ReplicatedInstances](uint32 InternalNetHandleIndex)
	{
		if (ReplicatedInstances[InternalNetHandleIndex] == nullptr)
		{
			const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalNetHandleIndex);
			if (ObjectData.InstanceProtocol)
			{
				const FReplicationProtocol* Protocol = ObjectData.Protocol;
				const FNetDebugName* DebugName = Protocol ? Protocol->DebugName : nullptr;
				UE_LOG(LogIrisBridge, Warning, TEXT("UObjectReplicationBridge::PruneStaleObjects ObjectInstance replicated as: %s of Type named:%s has been destroyed without notifying the ReplicationSystem %u"), *ObjectData.RefHandle.ToString(), ToCStr(DebugName), ObjectData.RefHandle.GetReplicationSystemId());

				// If the instance protocol is bound, then this is an error and we cannot safely cleanup as unbinding abound instance protocol will modify bound states
				checkf(!EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::IsBound), TEXT("UObjectReplicationBridge::PruneStaleObjects Bound ObjectInstance replicated as: %s has been destroyed without notifying the ReplicationSystem."), *ObjectData.RefHandle.ToString(), ToCStr(DebugName), ObjectData.RefHandle.GetReplicationSystemId());

				StaleObjects.Push(ObjectData.RefHandle);
			}
		}
	};

	// Iterate over assigned indices and detect if any of the replicated instances has been garbagecollected (excluding DestroyedStartupObjectInternalIndices) as they never have an instance
	FNetBitArray::ForAllSetBits(LocalNetRefHandleManager.GetAssignedInternalIndices(), LocalNetRefHandleManager.GetDestroyedStartupObjectInternalIndices(), FNetBitArray::AndNotOp, DetectStaleObjectsFunc);

	// EndReplication/detach stale instances
	for (FNetRefHandle Handle : MakeArrayView(StaleObjects.GetData(), StaleObjects.Num()))
	{
		UReplicationBridge::EndReplication(Handle);
	}

	// Mark poll override info as dirty
	bHasDirtyClassesInPollPeriodOverrides = ClassHierarchyPollPeriodOverrides.Num() > 0;
}

void UObjectReplicationBridge::SetInstancePreUpdateFunction(FInstancePreUpdateFunction InPreUpdateFunction)
{
	PreUpdateInstanceFunction = InPreUpdateFunction;
}

void UObjectReplicationBridge::SetInstanceGetWorldLocationFunction(FInstanceGetWorldLocationFunction InGetWorldLocationFunction)
{
	GetInstanceWorldLocationFunction = InGetWorldLocationFunction;
}

void UObjectReplicationBridge::PreUpdateAndPollImpl(FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	const FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();
	const TArray<UObject*>& ReplicatedInstances = LocalNetRefHandleManager.GetReplicatedInstances();
	const bool bIsUsingPushModel = IsIrisPushModelEnabled();
	FNetBitArrayView DirtyObjects = ReplicationSystemInternal->GetDirtyNetObjectTracker().GetDirtyNetObjects();

	struct FPreUpdateAndPollStats
	{
		uint32 PreUpdatedObjectCount = 0;
		uint32 PolledObjectCount = 0;
		uint32 PolledReferencesObjectCount = 0;
	};

	FPreUpdateAndPollStats Stats;

	auto UpdateAndPollFunction = [&ReplicatedInstances, this, &LocalNetRefHandleManager, &DirtyObjects, &Stats](uint32 InternalObjectIndex)
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);
		if (ObjectData.InstanceProtocol)
		{
			// Call per-instance PreUpdate function
			if (PreUpdateInstanceFunction && EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				IRIS_PROFILER_SCOPE(PreReplicationUpdate);
				IRIS_PROFILER_SCOPE_TEXT(ObjectData.Protocol->DebugName->Name);
#endif
				(*PreUpdateInstanceFunction)(ObjectData.RefHandle, ReplicatedInstances[InternalObjectIndex], this);
				++Stats.PreUpdatedObjectCount;
			}

			// Poll properties if the instance protocol requires it
			if (EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll))
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				IRIS_PROFILER_SCOPE(PreReplicationUpdate);
				IRIS_PROFILER_SCOPE_TEXT(ObjectData.Protocol->DebugName->Name);
#endif

				const bool bIsGCAffectedObject = GarbageCollectionAffectedObjects.GetBit(InternalObjectIndex);
				GarbageCollectionAffectedObjects.ClearBit(InternalObjectIndex);

				// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references
				EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
				PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

				const bool bWasMarkedDirty = FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(ObjectData.InstanceProtocol, PollOptions);
				if (bWasMarkedDirty)
				{
					DirtyObjects.SetBit(InternalObjectIndex);
				}
				++Stats.PolledObjectCount;
			}
		}
	};

	const FNetBitArray& PrevScopableObjects = LocalNetRefHandleManager.GetPrevFrameScopableInternalIndices();

	auto PushModelUpdateAndPollFunction = [&ReplicatedInstances, this, &LocalNetRefHandleManager, &DirtyObjects, &PrevScopableObjects, &Stats](uint32 InternalObjectIndex)
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);
		if (const FReplicationInstanceProtocol* InstanceProtocol = ObjectData.InstanceProtocol)
		{
			const EReplicationInstanceProtocolTraits InstanceTraits = InstanceProtocol->InstanceTraits;
			// Call per-instance PreUpdate function
			if (PreUpdateInstanceFunction && EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPreSendUpdate))
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				IRIS_PROFILER_SCOPE(PreReplicationUpdate);
				IRIS_PROFILER_SCOPE_TEXT(ObjectData.Protocol->DebugName->Name);
#endif
				(*PreUpdateInstanceFunction)(ObjectData.RefHandle, ReplicatedInstances[InternalObjectIndex], this);
				++Stats.PreUpdatedObjectCount;
			}

			const bool bIsDirtyObject = DirtyObjects.GetBit(InternalObjectIndex);
			const bool bIsNewInScope = !PrevScopableObjects.GetBit(InternalObjectIndex);
			const bool bIsGCAffectedObject = GarbageCollectionAffectedObjects.GetBit(InternalObjectIndex);			
			GarbageCollectionAffectedObjects.ClearBit(InternalObjectIndex);

			// Early out if the instance does not require polling
			if (!EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::NeedsPoll))
			{
				return;
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			IRIS_PROFILER_SCOPE(PollPushBased);
			IRIS_PROFILER_SCOPE_TEXT(ObjectData.Protocol->DebugName->Name);
#endif

			// If the object is fully push model we only need to poll it if it's dirty, unless it's a new object or was garbage collected.
			bool bWasMarkedDirty = false;
			if (EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
			{
				if (bIsDirtyObject | bIsNewInScope)
				{
					// We need to do a poll if object is marked as dirty
					EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
					PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;
					bWasMarkedDirty = FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(InstanceProtocol, EReplicationFragmentTraits::None, PollOptions);
					++Stats.PolledObjectCount;
				}
				else if (bIsGCAffectedObject)
				{
					// If this object might have been affected by GC, only refresh cached references
					const EReplicationFragmentTraits RequiredTraits = EReplicationFragmentTraits::HasPushBasedDirtiness;
					bWasMarkedDirty = FReplicationInstanceOperations::PollAndRefreshCachedObjectReferences(InstanceProtocol, RequiredTraits);
					++Stats.PolledReferencesObjectCount;
				}
			}
			else
			{
				// If the object has pushed based fragments, and not is marked dirty and object is affected by GC we need to make sure that we refresh cached references for all push based fragments
				const bool bIsPushBasedObject = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness);
				const bool bHasObjectReferences = EnumHasAnyFlags(InstanceTraits, EReplicationInstanceProtocolTraits::HasObjectReference);
				const bool bNeedsRefreshOfCachedObjectReferences = ((!(bIsNewInScope | bIsDirtyObject)) & bIsGCAffectedObject & bIsPushBasedObject & bHasObjectReferences);
				if (bNeedsRefreshOfCachedObjectReferences)
				{
					// Only states which has push based dirtiness need to be updated as the other states will be polled in full anyway.
					const EReplicationFragmentTraits RequiredTraits = EReplicationFragmentTraits::HasPushBasedDirtiness;
					bWasMarkedDirty = FReplicationInstanceOperations::PollAndRefreshCachedObjectReferences(InstanceProtocol, RequiredTraits);
					++Stats.PolledReferencesObjectCount;
				}

				// If this object has been around for a garbage collect and it has object references we must make sure that we update all cached object references 
				EReplicationFragmentPollFlags PollOptions = EReplicationFragmentPollFlags::PollAllState;
				PollOptions |= bIsGCAffectedObject ? EReplicationFragmentPollFlags::ForceRefreshCachedObjectReferencesAfterGC : EReplicationFragmentPollFlags::None;

				// If the object is not new or dirty at this point we only need to poll non-push based fragments as we know that pushed based states have not been modified
				const EReplicationFragmentTraits ExcludeTraits = (bIsDirtyObject | bIsNewInScope) ? EReplicationFragmentTraits::None : EReplicationFragmentTraits::HasPushBasedDirtiness;
				bWasMarkedDirty |= FReplicationInstanceOperations::PollAndRefreshCachedPropertyData(InstanceProtocol, ExcludeTraits, PollOptions);
				++Stats.PolledObjectCount;
			}

			if (bWasMarkedDirty)
			{
				DirtyObjects.SetBit(InternalObjectIndex);
			}
		}
	};

	// Update all objects
	if (!Handle.IsValid())
	{
		const FNetBitArray& ScopableObjects = LocalNetRefHandleManager.GetScopableInternalIndices();
		const FNetBitArray& WantToBeDormantObjects = LocalNetRefHandleManager.GetWantToBeDormantInternalIndices();

		// Filter the set of objects considered for pre-update and polling
		// We always want to consider objects marked as dirty and their subobjects
		FNetBitArray ObjectsConsideredForPolling(LocalNetRefHandleManager.GetMaxActiveObjectCount());
		if (bUseFrequencyBasedPolling)
		{
			FNetBitArrayView FrequencyBasedObjectsToPollView = MakeNetBitArrayView(ObjectsConsideredForPolling);
			PollFrequencyLimiter->Update(MakeNetBitArrayView(ScopableObjects), DirtyObjects, FrequencyBasedObjectsToPollView);
		}
		else
		{
			ObjectsConsideredForPolling.Copy(ScopableObjects);
		}

		// Mask off objects pending dormancy as we do not want to poll/pre-update them unless they are marked for flush or are dirty
		if (bUseDormancyToFilterPolling)
		{
			IRIS_PROFILER_SCOPE(UObjectReplicationBridge_PreUpdateAndPollImpl_Dormancy);

			// Mask off objects pending dormancy, masked off dirty objects are restored later
			ObjectsConsideredForPolling.Combine(WantToBeDormantObjects, FNetBitArrayView::AndNotOp);

			// Add objects that have requested to flush dormancy
			for (FNetRefHandle HandlePendingFlush : MakeArrayView(DormantHandlesPendingFlush))
			{
				if (const uint32 InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(HandlePendingFlush))
				{
					if (ObjectsConsideredForPolling.GetBit(InternalObjectIndex))
					{
						continue;
					}

					ObjectsConsideredForPolling.SetBit(InternalObjectIndex);
					for (const FInternalNetRefIndex SubObjectIndex : LocalNetRefHandleManager.GetSubObjects(InternalObjectIndex))
					{
						ObjectsConsideredForPolling.SetBit(SubObjectIndex);
					}
				}
			}
		}
		DormantHandlesPendingFlush.Reset();

		/**
		 * Make sure to propagate polling for owners to subobjects and vice versa. If an actor is not due to update due to
		 * polling frequency it can still be dirty or a dormant object marked for flush and polled for that reason. In order to make sure all recent state updates
		 * are replicated atomically this polling propagation is required.
		 */
		{
			IRIS_PROFILER_SCOPE(UObjectReplicationBridge_PreUpdateAndPollImpl_PropagateDirtyness);

			auto PropagateSubObjectDirtinessToOwner = [&LocalNetRefHandleManager, &ObjectsConsideredForPolling](uint32 InternalObjectIndex)
			{
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);
				ObjectsConsideredForPolling.SetBit(ObjectData.SubObjectRootIndex);
			};

			auto PropagateOwnerDirtinessToSubObjectsAndDependentObjects = [&LocalNetRefHandleManager, &ObjectsConsideredForPolling](uint32 InternalObjectIndex)
			{
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);

				// Mark owner as well as we might have masked it out in the dormancy pass
				ObjectsConsideredForPolling.SetBit(InternalObjectIndex);

				for (const FInternalNetRefIndex SubObjectInternalIndex : LocalNetRefHandleManager.GetSubObjects(InternalObjectIndex))
				{
					ObjectsConsideredForPolling.SetBit(SubObjectInternalIndex);
				}
			};

			// Update subobjects' owner first and owners' subobjects second. It's the only way to properly mark all groups of objects in two passes.
			const FNetBitArrayView SubObjects = MakeNetBitArrayView(LocalNetRefHandleManager.GetSubObjectInternalIndices());
			FNetBitArrayView::ForAllSetBits(DirtyObjects, SubObjects, FNetBitArray::AndOp, PropagateSubObjectDirtinessToOwner);
			FNetBitArrayView::ForAllSetBits(DirtyObjects, SubObjects, FNetBitArray::AndNotOp, PropagateOwnerDirtinessToSubObjectsAndDependentObjects);
			
			// Patch in dependent objects, not subtle as this point
			{
				IRIS_PROFILER_SCOPE(UObjectReplicationBridge_PreUpdateAndPollImpl Patch in depedent objects);

				FNetBitArray TempObjectsConsideredForPolling(ObjectsConsideredForPolling);
				FNetBitArray::ForAllSetBits(TempObjectsConsideredForPolling, NetRefHandleManager->GetObjectsWithDependentObjectsInternalIndices(), FNetBitArray::AndOp,
					[&LocalNetRefHandleManager, &ObjectsConsideredForPolling](FInternalNetRefIndex ObjectIndex) 
					{
						LocalNetRefHandleManager.ForAllDependentObjectsRecursive(ObjectIndex, [&ObjectsConsideredForPolling, ObjectIndex](FInternalNetRefIndex DependentObjectIndex) { ObjectsConsideredForPolling.SetBit(DependentObjectIndex); });
					}
				);
			}
		}

		if (IsIrisPushModelEnabled())
		{
			ObjectsConsideredForPolling.ForAllSetBits(PushModelUpdateAndPollFunction);
		}
		else
		{
			ObjectsConsideredForPolling.ForAllSetBits(UpdateAndPollFunction);
		}
	}
	else if (uint32 InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(Handle))
	{
		UpdateAndPollFunction(InternalObjectIndex);
	}

	// Report stats
	UE_NET_TRACE_FRAME_STATSCOUNTER(ReplicationSystem->GetId(), ReplicationSystem.PreUpdatedObjectCount, Stats.PreUpdatedObjectCount, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_FRAME_STATSCOUNTER(ReplicationSystem->GetId(), ReplicationSystem.PolledObjectCount, Stats.PolledObjectCount, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_FRAME_STATSCOUNTER(ReplicationSystem->GetId(), ReplicationSystem.PolledReferencesObjectCount, Stats.PolledReferencesObjectCount, ENetTraceVerbosity::Trace);
}

void UObjectReplicationBridge::UpdateInstancesWorldLocation()
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (!GetInstanceWorldLocationFunction)
	{
		return;
	}

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	const FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();
	FWorldLocations& WorldLocations = ReplicationSystemInternal->GetWorldLocations();
	const TArray<UObject*>& ReplicatedInstances = LocalNetRefHandleManager.GetReplicatedInstances();


	// Retrieve the world location for instances that supports it. Only dirty objects are considered.
	auto UpdateInstanceWorldLocation = [this, &ReplicatedInstances, &LocalNetRefHandleManager, &WorldLocations](uint32 InternalObjectIndex)
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = LocalNetRefHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);
		if (ObjectData.InstanceProtocol && EnumHasAnyFlags(ObjectData.InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::NeedsWorldLocationUpdate))
		{
			FVector WorldLocation = (*GetInstanceWorldLocationFunction)(ObjectData.RefHandle, ReplicatedInstances[InternalObjectIndex]);
			WorldLocations.SetWorldLocation(InternalObjectIndex, WorldLocation);
		}
	};

	const FNetBitArrayView DirtyObjects = ReplicationSystemInternal->GetDirtyNetObjectTracker().GetDirtyNetObjects();
	DirtyObjects.ForAllSetBits(UpdateInstanceWorldLocation);

}

void UObjectReplicationBridge::SetPollFramePeriod(UE::Net::Private::FInternalNetRefIndex InternalReplicationIndex, uint8 PollFramePeriod)
{
	PollFrequencyLimiter->SetPollFramePeriod(InternalReplicationIndex, PollFramePeriod);
}

void UObjectReplicationBridge::SetPollWithObject(FNetRefHandle ObjectToPollWithHandle, FNetRefHandle ObjectHandle)
{
	const UE::Net::Private::FInternalNetRefIndex PollWithInternalReplicationIndex = NetRefHandleManager->GetInternalIndex(ObjectToPollWithHandle);
	const UE::Net::Private::FInternalNetRefIndex InternalReplicationIndex = NetRefHandleManager->GetInternalIndex(ObjectHandle);
	PollFrequencyLimiter->SetPollWithObject(PollWithInternalReplicationIndex, InternalReplicationIndex);
}

bool UObjectReplicationBridge::GetObjectWantsToBeDormant(FNetRefHandle Handle) const
{
	using namespace UE::Net::Private;

	const FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	const FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();

	if (const FInternalNetRefIndex InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(Handle))
	{
		return LocalNetRefHandleManager.GetWantToBeDormantInternalIndices().GetBit(InternalObjectIndex);
	}

	return false;
}

void UObjectReplicationBridge::SetObjectWantsToBeDormant(FNetRefHandle Handle, bool bWantsToBeDormant)
{
	using namespace UE::Net::Private;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	FNetRefHandleManager& LocalNetRefHandleManager = ReplicationSystemInternal->GetNetRefHandleManager();

	if (const FInternalNetRefIndex InternalObjectIndex = LocalNetRefHandleManager.GetInternalIndex(Handle))
	{
		UE::Net::FNetBitArray& WantToBeDormantObjects = LocalNetRefHandleManager.GetWantToBeDormantInternalIndices();

		// Update pending dormancy status
		WantToBeDormantObjects.SetBitValue(InternalObjectIndex, bWantsToBeDormant);

		// If we want to be dormant we want to make sure we poll the object one more time
		if (bWantsToBeDormant)
		{
			// Note: this will override update frequency forcing a last poll/pre-replication-update if necessary
			// No need to do this for subobjects as they always are handled with owner
			DormantHandlesPendingFlush.Add(Handle);
		}

		// Since we use this as a mask when updating objects we must include subobjects as well
		// Subobjects added later will copy status from owner when they are added
		for (const FInternalNetRefIndex SubObjectInternalIndex : LocalNetRefHandleManager.GetSubObjects(InternalObjectIndex))
		{
			WantToBeDormantObjects.SetBitValue(SubObjectInternalIndex, bWantsToBeDormant);
		}
	}
}

void UObjectReplicationBridge::ForceUpdateWantsToBeDormantObject(FNetRefHandle Handle)
{
	DormantHandlesPendingFlush.Add(Handle);
}

void UObjectReplicationBridge::SetNetPushIdOnInstance(UE::Net::FReplicationInstanceProtocol* InstanceProtocol, FNetHandle NetHandle)
{
#if WITH_PUSH_MODEL
	using namespace UE::Net;

	// Set push ID only if any state supports it. If no state supports it then we might crash if setting the ID.
	if (EnumHasAnyFlags(InstanceProtocol->InstanceTraits, EReplicationInstanceProtocolTraits::HasPartialPushBasedDirtiness | EReplicationInstanceProtocolTraits::HasFullPushBasedDirtiness))
	{
		const Private::FNetPushObjectHandle PushHandle(NetHandle);
		TArrayView<const FReplicationFragment* const> Fragments(InstanceProtocol->Fragments, InstanceProtocol->FragmentCount);
		SetNetPushIdOnFragments(Fragments, PushHandle);
	}
#endif
}

bool UObjectReplicationBridge::FindPollInfo(const UClass* Class, uint8& OutPollPeriod)
{
	if (!(bAllowPollPeriodOverrides & bHasPollOverrides))
	{
		return false;
	}

	const FName ClassName = Class->GetFName();
	if (const FPollInfo* PollInfo = ClassesWithPollPeriodOverride.Find(ClassName))
	{
		OutPollPeriod = PollInfo->PollFramePeriod;
		return true;
	}

	if (ClassesWithoutPollPeriodOverride.Find(ClassName))
	{
		return false;
	}

	/**
	 * Only if there are poll period overrides for class hierarchies does it make sense to add
	 * more class names to the exact match containers.
	 */
	if (ClassHierarchyPollPeriodOverrides.Num() == 0)
	{
		return false;
	}

	// We have not encountered this class before. Let's add it to the appropriate container for faster lookup next time.
	if (bHasDirtyClassesInPollPeriodOverrides)
	{
		FindClassesInPollPeriodOverrides();
	}

	const UClass* SuperclassWithPollInfo = nullptr;
	FPollInfo SuperclassPollInfo;
	for (const auto& ClassNameAndPollInfo : ClassHierarchyPollPeriodOverrides)
	{
		const UClass* ClassWithPollInfo = ClassNameAndPollInfo.Value.Class.Get();
		if (ClassWithPollInfo == nullptr)
		{
			continue;
		}

		if (Class->IsChildOf(ClassWithPollInfo))
		{
			// If we've already found a superclass with a config, see which one is closer in the hierarchy.
			if (SuperclassWithPollInfo != nullptr)
			{
				if (ClassWithPollInfo->IsChildOf(SuperclassWithPollInfo))
				{
					SuperclassWithPollInfo = ClassWithPollInfo;
					SuperclassPollInfo = ClassNameAndPollInfo.Value;
				}
			}
			else
			{
				SuperclassWithPollInfo = ClassWithPollInfo;
				SuperclassPollInfo = ClassNameAndPollInfo.Value;
			}
		}
	}

	if (SuperclassWithPollInfo != nullptr)
	{
		/*
		 * Reset class weak pointer as it's not used for exact class matches
		 * and we have no interest in maintaining a valid weak pointer for this case.
		 */
		SuperclassPollInfo.Class.Reset();

		// Add the class hiearchy to our set of classes with overrides.
		for (const UClass* ClassToAdd = Class; ClassToAdd != nullptr; ClassToAdd = ClassToAdd->GetSuperClass())
		{
			const FName ClassToAddName = ClassToAdd->GetFName();
			ClassesWithPollPeriodOverride.FindOrAdd(ClassToAddName, SuperclassPollInfo);
		}

		OutPollPeriod = SuperclassPollInfo.PollFramePeriod;
		return true;
	}
	else
	{
		// Add the class hiearchy to our set of classes without overrides.
		for (const UClass* ClassToAdd = Class; ClassToAdd != nullptr; ClassToAdd = ClassToAdd->GetSuperClass())
		{
			// We avoid adding classes that are in the exact match container, even though it's not strictly necessary.
			// It makes it easier to reason about things as a class will only be found in exactly one exact cast container.
			const FName ClassToAddName = ClassToAdd->GetFName();
			if (ClassesWithPollPeriodOverride.Contains(ClassToAddName))
			{
				continue;
			}

			ClassesWithoutPollPeriodOverride.Add(ClassToAddName);
		}
	}

	return false;
}

FName UObjectReplicationBridge::GetConfigClassPathName(const UClass* Class)
{
	if (FName* CachedPathName = ConfigClassPathNameCache.Find(Class))
	{
		return *CachedPathName;
	}

	const FName ClassPathName(Class->GetPathName());

	ConfigClassPathNameCache.Add(Class, ClassPathName);

	return ClassPathName;
}

bool UObjectReplicationBridge::ShouldClassBeDeltaCompressed(const UClass* Class)
{
	using namespace UE::Net;

	if (ClassesWithDeltaCompression.Num() > 0)
	{
		for (; Class != nullptr; Class = Class->GetSuperClass())
		{
			if (bool* bShouldBeDeltaCompressed = ClassesWithDeltaCompression.Find(GetConfigClassPathName(Class)))
			{
				return *bShouldBeDeltaCompressed;
			}
		}
	}

	return false;
}

UE::Net::FNetObjectFilterHandle UObjectReplicationBridge::GetDynamicFilter(const UClass* Class)
{
	using namespace UE::Net;

	if (ClassesWithDynamicFilter.Num() > 0)
	{
		const FName ClassName = GetConfigClassPathName(Class);

		// Try exact match first.
		if (FNetObjectFilterHandle* FilterHandlePtr = ClassesWithDynamicFilter.Find(ClassName))
		{
			return *FilterHandlePtr;
		}

		/**
		 * Try to find superclass. If we find it and the classes are considered equal we copy the filter setting.
		 * If it's not equal we check whether it can be spatialized or not and use the result of that.
		 * In all cases we add the result to the mapping for faster lookup next time.
		 */
		for (const UClass* SuperClass = Class->GetSuperClass(); SuperClass != nullptr; SuperClass = SuperClass->GetSuperClass())
		{
			const FName SuperClassName = GetConfigClassPathName(SuperClass);

			// Try to get exact match first.
			if (FNetObjectFilterHandle* FilterHandlePtr = ClassesWithDynamicFilter.Find(SuperClassName))
			{
				if (ShouldSubclassUseSameFilterFunction(SuperClass, Class))
				{
					const FNetObjectFilterHandle FilterHandle = *FilterHandlePtr;
					ClassesWithDynamicFilter.Add(ClassName, FilterHandle);
					return FilterHandle;
				}

				// Here's a good place to put a line of code and set a breakpoint to debug inheritance issues.

				break;
			}
		}

		// Either super class wasn't found or it wasn't considered equal. Let's add a new filter mapping.
		const FNetObjectFilterHandle FilterHandle = ShouldUseDefaultSpatialFilterFunction(Class) ? DefaultSpatialFilterHandle : InvalidNetObjectFilterHandle;
		ClassesWithDynamicFilter.Add(ClassName, FilterHandle);
		return FilterHandle;
	}

	/**
	 * For the cases when there are no configured filter mappings we just check whether to use a spatial filter or not.
	 * We don't add anything to the filter mapping.
	 */
	const FNetObjectFilterHandle FilterHandle = ShouldUseDefaultSpatialFilterFunction(Class) ? DefaultSpatialFilterHandle : InvalidNetObjectFilterHandle;
	return FilterHandle;
}

UE::Net::FNetObjectPrioritizerHandle UObjectReplicationBridge::GetPrioritizer(const UClass* Class)
{
	using namespace UE::Net;

	if (ClassesWithPrioritizer.Num() > 0)
	{
		const FName ClassName = GetConfigClassPathName(Class);

		// Try exact match first.
		if (FNetObjectPrioritizerHandle* PrioritizerHandlePtr = ClassesWithPrioritizer.Find(ClassName))
		{
			return *PrioritizerHandlePtr;
		}

		/**
		 * Try to find superclass. If we find it and the classes are considered equal we copy the filter setting.
		 * If it's not equal we check whether it can be spatialized or not and use the result of that.
		 * In all cases we add the result to the mapping for faster lookup next time.
		 */
		for (const UClass* SuperClass = Class->GetSuperClass(); SuperClass != nullptr; SuperClass = SuperClass->GetSuperClass())
		{
			const FName SuperClassName = GetConfigClassPathName(SuperClass);

			// Try to get exact match first.
			if (FNetObjectPrioritizerHandle* PrioritizerHandlePtr = ClassesWithPrioritizer.Find(SuperClassName))
			{
				// Assume this class should use the same prioritizer
				{
					const FNetObjectPrioritizerHandle PrioritizerHandle = *PrioritizerHandlePtr;
					ClassesWithPrioritizer.Add(ClassName, PrioritizerHandle);
					return PrioritizerHandle;
				}
			}
		}
	}

	// No prioritizer has been configured for this class.
	return InvalidNetObjectPrioritizerHandle;
}

void UObjectReplicationBridge::LoadConfig()
{
	// Clear everything related to the config.
	bHasPollOverrides = false;
	bHasDirtyClassesInPollPeriodOverrides = false;
	ClassHierarchyPollPeriodOverrides.Empty();
	ClassesWithPollPeriodOverride.Empty();
	ClassesWithoutPollPeriodOverride.Empty();
	ClassesWithDynamicFilter.Empty();
	ClassesWithPrioritizer.Empty();
	ClassesWithDeltaCompression.Empty();

	// Reset PathNameCache
	ConfigClassPathNameCache.Empty();

	const UObjectReplicationBridgeConfig* BridgeConfig = UObjectReplicationBridgeConfig::GetConfig();

	// Load poll configs
	
	// These classes are forbidden to override due to being too generic and could cause memory and performance issues.
	// If there's need for a global poll period override it should be implemented separately and not via class overrides.
	const FName ForbiddenNames[] = {NAME_Object, NAME_Actor};
	const TConstArrayView<FName> ForbiddenNamesArray = MakeArrayView(ForbiddenNames);

	for (const FObjectReplicationBridgePollConfig& PollOverride : BridgeConfig->GetPollConfigs())
	{
		if (!ensure(!ForbiddenNamesArray.Contains(PollOverride.ClassName)))
		{
			continue;
		}

		bHasPollOverrides = true;

		FPollInfo PollInfo;
		PollInfo.PollFramePeriod = FPlatformMath::Min(PollOverride.PollFramePeriod, 255U) & 255U;
		if (PollOverride.bIncludeSubclasses)
		{
			bHasDirtyClassesInPollPeriodOverrides = true;
			ClassHierarchyPollPeriodOverrides.Add(PollOverride.ClassName, PollInfo);
		}
		else
		{
			ClassesWithPollPeriodOverride.Add(PollOverride.ClassName, PollInfo);
		}
	}

	if (bHasDirtyClassesInPollPeriodOverrides)
	{
		FindClassesInPollPeriodOverrides();
	}

	// Filter mappings.
	DefaultSpatialFilterHandle = ReplicationSystem->GetFilterHandle(BridgeConfig->GetDefaultSpatialFilterName());

	{
		for (const FObjectReplicationBridgeFilterConfig& FilterConfig : BridgeConfig->GetFilterConfigs())
		{
			const UE::Net::FNetObjectFilterHandle FilterHandle = ReplicationSystem->GetFilterHandle(FilterConfig.DynamicFilterName);
			ClassesWithDynamicFilter.Add(FilterConfig.ClassName, FilterHandle);
		}
	}

	// Prioritizer mappings
	{
		for (const FObjectReplicationBridgePrioritizerConfig& PrioritizerConfig : BridgeConfig->GetPrioritizerConfigs())
		{
			const UE::Net::FNetObjectPrioritizerHandle PrioritizerHandle = ReplicationSystem->GetPrioritizerHandle(PrioritizerConfig.PrioritizerName);
			ClassesWithPrioritizer.Add(PrioritizerConfig.ClassName, PrioritizerHandle);
		}
	}

	// Load delta compression settings
	{
		for (const FObjectReplicationBridgeDeltaCompressionConfig& DCConfig : BridgeConfig->GetDeltaCompressionConfigs())
		{
			if (!ensure(!ForbiddenNamesArray.Contains(DCConfig.ClassName)))
			{
				continue;
			}

			ClassesWithDeltaCompression.Add(DCConfig.ClassName, DCConfig.bEnableDeltaCompression);
		}
	}
}

void UObjectReplicationBridge::FindClassesInPollPeriodOverrides()
{
	bool bFailedToFindClass = false;
	for (auto& ClassNameAndPollInfo : ClassHierarchyPollPeriodOverrides)
	{
		FPollInfo& PollInfo = ClassNameAndPollInfo.Value;
		if (!PollInfo.Class.IsValid())
		{
			constexpr UObject* ClassOuter = nullptr;
			constexpr bool bExactClass = true;
			const UClass* Class = CastChecked<UClass>(StaticFindObject(UClass::StaticClass(), ClassOuter, ToCStr(ClassNameAndPollInfo.Key.ToString()), bExactClass));
			bFailedToFindClass |= (Class == nullptr);
			PollInfo.Class = Class;
		}
	}

	bHasDirtyClassesInPollPeriodOverrides = bFailedToFindClass;
}

void UObjectReplicationBridge::SetShouldUseDefaultSpatialFilterFunction(TFunction<bool(const UClass*)> InShouldUseDefaultSpatialFilterFunction)
{
	if (!ensureAlwaysMsgf((bool)InShouldUseDefaultSpatialFilterFunction, TEXT("%s"), TEXT("A valid function must be provided for SetShouldUseDefaultSpatialFilterFunction.")))
	{
		return;
	}

	ShouldUseDefaultSpatialFilterFunction = InShouldUseDefaultSpatialFilterFunction;
}

void UObjectReplicationBridge::SetShouldSubclassUseSameFilterFunction(TFunction<bool(const UClass* Class, const UClass* Subclass)> InShouldSubclassUseSameFilterFunction)
{
	if (!ensureAlwaysMsgf((bool)InShouldSubclassUseSameFilterFunction, TEXT("%s"), TEXT("A valid function must be provided for SetShouldSubclassUseSameFilterFunction.")))
	{
		return;
	}

	ShouldSubclassUseSameFilterFunction = InShouldSubclassUseSameFilterFunction;
}
