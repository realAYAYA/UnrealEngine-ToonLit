// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"

#if UE_WITH_IRIS

#include "EngineUtils.h"
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
#include "GameFramework/PlayerController.h"

namespace UE::Net
{

UReplicationSystem* FReplicationSystemUtil::GetReplicationSystem(const AActor* Actor)
{
	UNetDriver* NetDriver = Actor ? Actor->GetNetDriver() : nullptr;
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
	// We really should cache the handle in the actor (or UObject if we can afford it)
	if (!Actor || !Actor->GetIsReplicated())
	{
		return FNetHandle();
	}

	UActorReplicationBridge* Bridge = GetActorReplicationBridge(Actor);
	return Bridge ? Bridge->GetReplicatedHandle(Actor) : FNetHandle();
}

FNetHandle FReplicationSystemUtil::GetNetHandle(const UActorComponent* SubObject)
{
	UActorReplicationBridge* Bridge = SubObject ? GetActorReplicationBridge(SubObject->GetOwner()) : nullptr;
	return Bridge ? Bridge->GetReplicatedHandle(SubObject) : FNetHandle();
}

FNetHandle FReplicationSystemUtil::BeginReplicationForActorComponent(FNetHandle ActorHandle, UActorComponent* ActorComp)
{
	if (ActorHandle.IsValid())
	{
		UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(ActorHandle.GetReplicationSystemId());
		if (ReplicationSystem && ReplicationSystem->IsServer())
		{
			if (UActorReplicationBridge* Bridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge()))
			{
				return Bridge->BeginReplication(ActorHandle, ActorComp);
			}
		}
	}
	return FNetHandle();
}

FNetHandle FReplicationSystemUtil::BeginReplicationForActorComponent(const AActor* Actor, UActorComponent* ActorComp)
{
	return FReplicationSystemUtil::BeginReplicationForActorComponent(FReplicationSystemUtil::GetNetHandle(Actor), ActorComp);
}

FNetHandle FReplicationSystemUtil::BeginReplicationForActorSubObject(const AActor* Actor, UObject* ActorSubObject, ELifetimeCondition NetCondition)
{
	if (NetCondition != ELifetimeCondition::COND_Never)
	{
		const FNetHandle OwnerHandle = GetNetHandle(Actor);
		if (OwnerHandle.IsValid())
		{
			UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(OwnerHandle.GetReplicationSystemId());
			if (ReplicationSystem && ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetHandle SubObjectHandle = Bridge->BeginReplication(OwnerHandle, ActorSubObject);
					if (SubObjectHandle.IsValid() && NetCondition != ELifetimeCondition::COND_None)
					{
						Bridge->SetSubObjectNetCondition(SubObjectHandle, NetCondition);
					}
					return SubObjectHandle;
				}
			}
		}
	}

	return FNetHandle();
}

FNetHandle FReplicationSystemUtil::BeginReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject, ELifetimeCondition NetCondition)
{
	const AActor* Actor = ActorComponent->GetOwner();
	if (Actor && NetCondition != ELifetimeCondition::COND_Never)
	{		
		const FNetHandle OwnerHandle = GetNetHandle(ActorComponent->GetOwner());
		if (OwnerHandle.IsValid())
		{
			UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(OwnerHandle.GetReplicationSystemId());
			if (ReplicationSystem && ReplicationSystem->IsServer())
			{
				if (UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetHandle ActorComponentHandle = Bridge->GetReplicatedHandle(ActorComponent);
					if (ActorComponentHandle.IsValid())
					{
						const FNetHandle SubObjectHandle = Bridge->BeginReplication(OwnerHandle, SubObject, ActorComponentHandle, UReplicationBridge::ESubObjectInsertionOrder::ReplicateWith);
						if (NetCondition != ELifetimeCondition::COND_None)
						{
							Bridge->SetSubObjectNetCondition(SubObjectHandle, NetCondition);
						}
						return SubObjectHandle;
					}
				}
			}
		}
	}

	return FNetHandle();
}

void FReplicationSystemUtil::EndReplicationForActorSubObject(const AActor* Actor, UObject* SubObject)
{
	if (UObjectReplicationBridge* Bridge = Cast<UObjectReplicationBridge>(GetActorReplicationBridge(Actor)))
	{
		Bridge->EndReplication(SubObject);
	}
}

