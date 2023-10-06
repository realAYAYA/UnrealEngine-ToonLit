// Copyright Epic Games, Inc. All Rights Reserved.
#include "Net/Iris/ReplicationSystem/ActorReplicationBridge.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorReplicationBridge)

#if UE_WITH_IRIS

#include "Net/Iris/ReplicationSystem/ActorReplicationBridgeInternal.h"
#include "Iris/IrisConfig.h"
#include "Iris/IrisConstants.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationSystem/NetToken.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridgeConfig.h"
#include "Iris/ReplicationSystem/StringTokenStore.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Serialization/IrisObjectReferencePackageMap.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Level.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/DataBunch.h"
#include "Net/Core/Misc/NetSubObjectRegistry.h"
#include "Net/NetSubObjectRegistryGetter.h"
#include "Templates/Casts.h"
#include "UObject/Package.h"
#include <limits>

#define UE_LOG_ACTORREPLICATIONBRIDGE(Category, Format, ...)	UE_LOG(LogIrisBridge, Category, TEXT("ActorReplicationBridge(%u)::") Format, GetReplicationSystem()->GetId(), ##__VA_ARGS__)

extern bool GDefaultUseSubObjectReplicationList;

namespace UE::Net
{

bool ShouldUseIrisReplication(const UObject* Object)
{
	return ShouldUseIrisReplication();
}

}

namespace UE::Net::Private
{

bool IsActorValidForIrisReplication(const AActor* Actor)
{
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && !Actor->IsUnreachable();
}

void ActorReplicationBridgePreUpdateFunction(FNetRefHandle Handle, UObject* Instance, const UReplicationBridge* Bridge)
{
	AActor* Actor = Cast<AActor>(Instance);
	if (IsActorValidForIrisReplication(Actor))
	{
		/** $IRIS TODO:
		 * Here we need to call something that either fakes CallPrereplication or we need to mimic the interfaces of NetDriver/PropertyChangeTracker
		 * however we do think any state changes should be pushed to the network system rather than polled
		 * One example is the gatherCurrent() movement, movementdata should have its own ReplicationState that is updated when changed. (or updated using a helper method)
		 */
		Actor->CallPreReplication(CastChecked<const UActorReplicationBridge>(Bridge)->GetNetDriver());
	}
}

void ActorReplicationBridgeGetActorWorldObjectInfo(FNetRefHandle Handle, const UObject* Instance, FVector& OutWorldLocation, float& OutCullDistance)
{
	if (const AActor* Actor = Cast<AActor>(Instance))
	{
		OutWorldLocation = Actor->GetActorLocation();
		OutCullDistance = Actor->NetCullDistanceSquared > 0.0f ? FMath::Sqrt(Actor->NetCullDistanceSquared) : 0.0f;
	}
}

} // end namespace UE::Net::Private

UActorReplicationBridge::UActorReplicationBridge()
: UObjectReplicationBridge()
, NetDriver(nullptr)
, ObjectReferencePackageMap(nullptr)
, SpawnInfoFlags(0U)
{
	SetInstancePreUpdateFunction(UE::Net::Private::ActorReplicationBridgePreUpdateFunction);
	SetInstanceGetWorldObjectInfoFunction(UE::Net::Private::ActorReplicationBridgeGetActorWorldObjectInfo);
}

void UActorReplicationBridge::Initialize(UReplicationSystem* InReplicationSystem)
{
	Super::Initialize(InReplicationSystem);

	{
		auto ShouldSpatialize = [](const UClass* Class)
		{
			if (AActor* CDO = Cast<AActor>(Class->GetDefaultObject()))
			{
				return !(CDO->bAlwaysRelevant || CDO->bOnlyRelevantToOwner || CDO->bNetUseOwnerRelevancy);
			}

			return false;
		};

		SetShouldUseDefaultSpatialFilterFunction(ShouldSpatialize);
	}

	{
		auto ClassesAreRelevantEqual = [](const UClass* Class, const UClass* Subclass)
		{
			const AActor* CDO = Cast<AActor>(Class->GetDefaultObject());
			const AActor* SubCDO = Cast<AActor>(Subclass->GetDefaultObject());
			// Same CDO (nullptr)?
			if (CDO == SubCDO)
			{
				return true;
			}

			if (CDO == nullptr || SubCDO == nullptr)
			{
				return false;
			}

			{
				return CDO->bAlwaysRelevant == SubCDO->bAlwaysRelevant
					&& CDO->bOnlyRelevantToOwner == SubCDO->bOnlyRelevantToOwner
					&& CDO->bNetUseOwnerRelevancy == SubCDO->bNetUseOwnerRelevancy;
			}
		};

		SetShouldSubclassUseSameFilterFunction(ClassesAreRelevantEqual);
	}

	ObjectReferencePackageMap = NewObject<UIrisObjectReferencePackageMap>();
	ObjectReferencePackageMap->AddToRoot();

	// Get spawn info flags from cvars
	SpawnInfoFlags = UE::Net::Private::GetActorReplicationBridgeSpawnInfoFlags();
}

UActorReplicationBridge::~UActorReplicationBridge()
{
	if (ObjectReferencePackageMap)
	{
		ObjectReferencePackageMap->RemoveFromRoot();
		ObjectReferencePackageMap->MarkAsGarbage();
		ObjectReferencePackageMap = nullptr;
	}
}

