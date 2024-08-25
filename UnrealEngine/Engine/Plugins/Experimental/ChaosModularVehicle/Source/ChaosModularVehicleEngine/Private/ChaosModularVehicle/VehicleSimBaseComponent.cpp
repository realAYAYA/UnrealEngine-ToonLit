// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimBaseComponent.h"


UVehicleSimBaseComponent::UVehicleSimBaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TransformIndex = 0;
	bRemoveFromClusterCollisionModel = false;
	TreeIndex = -1;
	bAnimationEnabled = false;
}

