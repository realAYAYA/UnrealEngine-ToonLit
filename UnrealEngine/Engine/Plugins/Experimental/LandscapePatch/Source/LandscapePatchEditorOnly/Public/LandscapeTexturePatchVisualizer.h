// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ComponentVisualizer.h"

/**
 * Adds a rectangle denoting the area covered by a landscape texture patch when it is selected in editor.
 */
class LANDSCAPEPATCHEDITORONLY_API FLandscapeTexturePatchVisualizer : public FComponentVisualizer
{
private:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};