UE::Net::FNetRefHandle UActorReplicationBridge::BeginReplication(AActor* Actor, const FActorBeginReplicationParams& Params)
{
	using namespace UE::Net;

	if (!ShouldUseIrisReplication(Actor))
	{
		return FNetRefHandle();
	}

	if (!ensureMsgf(Actor == nullptr || !(Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)), TEXT("Actor %s is a CDO or Archetype and should not be replicated."), ToCStr(GetFullNameSafe(Actor))))
	{
		return FNetRefHandle();
	}

	const bool bIsNetActor = ULevel::IsNetActor(Actor);
	if (!bIsNetActor)
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(VeryVerbose, TEXT("Actor %s doesn't have a NetRole."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (Actor->GetLocalRole() != ROLE_Authority)
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(VeryVerbose, TEXT("Actor %s NetRole isn't Authority."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (Actor->IsActorBeingDestroyed() || !IsValid(Actor) || Actor->IsUnreachable())
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("Actor %s is being destroyed or unreachable and can't be replicated."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (!Actor->GetIsReplicated())
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("Actor %s is not supposed to be replicated."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (Actor->GetTearOff())
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("Actor %s is torn off and should not be replicated."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (!Actor->IsActorInitialized())
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("Actor %s is not initialized and won't be replicated."), ToCStr(GetFullNameSafe(Actor)));
		return FNetRefHandle();
	}

	if (!NetDriver)
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(VeryVerbose, TEXT("There's no NetDriver so nothing can be replicated."));
		return FNetRefHandle();
	}


	if (!NetDriver->ShouldReplicateActor(Actor))
	{
		UE_LOG_ACTORREPLICATIONBRIDGE(VeryVerbose, TEXT("Actor %s doesn't want to replicate with NetDriver %s."), ToCStr(GetFullNameSafe(Actor)), ToCStr(NetDriver->GetName()));
		return FNetRefHandle();
	}

	// Initially dormant actors begin replication when their dormancy is flushed
	const ENetDormancy Dormancy = Actor->NetDormancy;	
	if (Actor->IsNetStartupActor() && (Dormancy == DORM_Initial))
	{
		return FNetRefHandle();
	}

	FNetRefHandle ExistingHandle = GetReplicatedRefHandle(Actor);
	if (ExistingHandle.IsValid())
	{
		return ExistingHandle;
	}

	if (!Actor->IsUsingRegisteredSubObjectList())
	{
		// Ensure the first time to get attention!
		ensureMsgf(GDefaultUseSubObjectReplicationList, TEXT("Iris requires replicated actors to use registered subobjectslists. Add \n[SystemSettings]\nnet.SubObjects.DefaultUseSubObjectReplicationList=1\n to your DefaultEngine.ini"));
		UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("Actor %s does not replicate subobjects using the registered SubObjectsLists, SubObjects will not replicate properly"), ToCStr(GetFullNameSafe(Actor)));
	}

	// Create handles for the registered fragments
	Super::FCreateNetRefHandleParams CreateNetRefHandleParams = DefaultCreateNetRefHandleParams;
	CreateNetRefHandleParams.bNeedsPreUpdate = 1U;
	CreateNetRefHandleParams.bNeedsWorldLocationUpdate = 1U;
	CreateNetRefHandleParams.StaticPriority = (Actor->bAlwaysRelevant || Actor->bOnlyRelevantToOwner) ? Actor->NetPriority : 0.0f;
	CreateNetRefHandleParams.PollFrequency = Actor->NetUpdateFrequency;

#if !UE_BUILD_SHIPPING
	ensureMsgf(!(Actor->bAlwaysRelevant || Actor->bOnlyRelevantToOwner) || CreateNetRefHandleParams.StaticPriority >= 1.0f, TEXT("Very low NetPriority %.02f for always relevant or owner relevant Actor %s. Set it to 1.0f or higher."), Actor->NetPriority, ToCStr(Actor->GetName()));
#endif

	FNetRefHandle ActorRefHandle = Super::BeginReplication(Actor, CreateNetRefHandleParams);

	if (!ActorRefHandle.IsValid())
	{
		ensureMsgf(false, TEXT("Failed to create NetRefHandle for Actor Named %s"), ToCStr(Actor->GetName()));
		return FNetRefHandle();
	}

	// Set owning connection filtering if actor is only relevant to owner
	{
		if (Actor->bOnlyRelevantToOwner && !Actor->bAlwaysRelevant)
		{
			GetReplicationSystem()->SetFilter(ActorRefHandle, ToOwnerFilterHandle);
		}
	}

	// Set if this is a NetTemporary
	{
		if (Actor->bNetTemporary)
		{
			GetReplicationSystem()->SetIsNetTemporary(ActorRefHandle);
		}
	}

	// Dormancy, we track all actors that does want to be dormant
	{
		if (Dormancy > DORM_Awake)
		{
			SetObjectWantsToBeDormant(ActorRefHandle, true);
		}
	}

	// Setup Level filtering unless this actor belongs to the persistent level
	{
		ULevel* Level = Actor->GetLevel();
		if (Params.bIncludeInLevelGroupFilter && (!Level->IsPersistentLevel() || (Level != NetDriver->GetWorld()->PersistentLevel)))
		{
			const UPackage* const LevelPackage = Level->GetOutermost();
			const FName PackageName = LevelPackage->GetFName();

			FNetObjectGroupHandle LevelGroup = GetLevelGroup(Level);
			if (!LevelGroup)
			{
				LevelGroup = CreateLevelGroup(Level);

				UE_LOG_ACTORREPLICATIONBRIDGE(Log, TEXT("Created new GroupIndex: %u for Level: %s"), LevelGroup, ToCStr(PackageName.ToString()));

				// Update the filtering status of the group based on current level visibility for all connections
				NetDriver->UpdateGroupFilterStatusForLevel(Level, LevelGroup);
			}

			// Add object to group
			UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("Added %s to GroupIndex: %u Level: %s"), *ActorRefHandle.ToString(), LevelGroup, ToCStr(PackageName.ToString()));
			GetReplicationSystem()->AddToGroup(LevelGroup, ActorRefHandle);	
		}
	}

	// If we have registered sub objects we replicate them as well
	const FSubObjectRegistry& ActorSubObjects = FSubObjectRegistryGetter::GetSubObjects(Actor);
	const TArray<FReplicatedComponentInfo>& ReplicatedComponents = FSubObjectRegistryGetter::GetReplicatedComponents(Actor);

	if (ActorSubObjects.GetRegistryList().Num() != 0 || ReplicatedComponents.Num() != 0)
	{
		// Start with the Actor's SubObjects (that is SubObjects that are not ActorComponents)
		for (const FSubObjectRegistry::FEntry& SubObjectInfo : ActorSubObjects.GetRegistryList())
		{
			UObject* SubObjectToReplicate = SubObjectInfo.GetSubObject();
			if (IsValid(SubObjectToReplicate) && SubObjectInfo.NetCondition != ELifetimeCondition::COND_Never)
			{
				FNetRefHandle SubObjectRefHandle = UObjectReplicationBridge::BeginReplication(ActorRefHandle, SubObjectToReplicate);
				if (SubObjectRefHandle.IsValid() && SubObjectInfo.NetCondition != ELifetimeCondition::COND_None)
				{
					UObjectReplicationBridge::SetSubObjectNetCondition(SubObjectRefHandle, SubObjectInfo.NetCondition);
				}
			}
		}

		// Now the replicated ActorComponents and their SubObjects
		for (const FReplicatedComponentInfo& RepComponentInfo : ReplicatedComponents)
		{
			if (IsValid(RepComponentInfo.Component) && RepComponentInfo.NetCondition != ELifetimeCondition::COND_Never)
			{
				UActorComponent* ReplicatedComponent = RepComponentInfo.Component;
				ReplicatedComponent->BeginReplication();
				// NetCondition is set by replicated component
			}
		}
	}

	return ActorRefHandle;
}

UE::Net::FNetRefHandle UActorReplicationBridge::BeginReplication(FNetRefHandle OwnerHandle, UActorComponent* SubObject)
{
	using namespace UE::Net;

	if (!OwnerHandle.IsValid() || !ShouldUseIrisReplication(SubObject))
	{
		return FNetRefHandle();
	}

	AActor* Owner = SubObject->GetOwner();

	FNetRefHandle ReplicatedComponentHandle = GetReplicatedRefHandle(SubObject);
	const FReplicatedComponentInfo* RepComponentInfo = FSubObjectRegistryGetter::GetReplicatedComponentInfoForComponent(Owner, SubObject);

	if (!ReplicatedComponentHandle.IsValid())
	{
		if (!IsValid(SubObject)
			|| SubObject->IsUnreachable()
			|| !SubObject->GetIsReplicated()
			|| SubObject->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject)
		)
		{
			return FNetRefHandle();
		}

		if (!SubObject->IsUsingRegisteredSubObjectList())
		{
			UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("ActorComponent %s does not replicate subobjects using the registered SubObjectsLists, SubObjects might not replicate properly."), ToCStr(GetFullNameSafe(SubObject)));
		}

		if (RepComponentInfo == nullptr || RepComponentInfo->NetCondition == ELifetimeCondition::COND_Never)
		{
			return FNetRefHandle();
		}

		// Start replicating the subobject with its owner.
		ReplicatedComponentHandle = Super::BeginReplication(OwnerHandle, SubObject);
	}

	if (!ReplicatedComponentHandle.IsValid())
	{
		ensureMsgf(false, TEXT("Failed to create or find NetRefHandle for ActorComponent Named %s"), ToCStr(SubObject->GetName()));
		return FNetRefHandle();
	}

	// Update or set any conditionals
	if (RepComponentInfo->NetCondition != ELifetimeCondition::COND_None)
	{
		SetSubObjectNetCondition(ReplicatedComponentHandle, RepComponentInfo->NetCondition);
	}

	// Begin replication for any SubObjects registered by the component
	for (const FSubObjectRegistry::FEntry& SubObjectInfo : RepComponentInfo->SubObjects.GetRegistryList())
	{
		UObject* SubObjectToReplicate = SubObjectInfo.GetSubObject();
		if (IsValid(SubObjectToReplicate) && SubObjectInfo.NetCondition != ELifetimeCondition::COND_Never)
		{
			FNetRefHandle SubObjectHandle = UObjectReplicationBridge::BeginReplication(OwnerHandle, SubObjectToReplicate, ReplicatedComponentHandle, UReplicationBridge::ESubObjectInsertionOrder::ReplicateWith);
			if (SubObjectHandle.IsValid() && SubObjectInfo.NetCondition != ELifetimeCondition::COND_None)
			{
				SetSubObjectNetCondition(SubObjectHandle, SubObjectInfo.NetCondition);
			}
		}
	}

	return ReplicatedComponentHandle;
}

