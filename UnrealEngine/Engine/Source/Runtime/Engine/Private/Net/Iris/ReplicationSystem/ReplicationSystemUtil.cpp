// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"

#if UE_WITH_IRIS

#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/Core/IrisLog.h"
#include "Net/Iris/ReplicationSystem/ActorReplicationBridge.h"
#include "Net/NetSubObjectRegistryGetter.h"
#include "HAL/IConsoleManager.h"
#include "Templates/Casts.h"
#include "Net/Subsystems/NetworkSubsystem.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Net/Core/NetHandle/NetHandleManager.h"
#include "GameFramework/PlayerController.h"
#include "Templates/Function.h"

namespace UE::Net::ReplicationSystemUtil
{
	static void ForEachReplicationSystem(const UEngine* Engine, const UWorld* World, TFunctionRef<void(UReplicationSystem*)> Function)
	{
		if (Engine == nullptr || World == nullptr)
		{
			return;
		}

		if (const FWorldContext* Context = Engine->GetWorldContextFromWorld(World))
		{
			for (const FNamedNetDriver& NamedNetDriver : Context->ActiveNetDrivers)
			{
				if (UNetDriver* NetDriver = NamedNetDriver.NetDriver)
				{
					if (UReplicationSystem* ReplicationSystem = NetDriver->GetReplicationSystem())
					{
						Function(ReplicationSystem);
					}
				}
			}
		}
	}

	static void ForEachReplicationSystem(TFunctionRef<void(UReplicationSystem*)> Function)
	{
		for (UReplicationSystem* ReplicationSystem : FReplicationSystemFactory::GetAllReplicationSystems())
		{
			if (ReplicationSystem != nullptr)
			{
				Function(ReplicationSystem);
			}
		}
	}
}

namespace UE::Net
{

UReplicationSystem* FReplicationSystemUtil::GetReplicationSystem(const AActor* Actor)
{
	UNetDriver* NetDriver = Actor && Actor->GetWorld() ? Actor->GetNetDriver() : nullptr;
	return NetDriver ? NetDriver->GetReplicationSystem() : nullptr;
}

UActorReplicationBridge* FReplicationSystemUtil::GetActorReplicationBridge(const AActor* Actor)
{
	if (UReplicationSystem* ReplicationSystem = GetReplicationSystem(Actor))
	{
		return Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge());
	}
	else
	{
		return nullptr;
	}
}

UActorReplicationBridge* FReplicationSystemUtil::GetActorReplicationBridge(const UNetConnection* NetConnection)
{
	const UNetDriver* Driver = NetConnection ? NetConnection->GetDriver() : nullptr;
	if (const UReplicationSystem* ReplicationSystem = Driver ? Driver->GetReplicationSystem() : nullptr)
	{
		return Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge());
	}
	else
	{
		return nullptr;
	}
}

FNetHandle FReplicationSystemUtil::GetNetHandle(const AActor* Actor)
{
	FNetHandle NetHandle = FNetHandleManager::GetNetHandle(Actor);
	return NetHandle;
}

FNetHandle FReplicationSystemUtil::GetNetHandle(const UActorComponent* SubObject)
{
	FNetHandle NetHandle = FNetHandleManager::GetNetHandle(SubObject);
	return NetHandle;
}

FNetHandle FReplicationSystemUtil::GetNetHandle(const UObject* Object)
{
	FNetHandle NetHandle = FNetHandleManager::GetNetHandle(Object);
	return NetHandle;
}

void FReplicationSystemUtil::BeginReplication(AActor* Actor, const FActorBeginReplicationParams& Params)
{
	if (const UWorld* World = Actor->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [Actor, &Params](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
				{
					Bridge->BeginReplication(Actor, Params);
				}
			}
		});
	}
}

void FReplicationSystemUtil::BeginReplication(AActor* Actor)
{
	const FActorBeginReplicationParams BeginReplicationParams;
	return BeginReplication(Actor, BeginReplicationParams);
}