void FReplicationSystemUtil::EndReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject)
{
	EndReplicationForActorSubObject(ActorComponent->GetOwner(), SubObject);
}

FNetHandle FReplicationSystemUtil::BeginReplication(AActor* Actor, const FActorBeginReplicationParams& Params)
{
	UReplicationSystem* ReplicationSystem = GetReplicationSystem(Actor);
	if (ReplicationSystem && ReplicationSystem->IsServer())
	{
		if (UActorReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UActorReplicationBridge>())
		{
			return Bridge->BeginReplication(Actor, Params);
		}
	}

	return FNetHandle();
}

FNetHandle FReplicationSystemUtil::BeginReplication(AActor* Actor)
{
	const FActorBeginReplicationParams BeginReplicationParams;
	return BeginReplication(Actor, BeginReplicationParams);
}

void FReplicationSystemUtil::AddDependentActor(const AActor* Parent, AActor* Child)
{
	// Can only add dependent actors on already replicating actors
	FNetHandle ParentHandle = GetNetHandle(Parent);
	if (ensureMsgf(ParentHandle.IsValid(), TEXT("FReplicationSystemUtil::AddDependentActor Parent %s must be replicated"), *GetPathNameSafe(Parent)))
	{
		FNetHandle ChildHandle = BeginReplication(Child);
		if (ensureMsgf(ChildHandle.IsValid(), TEXT("FReplicationSystemUtil::AddDependentActor Child %s must be replicated"), *GetPathNameSafe(Child)))
		{
			UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(ParentHandle.GetReplicationSystemId());
			if (UObjectReplicationBridge* Bridge = ReplicationSystem ? ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>() : nullptr)
			{
				Bridge->AddDependentObject(ParentHandle, ChildHandle);
			}
		}
	}
}

void FReplicationSystemUtil::RemoveDependentActor(const AActor* Parent, AActor* Child)
{
	// Need to find replication system
	FNetHandle ParentHandle = GetNetHandle(Parent);
	FNetHandle ChildHandle = GetNetHandle(Child);
	if (ParentHandle.IsValid() && ChildHandle.IsValid())
	{
		UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(ChildHandle.GetReplicationSystemId());
		if (UObjectReplicationBridge* Bridge = ReplicationSystem ? ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>() : nullptr)
		{
			Bridge->RemoveDependentObject(ParentHandle, ChildHandle);
		}
	}
}