void UActorReplicationBridge::EndReplication(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	using namespace UE::Net;

	FNetRefHandle RefHandle = GetReplicatedRefHandle(Actor);
	if (RefHandle.IsValid())
	{
		UE_LOG(LogIrisBridge, Verbose, TEXT("EndReplication for %s %s. Reason %s "), *Actor->GetName(), *RefHandle.ToString(), *UEnum::GetValueAsString(TEXT("Engine.EEndPlayReason"), EndPlayReason));
	
		EEndReplicationFlags Flags = EEndReplicationFlags::None;
		const bool bShouldDestroyObject = EndPlayReason == EEndPlayReason::Destroyed;		
		if (bShouldDestroyObject)
		{ 
			Flags |= EEndReplicationFlags::Destroy | EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId;
		}
					
		const bool bShouldCreateDestructionInfo = RefHandle.IsStatic() && bShouldDestroyObject && GetReplicationSystem()->IsServer();
		if (bShouldCreateDestructionInfo)
		{
			UObjectReplicationBridge::FEndReplicationParameters EndReplicationParameters;

			EndReplicationParameters.Location = Actor->GetActorLocation();
			EndReplicationParameters.Level = Actor->GetLevel();
			EndReplicationParameters.bUseDistanceBasedPrioritization = Actor->bAlwaysRelevant == false;

			UObjectReplicationBridge::EndReplication(RefHandle, Flags, &EndReplicationParameters);		
		}
		else
		{
			UObjectReplicationBridge::EndReplication(RefHandle, Flags, nullptr);
		}
	}
}