void FReplicationSystemUtil::EndReplication(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	// If the call is coming from for example destroying an actor then a formerly associated NetRefHandle will no longer be valid.
	// The bridge itself will verify that the actor is replicated by it so there's no reason to check that either.
	ReplicationSystemUtil::ForEachReplicationSystem([Actor, EndPlayReason](UReplicationSystem* ReplicationSystem)
	{
		if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
		{
			Bridge->EndReplication(Actor, EndPlayReason);
		}
	});
}


void FReplicationSystemUtil::BeginReplicationForActorComponent(FNetHandle ActorHandle, UActorComponent* ActorComp)
{
	if (!ActorHandle.IsValid())
	{
		return;
	}

	if (!ensureAlways(ActorComp != nullptr))
	{
		return;
	}

	AActor* Actor = Cast<AActor>(ActorComp->GetOwner());
	ensureAlways(FNetHandleManager::GetNetHandle(Actor) == ActorHandle);
	if (Actor)
	{
		if (const UWorld* World = Actor->GetWorld())
		{
			ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ActorHandle, ActorComp](UReplicationSystem* ReplicationSystem)
			{
				if (ReplicationSystem->IsServer())
				{
					if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
					{
						const FNetRefHandle OwnerRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
						if (OwnerRefHandle.IsValid())
						{
							Bridge->BeginReplication(OwnerRefHandle, ActorComp);
						}
					}
				}
			});
		}
	}
}

