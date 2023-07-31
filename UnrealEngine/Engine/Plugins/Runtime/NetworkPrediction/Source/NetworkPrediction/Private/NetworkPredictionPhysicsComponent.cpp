// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionPhysicsComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "NetworkPredictionCheck.h"
#include "NetworkPredictionPhysics.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionPhysics.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Object.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPredictionPhysicsComponent)

NP_MODEL_REGISTER(FGenericPhysicsModelDef);

UNetworkPredictionPhysicsComponent::UNetworkPredictionPhysicsComponent()
{
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);
}

void UNetworkPredictionPhysicsComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	if (!UpdatedPrimitive)
	{
		// Auto-register owner's root component if found.
		if (AActor* MyActor = GetOwner())
		{
			if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(MyActor->GetRootComponent()))
			{
				SetPrimitiveComponent(RootPrimitive);
			}
			else if (UPrimitiveComponent* FoundPrimitive = MyActor->FindComponentByClass<UPrimitiveComponent>())
			{
				SetPrimitiveComponent(FoundPrimitive);
			}
			else
			{
				npEnsureMsgf(false, TEXT("No PrimitiveComponent found on %s"), *MyActor->GetPathName());
			}
		}
	}

	UWorld* World = GetWorld();	
	UNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (NetworkPredictionWorldManager)
	{
		// Init RepProxies
		ReplicationProxy.Init(&NetworkPredictionProxy, EReplicationProxyTarget::SimulatedProxy);

		// Init NP proxy with generic physics def
		InitializeNetworkPredictionProxy();

		CheckOwnerRoleChange();
	}
}

void UNetworkPredictionPhysicsComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FGenericPhysicsModelDef>(GetWorld(), GetReplicationProxies(), nullptr, UpdatedPrimitive);
}

void UNetworkPredictionPhysicsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	NetworkPredictionProxy.EndPlay();
}

void UNetworkPredictionPhysicsComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	
	CheckOwnerRoleChange();
	ReplicationProxy.OnPreReplication();
	
}

void UNetworkPredictionPhysicsComponent::PreNetReceive()
{
	Super::PreNetReceive();
	CheckOwnerRoleChange();
}

void UNetworkPredictionPhysicsComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME( UNetworkPredictionPhysicsComponent, NetworkPredictionProxy);
	DOREPLIFETIME_CONDITION( UNetworkPredictionPhysicsComponent, ReplicationProxy, COND_None);
}

void UNetworkPredictionPhysicsComponent::InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection)
{
	NetworkPredictionProxy.InitForNetworkRole(Role, bHasNetConnection);
}

bool UNetworkPredictionPhysicsComponent::CheckOwnerRoleChange()
{
	AActor* OwnerActor = GetOwner();
	const ENetRole CurrentRole = OwnerActor->GetLocalRole();
	const bool bHasNetConnection = OwnerActor->GetNetConnection() != nullptr;
	
	if (CurrentRole != NetworkPredictionProxy.GetCachedNetRole() || bHasNetConnection != NetworkPredictionProxy.GetCachedHasNetConnection())
	{
		InitializeForNetworkRole(CurrentRole, bHasNetConnection);
		return true;
	}

	return false;
}

void UNetworkPredictionPhysicsComponent::SetPrimitiveComponent(UPrimitiveComponent* NewUpdatedPrimitive)
{
	// Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
	UpdatedPrimitive = IsValid(NewUpdatedPrimitive) ? NewUpdatedPrimitive : nullptr;

	if (UpdatedPrimitive)
	{
		PhysicsActorHandle = UpdatedPrimitive->BodyInstance.GetPhysicsActorHandle();
	}
}
