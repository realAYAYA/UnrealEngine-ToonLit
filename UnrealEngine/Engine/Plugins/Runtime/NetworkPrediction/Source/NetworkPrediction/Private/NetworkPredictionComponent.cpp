// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "NetworkPredictionCheck.h"
#include "NetworkPredictionWorldManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPredictionComponent)

UNetworkPredictionComponent::UNetworkPredictionComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNetworkPredictionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	UWorld* World = GetWorld();	
	UNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (NetworkPredictionWorldManager)
	{
		// Init RepProxies
		ReplicationProxy_ServerRPC.Init(&NetworkPredictionProxy, EReplicationProxyTarget::ServerRPC);
		ReplicationProxy_Autonomous.Init(&NetworkPredictionProxy, EReplicationProxyTarget::AutonomousProxy);
		ReplicationProxy_Simulated.Init(&NetworkPredictionProxy, EReplicationProxyTarget::SimulatedProxy);
		ReplicationProxy_Replay.Init(&NetworkPredictionProxy, EReplicationProxyTarget::Replay);

		InitializeNetworkPredictionProxy();

		CheckOwnerRoleChange();
	}
}

void UNetworkPredictionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	NetworkPredictionProxy.EndPlay();
}

void UNetworkPredictionComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	
	CheckOwnerRoleChange();

	// We have to update our replication proxies so they can be accurately compared against client shadowstate during property replication. ServerRPC proxy does not need to do this.
	ReplicationProxy_Autonomous.OnPreReplication();
	ReplicationProxy_Simulated.OnPreReplication();
	ReplicationProxy_Replay.OnPreReplication();
}

void UNetworkPredictionComponent::PreNetReceive()
{
	Super::PreNetReceive();
	CheckOwnerRoleChange();
}

void UNetworkPredictionComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME( UNetworkPredictionComponent, NetworkPredictionProxy);
	DOREPLIFETIME_CONDITION( UNetworkPredictionComponent, ReplicationProxy_Autonomous, COND_AutonomousOnly);
	DOREPLIFETIME_CONDITION( UNetworkPredictionComponent, ReplicationProxy_Simulated, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION( UNetworkPredictionComponent, ReplicationProxy_Replay, COND_ReplayOnly);
}

void UNetworkPredictionComponent::InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection)
{
	NetworkPredictionProxy.InitForNetworkRole(Role, bHasNetConnection);
}

bool UNetworkPredictionComponent::CheckOwnerRoleChange()
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

bool UNetworkPredictionComponent::ServerReceiveClientInput_Validate(const FServerReplicationRPCParameter& ProxyParameter)
{
	return true;
}

void UNetworkPredictionComponent::ServerReceiveClientInput_Implementation(const FServerReplicationRPCParameter& ProxyParameter)
{
	// The const_cast is unavoidable here because the replication system only allows by value (forces copy, bad) or by const reference. This use case is unique because we are using the RPC parameter as a temp buffer.
	const_cast<FServerReplicationRPCParameter&>(ProxyParameter).NetSerializeToProxy(ReplicationProxy_ServerRPC);
}

void UNetworkPredictionComponent::CallServerRPC()
{
	// Temp hack to make sure the ServerRPC doesn't get suppressed from bandwidth limiting
	// (system hasn't been optimized and not mature enough yet to handle gaps in input stream)
	FScopedBandwidthLimitBypass BandwidthBypass(GetOwner());

	FServerReplicationRPCParameter ProxyParameter(ReplicationProxy_ServerRPC);
	ServerReceiveClientInput(ProxyParameter);
}

// --------------------------------------------------------------


