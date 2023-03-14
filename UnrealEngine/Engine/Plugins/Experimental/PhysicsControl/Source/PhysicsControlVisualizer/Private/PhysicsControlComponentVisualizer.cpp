// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponentVisualizer.h"
#include "PhysicsControlComponent.h"

//======================================================================================================================
void FPhysicsControlComponentVisualizer::DrawVisualization(
	const UActorComponent*   Component,
	const FSceneView*        View,
	FPrimitiveDrawInterface* PDI)
{
	if (const UPhysicsControlComponent* PCC = Cast<const UPhysicsControlComponent>(Component))
	{
		PCC->DebugDraw(PDI);
	}
}