void UActorReplicationBridge::EndReplicationForActorComponent(UActorComponent* ActorComponent, EEndReplicationFlags EndReplicationFlags)
{
	using namespace UE::Net;

	FNetRefHandle ComponentHandle = GetReplicatedRefHandle(ActorComponent);
	const AActor* Actor = ActorComponent->GetOwner();
	if (ComponentHandle.IsValid() && Actor)
	{
		UE_LOG(LogIrisBridge, Verbose, TEXT("EndReplicationForActorComponent for %s %s."), *ActorComponent->GetName(), *ComponentHandle.ToString());
		
		UObjectReplicationBridge::EndReplication(ComponentHandle, EndReplicationFlags, nullptr);
	}
}

bool UActorReplicationBridge::WriteCreationHeader(UE::Net::FNetSerializationContext& Context, FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	ensure(IsReplicatedHandle(Handle));
	const UObject* Object = GetReplicatedObject(Handle);
	if (const AActor* Actor = Cast<AActor>(Object))
	{
		// Get Header
		FActorCreationHeader Header;
		GetActorCreationHeader(Actor, Header);
	
		// Serialize the data
		// Indicate that this is an actor
		Writer->WriteBool(true);
		WriteActorCreationHeader(Context, Header, SpawnInfoFlags);

		return !Writer->IsOverflown();
	}
	else if (Object)
	{
		// Get Header
		FSubObjectCreationHeader Header;
		GetSubObjectCreationHeader(Object, Header);

		// Serialize the data
		// Indicate that this is a SubObject
		Writer->WriteBool(false);
		WriteSubObjectCreationHeader(Context, Header);

		return !Writer->IsOverflown();
	}

	ensureMsgf(false, TEXT("UActorReplicationBridge::WriteCreationHeader Failed to write creationHeader for NetRefHandle (Id=%u)"), Handle.GetId());

	return false;
}

UObjectReplicationBridge::FCreationHeader* UActorReplicationBridge::ReadCreationHeader(UE::Net::FNetSerializationContext& Context)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	if (const bool bIsActor = Reader->ReadBool())
	{
		TUniquePtr<FActorCreationHeader> Header(new FActorCreationHeader);
		ReadActorCreationHeader(Context, *(Header.Get()));

		if (!Reader->IsOverflown())
		{
			return Header.Release();
		}
	}
	else
	{
		TUniquePtr<FSubObjectCreationHeader> Header(new FSubObjectCreationHeader);
		ReadSubObjectCreationHeader(Context, *(Header.Get()));

		if (!Reader->IsOverflown())
		{
			return Header.Release();
		}
	}

	return nullptr;
}

