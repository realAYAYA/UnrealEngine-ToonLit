// Copyright Epic Games, Inc. All Rights Reserved.
#include "GrassInstancedStaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GrassInstancedStaticMeshComponent)


static TAutoConsoleVariable<bool> CVarEnableGrassInstancedWPOVelocity(
	TEXT("r.Velocity.EnableLandscapeGrass"),
	true,
	TEXT("Specify if you want to output velocity for the grass component for WPO.\n")
	TEXT(" True (default)\n")
	TEXT(" False")
	);

bool UGrassInstancedStaticMeshComponent::SupportsWorldPositionOffsetVelocity() const 
{ 
	return CVarEnableGrassInstancedWPOVelocity.GetValueOnAnyThread() && Super::SupportsWorldPositionOffsetVelocity();
}
