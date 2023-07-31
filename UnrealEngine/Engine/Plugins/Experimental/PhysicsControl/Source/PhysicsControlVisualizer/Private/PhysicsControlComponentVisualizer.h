// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"

class PHYSICSCONTROLVISUALIZER_API FPhysicsControlComponentVisualizer : public FComponentVisualizer
{
private:
	virtual void DrawVisualization(
		const UActorComponent*   Component, 
		const FSceneView*        View, 
		FPrimitiveDrawInterface* PDI) override;
};