void FReplicationSystemUtil::EndReplication(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{	
	if (UActorReplicationBridge* Bridge = GetActorReplicationBridge(Actor))
	{
		Bridge->EndReplication(Actor, EndPlayReason);
	}
}

void FReplicationSystemUtil::EndReplicationForActorComponent(UActorComponent* SubObject)
{
	if (UActorReplicationBridge* Bridge = GetActorReplicationBridge(SubObject->GetOwner()))
	{
		Bridge->EndReplicationForActorComponent(SubObject);
	}
}

void FReplicationSystemUtil::SetActorComponentNetCondition(const UActorComponent* ActorComponent, ELifetimeCondition Condition)
{
	if (UActorReplicationBridge* Bridge = IsValid(ActorComponent) ? GetActorReplicationBridge(ActorComponent->GetOwner()) : nullptr)
	{
		Bridge->SetSubObjectNetCondition(Bridge->GetReplicatedHandle(ActorComponent), Condition);
	}
}

void FReplicationSystemUtil::SetPropertyCustomCondition(const UObject* Object, uint16 RepIndex, bool bIsActive)
{
	// If this is an actor component we can get the proper bridge through its owner, regardless of whether it's a default replicated component or not.
	if (const UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
	{
		const AActor* Owner = ActorComponent->GetOwner();
		const UActorReplicationBridge* Bridge = GetActorReplicationBridge(Owner);
		if (Bridge == nullptr)
		{
			return;
		}

		// Try to get a handle for the component. This can fail if we incorporate default components in the protocol for the actor.
		FNetHandle NetHandle = Bridge->GetReplicatedHandle(Object);
		if (!NetHandle.IsValid())
		{
			NetHandle = Bridge->GetReplicatedHandle(Owner);
			if (!NetHandle.IsValid())
			{
				return;
			}
		}

		Net::GetReplicationSystem(NetHandle.GetReplicationSystemId())->SetPropertyCustomCondition(NetHandle, Object, RepIndex, bIsActive);
	}
	else if (const AActor* Actor = Cast<AActor>(Object))
	{
		FNetHandle NetHandle = GetNetHandle(Actor);
		if (!NetHandle.IsValid())
		{
			return;
		}

		Net::GetReplicationSystem(NetHandle.GetReplicationSystemId())->SetPropertyCustomCondition(NetHandle, Object, RepIndex, bIsActive);
	}
	else
	{
		checkf(false, TEXT("Unexpected class %s trying to set custom condition."), ToCStr(Object->GetFName().GetPlainNameString()));
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

void FReplicationSystemUtil::NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState)
{
	const ENetDormancy Dormancy = Actor->NetDormancy;
	const bool bIsPendingDormancy = (Dormancy > DORM_Awake);

	FNetHandle NetHandle = GetNetHandle(Actor);
	if (NetHandle.IsValid())
	{
		UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(NetHandle.GetReplicationSystemId());
		if (UObjectReplicationBridge* Bridge = ReplicationSystem ? ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>() : nullptr)
		{
			Bridge->SetObjectWantsToBeDormant(NetHandle, bIsPendingDormancy);
		}
	}
}

void FReplicationSystemUtil::FlushNetDormancy(AActor* Actor, bool bWasDormInitial)
{
	FNetHandle NetHandle = GetNetHandle(Actor);
	if (NetHandle.IsValid())
	{
		UReplicationSystem* ReplicationSystem = Net::GetReplicationSystem(NetHandle.GetReplicationSystemId());
		if (UObjectReplicationBridge* Bridge = ReplicationSystem ? ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>() : nullptr)
		{
			Bridge->ForceUpdateWantsToBeDormantObject(NetHandle);
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
		const UNetDriver* NetDriver = World->GetNetDriver();
		if (UReplicationSystem* ReplicationSystem = NetDriver ? NetDriver->GetReplicationSystem() : nullptr)
		{
			if (UActorReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UActorReplicationBridge>())
			{
				FNetHandle Handle = Bridge->GetReplicatedHandle(SubObject);
				if (Handle.IsValid())
				{
					for (const FName NetGroup : NetSubsystem->GetNetConditionGroupManager().GetSubObjectNetConditionGroups(FObjectKey(SubObject)))
					{
						FNetObjectGroupHandle SubObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(NetGroup);
						ReplicationSystem->AddToGroup(SubObjectGroupHandle, Handle);
					}
				}
			}
		}
	}
}

void FReplicationSystemUtil::RemoveSubObjectGroupMembership(const APlayerController* PC, const FName NetGroup)
{
	UNetConnection* Conn = PC->GetNetConnection();
	if (ensureAlways(Conn))
	{
		if (UReplicationSystem* ReplicationSystem = Conn->GetDriver() ? Conn->GetDriver()->GetReplicationSystem() : nullptr)
		{
			if (!IsSpecialNetConditionGroup(NetGroup))
			{
				ReplicationSystem->SetSubObjectFilterStatus(NetGroup, Conn->GetConnectionId(), ENetFilterStatus::Disallow);
			}
		}
	}
}

void FReplicationSystemUtil::UpdateSubObjectGroupMemberships(const APlayerController* PC)
{
	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		if (UReplicationSystem* ReplicationSystem = Conn->GetDriver() ? Conn->GetDriver()->GetReplicationSystem() : nullptr)
		{
			for (const FName NetGroup : PC->GetNetConditionGroups())
			{
				if (!IsSpecialNetConditionGroup(NetGroup))
				{
					FNetObjectGroupHandle SubObjectGroupHandle = ReplicationSystem->GetOrCreateSubObjectFilter(NetGroup);
					ReplicationSystem->SetSubObjectFilterStatus(NetGroup, Conn->GetConnectionId(), ENetFilterStatus::Allow);
				}
			}
		}
	}
}

}

#endif