void FReplicationSystemUtil::BeginReplicationForActorComponent(const AActor* Actor, UActorComponent* ActorComp)
{
	const FNetHandle ActorHandle = GetNetHandle(Actor);
	// If the actor doesn't have a valid handle we assume it's not replicated by any ReplicationSystem
	if (!ActorHandle.IsValid())
	{
		return;
	}

	if (const UWorld* World = Actor->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ActorHandle, ActorComp](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
				{
					const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
					if (ActorRefHandle.IsValid())
					{
						Bridge->BeginReplication(ActorRefHandle, ActorComp);
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::BeginReplicationForActorSubObject(const AActor* Actor, UObject* ActorSubObject, ELifetimeCondition NetCondition)
{
	if (NetCondition == ELifetimeCondition::COND_Never)
	{
		return;
	}

	// Assume an actor without NetHandle isn't replicated.
	const FNetHandle ActorHandle = GetNetHandle(Actor);
	if (!ActorHandle.IsValid())
	{
		return;
	}

	if (const UWorld* World = Actor->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ActorHandle, ActorSubObject, NetCondition](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
					if (ActorRefHandle.IsValid())
					{
						const FNetRefHandle SubObjectRefHandle = Bridge->BeginReplication(ActorRefHandle, ActorSubObject);
						if (SubObjectRefHandle.IsValid() && NetCondition != ELifetimeCondition::COND_None)
						{
							Bridge->SetSubObjectNetCondition(SubObjectRefHandle, NetCondition);
						}
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::BeginReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject, ELifetimeCondition NetCondition)
{
	const AActor* Actor = ActorComponent->GetOwner();
	if (Actor && NetCondition != ELifetimeCondition::COND_Never)
	{		
		const FNetHandle ActorHandle = GetNetHandle(Actor);
		if (!ActorHandle.IsValid())
		{
			return;
		}

		if (const UWorld* World = Actor->GetWorld())
		{
			ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ActorHandle, ActorComponent, SubObject, NetCondition](UReplicationSystem* ReplicationSystem)
			{
				if (ReplicationSystem->IsServer())
				{
					if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
					{
						const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
						const FNetRefHandle ActorComponentRefHandle = Bridge->GetReplicatedRefHandle(ActorComponent);
						if (ActorRefHandle.IsValid() && ActorComponentRefHandle.IsValid())
						{
							const FNetRefHandle SubObjectRefHandle = Bridge->BeginReplication(ActorRefHandle, SubObject, ActorComponentRefHandle, UReplicationBridge::ESubObjectInsertionOrder::ReplicateWith);
							if (SubObjectRefHandle.IsValid() && NetCondition != ELifetimeCondition::COND_None)
							{
								Bridge->SetSubObjectNetCondition(SubObjectRefHandle, NetCondition);
							}
						}
					}
				}
			});
		}
	}
}

void FReplicationSystemUtil::EndReplicationForActorComponent(UActorComponent* ActorComponent)
{
	ReplicationSystemUtil::ForEachReplicationSystem([ActorComponent](UReplicationSystem* ReplicationSystem)
	{
		if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
		{
			constexpr EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId;
			Bridge->EndReplicationForActorComponent(ActorComponent, EndReplicationFlags);
		}
	});
}

void FReplicationSystemUtil::EndReplicationForActorSubObject(const AActor* Actor, UObject* SubObject)
{
	ReplicationSystemUtil::ForEachReplicationSystem([SubObject](UReplicationSystem* ReplicationSystem)
	{
		if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
		{				
			constexpr EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::Destroy | EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId;
			Bridge->EndReplication(SubObject, EndReplicationFlags);
		}
	});
}

void FReplicationSystemUtil::EndReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject)
{
	EndReplicationForActorSubObject(ActorComponent->GetOwner(), SubObject);
}

void FReplicationSystemUtil::AddDependentActor(const AActor* Parent, AActor* Child, EDependentObjectSchedulingHint SchedulingHint)
{
	// Can only add dependent actors on already replicating actors
	FNetHandle ParentHandle = GetNetHandle(Parent);
	if (!ensureMsgf(ParentHandle.IsValid(), TEXT("FReplicationSystemUtil::AddDependentActor Parent %s must be replicated"), *GetPathNameSafe(Parent)))
	{
		return;
	}

	if (const UWorld* World = Parent->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ParentHandle, Child, SchedulingHint](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UActorReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UActorReplicationBridge>())
				{
					const FNetRefHandle ParentRefHandle = Bridge->GetReplicatedRefHandle(ParentHandle);
					if (ParentRefHandle.IsValid())
					{
						FNetRefHandle ChildRefHandle = Bridge->GetReplicatedRefHandle(Child);
						if (!ChildRefHandle.IsValid())
						{
							const FActorBeginReplicationParams BeginReplicationParams;
							ChildRefHandle = Bridge->BeginReplication(Child, BeginReplicationParams);
						}
						if (ensureMsgf(ChildRefHandle.IsValid(), TEXT("FReplicationSystemUtil::AddDependentActor Child %s must be replicated"), *GetPathNameSafe(Child)))
						{
							Bridge->AddDependentObject(ParentRefHandle, ChildRefHandle, SchedulingHint);
						}
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::AddDependentActor(const AActor* Parent, AActor* Child)
{
	AddDependentActor(Parent, Child, EDependentObjectSchedulingHint::Default);
}

void FReplicationSystemUtil::RemoveDependentActor(const AActor* Parent, AActor* Child)
{
	const FNetHandle ParentHandle = GetNetHandle(Parent);
	const FNetHandle ChildHandle = GetNetHandle(Child);
	if (!ParentHandle.IsValid() || !ChildHandle.IsValid())
	{
		return;
	}

	if (const UWorld* World = Parent->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ParentHandle, ChildHandle](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetRefHandle ParentRefHandle = Bridge->GetReplicatedRefHandle(ParentHandle);
					if (ParentRefHandle.IsValid())
					{
						const FNetRefHandle ChildRefHandle = Bridge->GetReplicatedRefHandle(ChildHandle);
						Bridge->RemoveDependentObject(ParentRefHandle, ChildRefHandle);
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::SetActorComponentNetCondition(const UActorComponent* ActorComponent, ELifetimeCondition NetCondition)
{
	if (!IsValid(ActorComponent))
	{
		return;
	}

	const AActor* Actor = ActorComponent->GetOwner();
	const FNetHandle ActorHandle = GetNetHandle(Actor);
	if (!ActorHandle.IsValid())
	{
		return;
	}

	if (const UWorld* World = Actor->GetWorld())
	{
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [ActorComponent, NetCondition](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetRefHandle ActorComponentRefHandle = Bridge->GetReplicatedRefHandle(ActorComponent);
					if (ActorComponentRefHandle.IsValid())
					{
						Bridge->SetSubObjectNetCondition(ActorComponentRefHandle, NetCondition);
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::BeginReplicationForActorsInWorld(UWorld* World)
{
	// We only do this if the world already is initialized
	// Normally we rely on Begin/EndPlay to control if an actor is replicated or not.
	if (World && World->bIsWorldInitialized)
	{
		for (FActorIterator Iter(World); Iter; ++Iter)
		{
			AActor* Actor = *Iter;
			if (IsValid(Actor) && Actor->HasActorBegunPlay() && ULevel::IsNetActor(Actor))
			{
				Actor->BeginReplication();
			}
		}
	}
}

void FReplicationSystemUtil::NotifyActorDormancyChange(UReplicationSystem* ReplicationSystem, AActor* Actor, ENetDormancy OldDormancyState)
{
	if (!ReplicationSystem || !ReplicationSystem->IsServer())
	{
		return;
	}

	const ENetDormancy Dormancy = Actor->NetDormancy;
	const bool bIsPendingDormancy = (Dormancy > DORM_Awake);

	if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
	{
		const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(Actor);
		if (ActorRefHandle.IsValid())
		{
			Bridge->SetObjectWantsToBeDormant(ActorRefHandle, bIsPendingDormancy);
		}
	}
}

void FReplicationSystemUtil::FlushNetDormancy(UReplicationSystem* ReplicationSystem, AActor* Actor, bool bWasDormInitial)
{
	if (!ReplicationSystem || !ReplicationSystem->IsServer())
	{
		return;
	}

	if (!Actor->IsActorInitialized())
	{
		UE_LOG(LogIris, Verbose, TEXT("FReplicationSystemUtil::FlushNetDormancy called on %s that isn't fully initialized yet. Ingoring."), ToCStr(GetFullNameSafe(Actor)));
		return;
	}

	FNetHandle ActorHandle = GetNetHandle(Actor);
	if (ActorHandle.IsValid())
	{
		if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
		{
			const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
			if (ActorRefHandle.IsValid())
			{
				Bridge->ForceUpdateWantsToBeDormantObject(ActorRefHandle);
			}
		}
	}
	else
	{
		UE_CLOG(!Actor->GetIsReplicated(), LogIris, Warning, TEXT("FReplicationSystemUtil::FlushNetDormancy Actor %s that is not replicated"), ToCStr(Actor->GetName()));
		UE_CLOG(!bWasDormInitial, LogIris, Display, TEXT("FReplicationSystemUtil::FlushNetDormancy For not replicated Actor %s is not initially dormant"), ToCStr(Actor->GetName()));

		if (Actor->GetIsReplicated())
		{
 			Actor->BeginReplication();
		}
	}
}

void FReplicationSystemUtil::UpdateSubObjectGroupMemberships(const UObject* SubObject, const UWorld* World)
{
	if (const UNetworkSubsystem* NetSubsystem = World ? World->GetSubsystem<UNetworkSubsystem>() : nullptr)
	{
		FObjectKey SubObjectKey(SubObject);
		ReplicationSystemUtil::ForEachReplicationSystem(GEngine, World, [NetSubsystem, SubObject, SubObjectKey](UReplicationSystem* ReplicationSystem)
		{
			if (ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(SubObject);
					if (RefHandle.IsValid())
					{
						for (const FName NetGroup : NetSubsystem->GetNetConditionGroupManager().GetSubObjectNetConditionGroups(SubObjectKey))
						{
							FNetObjectGroupHandle SubObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(NetGroup);
							ReplicationSystem->AddToGroup(SubObjectGroupHandle, RefHandle);
						}
					}
				}
			}
		});
	}
}

void FReplicationSystemUtil::RemoveSubObjectGroupMembership(const APlayerController* PC, const FName NetGroup)
{
	if (IsSpecialNetConditionGroup(NetGroup))
	{
		return;
	}
	
	// We assume the player controller is tied to a single connection.
	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		if (UReplicationSystem* ReplicationSystem = Conn->GetDriver() ? Conn->GetDriver()->GetReplicationSystem() : nullptr)
		{
			ReplicationSystem->SetSubObjectFilterStatus(NetGroup, Conn->GetConnectionId(), ENetFilterStatus::Disallow);
		}
	}
}

void FReplicationSystemUtil::UpdateSubObjectGroupMemberships(const APlayerController* PC)
{
	// We assume the player controller is tied to a single connection.
	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		if (UReplicationSystem* ReplicationSystem = Conn->GetDriver() ? Conn->GetDriver()->GetReplicationSystem() : nullptr)
		{
			const uint32 ConnId = Conn->GetParentConnectionId();
			for (const FName NetGroup : PC->GetNetConditionGroups())
			{
				if (!IsSpecialNetConditionGroup(NetGroup))
				{
					FNetObjectGroupHandle SubObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(NetGroup);
					ReplicationSystem->SetSubObjectFilterStatus(NetGroup, ConnId, ENetFilterStatus::Allow);
				}
			}
		}
	}
}

void FReplicationSystemUtil::SetReplicationCondition(FNetHandle NetHandle, EReplicationCondition Condition, bool bEnableCondition)
{
	ReplicationSystemUtil::ForEachReplicationSystem([NetHandle, Condition, bEnableCondition](UReplicationSystem* ReplicationSystem)
	{
		if (ReplicationSystem->IsServer())
		{
			if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(NetHandle);
				if (RefHandle.IsValid())
				{
					ReplicationSystem->SetReplicationCondition(RefHandle, Condition, bEnableCondition);
				}
			}
		}
	});
}

void FReplicationSystemUtil::SetStaticPriority(const AActor* Actor, float Priority)
{
	FNetHandle ActorHandle = GetNetHandle(Actor);
	if (!ActorHandle.IsValid())
	{
		return;
	}
	
	ReplicationSystemUtil::ForEachReplicationSystem([ActorHandle, Priority](UReplicationSystem* ReplicationSystem)
	{
		if (ReplicationSystem->IsServer())
		{
			if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				const FNetRefHandle ActorRefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
				ReplicationSystem->SetStaticPriority(ActorRefHandle, Priority);
			}
		}
	});
}

void FReplicationSystemUtil::SetCullDistanceSqrOverride(const AActor* Actor, float CullDistSqr)
{
	FNetHandle ActorHandle = GetNetHandle(Actor);
	if (!ActorHandle.IsValid())
	{
		return;
	}
	
	ReplicationSystemUtil::ForEachReplicationSystem([ActorHandle, CullDistSqr](UReplicationSystem* ReplicationSystem)
	{
		if (ReplicationSystem->IsServer())
		{
			if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
				if (RefHandle.IsValid())
				{
					ReplicationSystem->SetCullDistanceSqrOverride(RefHandle, CullDistSqr);
				}
			}
		}
	});
}

void FReplicationSystemUtil::ClearCullDistanceSqrOverride(const AActor* Actor)
{
	FNetHandle ActorHandle = GetNetHandle(Actor);
	if (!ActorHandle.IsValid())
	{
		return;
	}
	
	ReplicationSystemUtil::ForEachReplicationSystem([ActorHandle](UReplicationSystem* ReplicationSystem)
	{
		if (ReplicationSystem->IsServer())
		{
			if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(ActorHandle);
				if (RefHandle.IsValid())
				{
					ReplicationSystem->ClearCullDistanceSqrOverride(RefHandle);
				}
			}
		}
	});
}

void FReplicationSystemUtil::SetPollFrequency(const UObject* Object, float PollFrequency)
{
	FNetHandle NetHandle = GetNetHandle(Object);
	if (!NetHandle.IsValid())
	{
		return;
	}
	
	ReplicationSystemUtil::ForEachReplicationSystem([NetHandle, PollFrequency](UReplicationSystem* ReplicationSystem)
	{
		if (ReplicationSystem->IsServer())
		{
			if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(NetHandle);
				if (RefHandle.IsValid())
				{
					Bridge->SetPollFrequency(RefHandle, PollFrequency);
				}
			}
		}
	});
}

}

#endif