FObjectReplicationBridgeInstantiateResult UActorReplicationBridge::BeginInstantiateFromRemote(FNetRefHandle SubObjectOwnerNetHandle, const UE::Net::FNetObjectResolveContext& ResolveContext, const UObjectReplicationBridge::FCreationHeader* InHeader)
{
	using namespace UE::Net::Private;

	IRIS_PROFILER_SCOPE(ActorReplicationBridge_OnBeginInstantiateFromRemote);

	FObjectReplicationBridgeInstantiateResult InstantiateResult;

	const FActorReplicationBridgeCreationHeader* BridgeHeader = static_cast<const FActorReplicationBridgeCreationHeader*>(InHeader);

	if (BridgeHeader->bIsActor)
	{
		const FActorCreationHeader* Header = static_cast<const FActorCreationHeader*>(InHeader);

		// Spawn actor
		// Try to instantiate the actor (or find it if it already exists)
		if (Header->bIsDynamic)
		{
			// Find archetype
			UObject* Archetype = ResolveObjectReference(Header->ArchetypeReference, ResolveContext);

			// Find level
			UObject* Level = nullptr;
			if (!Header->bUsePersistentLevel)
			{
				Level = ResolveObjectReference(Header->LevelReference, ResolveContext);
			}

			if (Archetype)
			{
				// For streaming levels, it's possible that the owning level has been made not-visible but is still loaded.
				// In that case, the level will still be found but the owning world will be invalid.
				// If that happens, wait to spawn the Actor until the next time the level is streamed in.
				// At that point, the Server should resend any dynamic Actors.
				ULevel* SpawnLevel = Cast<ULevel>(Level);
				check(SpawnLevel == nullptr || SpawnLevel->GetWorld() != nullptr);

				FActorSpawnParameters SpawnInfo;
				SpawnInfo.Template = Cast<AActor>(Archetype);
				SpawnInfo.OverrideLevel = SpawnLevel;
				SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				SpawnInfo.bRemoteOwned = true;
				SpawnInfo.bNoFail = true;

				UWorld* World = NetDriver->GetWorld();
				FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(Header->SpawnInfo.Location, World->OriginLocation);
		
				AActor* Actor = World->SpawnActorAbsolute(Archetype->GetClass(), FTransform(Header->SpawnInfo.Rotation, SpawnLocation), SpawnInfo);

				// For Iris we expect that we will be able to spawn the actor as streaming always is controlled from server
				if (ensure(Actor))
				{
					const FActorReplicationBridgeSpawnInfo& DefaultSpawnInfo = GetDefaultActorReplicationBridgeSpawnInfo();
					static constexpr float Epsilon = UE_KINDA_SMALL_NUMBER;

					// Set Velocity if it differs from Default
					if (!Header->SpawnInfo.Velocity.Equals(DefaultSpawnInfo.Velocity, Epsilon))
					{
						Actor->PostNetReceiveVelocity(Header->SpawnInfo.Velocity);
					}
					
					// Set Scale if it differs from Default
					if (!Header->SpawnInfo.Scale.Equals(DefaultSpawnInfo.Scale, Epsilon))
					{
						Actor->SetActorRelativeScale3D(Header->SpawnInfo.Scale);
					}

					UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("OnBeginInstantiateFromRemote Spawned Actor %s"), *Actor->GetPathName());
				}

				InstantiateResult.Object = Actor;
				if (NetDriver->ShouldClientDestroyActor(Actor))
				{
					InstantiateResult.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;
				}
				return InstantiateResult;
			}
			else
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("OnBeginInstantiateFromRemote Unable to spawn object, failed to resolve archetype with %s"), *(Header->ArchetypeReference.GetRefHandle().ToString()));
				check(false);
			}
		}
		else
		{
			if (UObject* Object = ResolveObjectReference(Header->ObjectReference, ResolveContext))
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("OnBeginInstantiateFromRemote Found static Actor using path %s"), ToCStr(Object->GetPathName()));
				InstantiateResult.Object = Object;
				if (NetDriver->ShouldClientDestroyActor(Cast<AActor>(Object)))
				{
					InstantiateResult.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;
				}
				return InstantiateResult;
			}
			else
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("OnBeginInstantiateFromRemote Failed to find Resolve ObjectReference for static Actor %s"), *Header->ObjectReference.ToString());
			}
		}
	}
	else
	{
		const FSubObjectCreationHeader* Header = static_cast<const FSubObjectCreationHeader*>(InHeader);

		// Spawn sub object
		if (Header->bIsDynamic)
		{
			// Resolve owner
			UObject* ResolvedOwnerObject = GetReplicatedObject(SubObjectOwnerNetHandle);
			AActor* Owner = Cast<AActor>(ResolvedOwnerObject);

			check(Owner);
			
			UObject* SubObj = nullptr;

			if (Header->bIsNameStableForNetworking)
			{
				// Resolve by finding object relative to owner. We do not allow this object to be destroyed.
				SubObj = ResolveObjectReference(Header->ObjectReference, ResolveContext);

				if (!SubObj)
				{
					UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("BeginInstantiateFromRemote Failed to find subobjectReference for dynamic SubObject %s, Owner %s (%s)"), *Header->ObjectReference.ToString(), *SubObjectOwnerNetHandle.ToString(), *GetPathNameSafe(GetReplicatedObject(SubObjectOwnerNetHandle)));
				}
			}
			else
			{
				UObject* ObjOuter = Owner;

				// We need to spawn the subobject
				UObject* SubObjClassObj = ResolveObjectReference(Header->ObjectClassReference, ResolveContext);
				UClass * SubObjClass = Cast<UClass>(SubObjClassObj);

				// Try to spawn SubObject
 				SubObj = NewObject<UObject>(Owner, SubObjClass);

				// Sanity check some things
				checkf(SubObj != nullptr, TEXT("UActorReplicationBridge::BeginInstantiateFromRemote: Subobject is NULL after instantiating. Class: %s, Actor %s"), *GetNameSafe(SubObjClass), *Owner->GetName());
				checkf(SubObj->IsIn(ObjOuter), TEXT("UActorReplicationBridge::BeginInstantiateFromRemote: Subobject is not in Outer. SubObject: %s, Actor: %s Outer: %s"), *SubObj->GetName(), *Owner->GetName(), *ObjOuter->GetName());
				checkf(Cast<AActor>(SubObj) == nullptr, TEXT("UActorReplicationBridge::BeginInstantiateFromRemote: Subobject is an Actor. SubObject: %s, Actor: %s"), *SubObj->GetName(), *Owner->GetName());

				// Notify actor that we created a component from replication
				Owner->OnSubobjectCreatedFromReplication(SubObj);

				// Created objects may be destroyed.
				InstantiateResult.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;
			}
			
			InstantiateResult.Object = SubObj;
			return InstantiateResult;
		}
		else
		{
			// try to find the object by path
			if (UObject* Object = ResolveObjectReference(Header->ObjectReference, ResolveContext))
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Verbose, TEXT("BeginInstantiateFromRemote Found static SubObject using path %s"), ToCStr(Object->GetPathName()));
				// We do not allow this object to be destroyed.
				InstantiateResult.Object = Object;
				return InstantiateResult;
			}
			else
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("BeginInstantiateFromRemote Failed to find Resolve SubObjectReference for static SubObject %s, Owner %s (%s)"), *Header->ObjectReference.ToString(), *SubObjectOwnerNetHandle.ToString(), *GetPathNameSafe(GetReplicatedObject(SubObjectOwnerNetHandle)));
			}			
		}
	}

	return InstantiateResult;
}

