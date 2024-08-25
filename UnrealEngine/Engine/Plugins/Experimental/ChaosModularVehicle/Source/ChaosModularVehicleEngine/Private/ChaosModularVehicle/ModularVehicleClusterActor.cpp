// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleClusterActor.h"
#include "ChaosModularVehicle/ClusterUnionVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"


AModularVehicleClusterActor::AModularVehicleClusterActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ClusterUnionVehicleComponent = CreateDefaultSubobject<UClusterUnionVehicleComponent>(TEXT("ClusterUnionVehicleComponent0"));
	SetRootComponent(ClusterUnionVehicleComponent);

	// Vehicle sim piggy-backs off of the cluster union component & its events
	VehicleSimComponent = CreateDefaultSubobject<UModularVehicleBaseComponent>(TEXT("VehicleSimComponent0"));
	VehicleSimComponent->SetClusterComponent(ClusterUnionVehicleComponent);

	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	SetReplicatingMovement(true);
}