bool UActorReplicationBridge::OnInstantiatedFromRemote(UObject* Instance, const UObjectReplicationBridge::FCreationHeader* InHeader, uint32 ConnectionId) const
{
	using namespace UE::Net::Private;

	const FActorReplicationBridgeCreationHeader* BridgeHeader = static_cast<const FActorReplicationBridgeCreationHeader*>(InHeader);

	if (BridgeHeader->bIsActor)
	{
		const FActorCreationHeader* Header = static_cast<const FActorCreationHeader*>(InHeader);

		if (Header->bIsDynamic)
		{
			// OnActorChannelOpen
			AActor* Actor = CastChecked<AActor>(Instance);
			{
				UNetConnection* Connection = NetDriver->GetConnectionById(ConnectionId);
				FInBunch Bunch(Connection, const_cast<uint8*>(Header->CustomCreationData.GetData()), Header->CustomCreationDataBitCount);
				Actor->OnActorChannelOpen(Bunch, Connection);

				if (Bunch.IsError() || Bunch.GetBitsLeft() != 0)
				{
					// $IRIS TODO Report an intelligent error
					check(false);
					return false;
				}
			}
		}
	}
	
	return true;
}

void UActorReplicationBridge::EndInstantiateFromRemote(FNetRefHandle Handle)
{
	if (AActor* Actor = Cast<AActor>(GetReplicatedObject(Handle)))
	{
		// Optional
		//Actor->NetHandle = Handle;

		Actor->PostNetInit();
	}
}

void UActorReplicationBridge::DestroyInstanceFromRemote(UObject* Instance, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags)
{
	if (DestroyReason == EReplicationBridgeDestroyInstanceReason::DoNotDestroy)
	{
		return;
	}

	if (AActor* Actor = Cast<AActor>(Instance))
	{
		if ((DestroyReason == EReplicationBridgeDestroyInstanceReason::TearOff) && !NetDriver->ShouldClientDestroyTearOffActors())
		{
			if (Actor->GetRemoteRole() == ROLE_Authority)
			{
				Actor->SetRole(ROLE_Authority);
				Actor->SetReplicates(false);

				if (Actor->GetWorld() != nullptr && !IsEngineExitRequested())
				{
					Actor->TornOff();
				}

				NetDriver->NotifyActorTornOff(Actor);
			}
			else
			{
				UE_LOG_ACTORREPLICATIONBRIDGE(Warning, TEXT("Trying to tear off actor which doesn't support it: %s"), ToCStr(GetNameSafe(Actor)));
			}
		}
		else if (EnumHasAnyFlags(DestroyFlags, EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote))
		{
			// Any subobjects have already been detached from ReplicationBridge
			Actor->PreDestroyFromReplication();
			Actor->Destroy(true);
		}
	}
	else
	{
		// If the SubObject is being torn off it is up to owning actor to clean it up properly
		if (DestroyReason == EReplicationBridgeDestroyInstanceReason::TearOff)
		{
			return;
		}

		if (!EnumHasAnyFlags(DestroyFlags, EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote))
		{
			return;
		}

		AActor* Owner = Cast<AActor>(Instance->GetOuter());
		if (ensureMsgf(IsValid(Owner) && !Owner->IsUnreachable(), TEXT("UActorReplicationBridge::DestroyInstanceFromRemote Destroyed subobject after owner %s"), *Instance->GetPathName()))
		{
			Owner->OnSubobjectDestroyFromReplication(Instance);
		}

		Instance->PreDestroyFromReplication();
		Instance->MarkAsGarbage();
	}
}

void UActorReplicationBridge::GetInitialDependencies(FNetRefHandle Handle, FNetDependencyInfoArray& OutDependencies) const
{
	using namespace UE::Net;

	uint32 DependencyCount = 0U;

	// $TODO: Cache create dependencies and do the lookup based on index

	// Handles with static names does not have have any initial dependencies (other than the reference itself)
	if (Handle.IsDynamic())
	{
		UObject* Object = GetReplicatedObject(Handle);
		if (!Object)
		{
			return;
		}

		if (AActor* Actor = Cast<AActor>(Object))
		{
			UObject* Archetype = nullptr;
			UObject* ActorLevel = nullptr;

			// ChildActor's need to be spawned from the ChildActorTemplate otherwise any non-replicated 
			// customized properties will be incorrect on the Client.
			if (UChildActorComponent* CAC = Actor->GetParentComponent())
			{
				Archetype = CAC->GetChildActorTemplate();
			}
			if (Archetype == nullptr)
			{
				Archetype = Actor->GetArchetype();
			}
			ActorLevel = Actor->GetLevel();

			check( Archetype != nullptr );
			check( Actor->NeedsLoadForClient() );			// We have no business sending this unless the client can load
			check( Archetype->NeedsLoadForClient() );		// We have no business sending this unless the client can load

			// Add initial dependencies
	
			// Archetype
			OutDependencies.Emplace(FNetDependencyInfo(GetOrCreateObjectReference(Archetype)));
			
			// Level if it differs from the persistent level
			const bool bUsePersistentLevel = NetDriver->GetWorld()->PersistentLevel == ActorLevel;
			if (!bUsePersistentLevel)
			{
				OutDependencies.Emplace(FNetDependencyInfo(GetOrCreateObjectReference(ActorLevel)));
			}
		}
		else
		{
			// Add initial dependencies

			// SubObjects either have a dependency on their path relative the owner or a reference to their class
			if (const bool bIsNameStableForNetworking = Object->IsNameStableForNetworking())
			{
				OutDependencies.Emplace(FNetDependencyInfo(GetOrCreateObjectReference(Object)));
			}
			else
			{
				OutDependencies.Emplace(FNetDependencyInfo(GetOrCreateObjectReference(Object->GetClass())));
			}
		}
	}
}

void UActorReplicationBridge::SetNetDriver(UNetDriver* const InNetDriver)
{
	Super::SetNetDriver(InNetDriver);

	if (NetDriver)
	{
		NetDriver->OnNetServerMaxTickRateChanged.RemoveAll(this);
	}

	NetDriver = InNetDriver;
	if (InNetDriver != nullptr)
	{
		SetMaxTickRate(static_cast<float>(FPlatformMath::Max(InNetDriver->GetNetServerMaxTickRate(), 0)));

		InNetDriver->OnNetServerMaxTickRateChanged.AddUObject(this, &UActorReplicationBridge::OnMaxTickRateChanged);

		const FName RequiredChannelName = UObjectReplicationBridgeConfig::GetConfig()->GetRequiredNetDriverChannelClassName();
		
		if (!RequiredChannelName.IsNone())
		{
			const bool bRequiredChannelIsConfigured = InNetDriver->ChannelDefinitions.FindByPredicate([RequiredChannelName](const FChannelDefinition& rhs)
				{
					return rhs.ClassName == RequiredChannelName;
				}) != nullptr;

			checkf(bRequiredChannelIsConfigured, TEXT("ObjectReplication needs the netdriver channel %s to work. Add this channel to the netdriver channel definitions config"), *RequiredChannelName.ToString());
		}
	}
}

void UActorReplicationBridge::OnMaxTickRateChanged(UNetDriver* InNetDriver, int32 NewMaxTickRate, int32 OldMaxTickRate)
{
	SetMaxTickRate(static_cast<float>(FPlatformMath::Max(InNetDriver->GetNetServerMaxTickRate(), 0)));

	ReinitPollFrequency();
}

void UActorReplicationBridge::GetActorCreationHeader(const AActor* Actor, UE::Net::Private::FActorCreationHeader& Header) const
{
	Header.bIsActor = true;
	
	UE::Net::FNetObjectReference ActorReference = GetOrCreateObjectReference(Actor);

	Header.bIsDynamic = ActorReference.GetRefHandle().IsDynamic();

	// Dynamic actor?
	if (Header.bIsDynamic)
	{
		// Get creation data

		// This is more or less a straight copy from ClientPackageMap and needs to be updated accordingly
		UObject* Archetype = nullptr;
		UObject* ActorLevel = nullptr;

		// ChildActor's need to be spawned from the ChildActorTemplate otherwise any non-replicated 
		// customized properties will be incorrect on the Client.
		if (UChildActorComponent* CAC = Actor->GetParentComponent())
		{
			Archetype = CAC->GetChildActorTemplate();
		}
		if (Archetype == nullptr)
		{
			Archetype = Actor->GetArchetype();
		}
		ActorLevel = Actor->GetLevel();

		check(Archetype != nullptr);
		check(Actor->NeedsLoadForClient());			// We have no business sending this unless the client can load
		check(Archetype->NeedsLoadForClient());		// We have no business sending this unless the client can load

		// Fill in Header
		Header.ArchetypeReference = GetOrCreateObjectReference(Archetype);
		Header.bUsePersistentLevel = NetDriver->GetWorld()->PersistentLevel == ActorLevel;
		if (!Header.bUsePersistentLevel)
		{
			Header.LevelReference = GetOrCreateObjectReference(ActorLevel);
		}

		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			Header.SpawnInfo.Location = FRepMovement::RebaseOntoZeroOrigin(Actor->GetActorLocation(), Actor);
			Header.SpawnInfo.Rotation = Actor->GetActorRotation();
			Header.SpawnInfo.Scale = Actor->GetActorScale();
			FVector Scale = Actor->GetActorScale();

			if (USceneComponent* AttachParent = RootComponent->GetAttachParent())
			{
				// If this actor is attached, when the scale is serialized on the client, the attach parent property won't be set yet.
				// USceneComponent::SetWorldScale3D (which got called by AActor::SetActorScale3D, which we used to do but no longer).
				// would perform this transformation so that what is sent is relative to the parent. If we don't do this, we will
				// apply the world scale on the client, which will then get applied a second time when the attach parent property is received.
				FTransform ParentToWorld = AttachParent->GetSocketTransform(RootComponent->GetAttachSocketName());
				Scale = Scale * ParentToWorld.GetSafeScaleReciprocal(ParentToWorld.GetScale3D());
			}

			Header.SpawnInfo.Scale = Scale;
			Header.SpawnInfo.Velocity = Actor->GetVelocity();
		}
		else
		{
			Header.SpawnInfo = UE::Net::Private::GetDefaultActorReplicationBridgeSpawnInfo();
		}
	}
	else // Refer by path
	{
		Header.ObjectReference = ActorReference;
	}

	// Custom actor creation data
	{
		constexpr int64 MaxBunchBits = 1024;
		FOutBunch Bunch(MaxBunchBits);
		const_cast<AActor*>(Actor)->OnSerializeNewActor(Bunch);
		Header.CustomCreationDataBitCount = Bunch.GetNumBits();
		if (Header.CustomCreationDataBitCount > 0)
		{
			check(Header.CustomCreationDataBitCount == Bunch.GetNumBits());
			Header.CustomCreationData.SetNumZeroed(Align(Bunch.GetNumBytes(), 4));
			FMemory::Memcpy(Header.CustomCreationData.GetData(), Bunch.GetData(), Bunch.GetNumBytes());
		}
	}
}

void UActorReplicationBridge::GetSubObjectCreationHeader(const UObject* Object, UE::Net::Private::FSubObjectCreationHeader& Header) const
{
	// SubObject
	UE::Net::FNetObjectReference ObjectRef = GetOrCreateObjectReference(Object);

	Header.bIsActor = false;
	Header.bIsDynamic = ObjectRef.GetRefHandle().IsDynamic();
	Header.bUsePersistentLevel = false;
	Header.bIsNameStableForNetworking = Object->IsNameStableForNetworking();

	if (Header.bIsDynamic)
	{
		check(Object->NeedsLoadForClient() );				// We have no business sending this unless the client can load
		check(Object->GetClass()->NeedsLoadForClient());	// We have no business sending this unless the client can load

		// We need to spawn this object on the receiving end, so we include the class
		if (!Header.bIsNameStableForNetworking)
		{
			Header.ObjectClassReference = GetOrCreateObjectReference(Object->GetClass());
		}
	}

	Header.ObjectReference = ObjectRef;
}

bool UActorReplicationBridge::RemapPathForPIE(uint32 ConnectionId, FString& Str, bool bReading) const
{
	if (ConnectionId == UE::Net::InvalidConnectionId)
	{
		return GEngine->NetworkRemapPath(static_cast<UNetConnection*>(nullptr), Str, bReading);
	}
	else
	{
		UObject* UserData = GetReplicationSystem()->GetConnectionUserData(ConnectionId);
		UNetConnection* NetConnection = Cast<UNetConnection>(UserData);
		return GEngine->NetworkRemapPath(NetConnection, Str, bReading);
	}
}

bool UActorReplicationBridge::ObjectLevelHasFinishedLoading(UObject* Object) const
{
	const UWorld* DriverWorld = Object && NetDriver ? NetDriver->GetWorld() : nullptr;
	if (DriverWorld)
	{
		// get the level for the object
		const ULevel* Level = Object->GetTypedOuter<ULevel>();

		if (Level != nullptr && Level != DriverWorld->PersistentLevel)
		{
			return Level->bIsVisible;
		}
	}

	return true;
}

bool UActorReplicationBridge::IsAllowedToDestroyInstance(const UObject* Instance) const
{
	if (AActor* Actor = const_cast<AActor*>(Cast<AActor>(Instance)))
	{
		return !NetDriver || NetDriver->ShouldClientDestroyActor(Actor);
	}

	return true;
}

UActorReplicationBridge* UActorReplicationBridge::Create(UNetDriver* NetDriver)
{
	UActorReplicationBridge* Bridge = NewObject<UActorReplicationBridge>(GetTransientPackage(), UActorReplicationBridge::StaticClass());
	Bridge->SetNetDriver(NetDriver);

	return Bridge;
}

float UActorReplicationBridge::GetPollFrequencyOfRootObject(const UObject* ReplicatedObject) const
{
	const AActor* ReplicatedActor = CastChecked<AActor>(ReplicatedObject);
	float PollFrequency = ReplicatedActor->NetUpdateFrequency;
	GetClassPollFrequency(ReplicatedActor->GetClass(), PollFrequency);
	return PollFrequency;
}

#else //!UE_WITH_IRIS

UActorReplicationBridge::UActorReplicationBridge() = default;
UActorReplicationBridge::~UActorReplicationBridge() = default;

#endif
