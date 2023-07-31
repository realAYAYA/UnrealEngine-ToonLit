// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class USmartObjectComponent;

/**
 * Visualizer for SmartObjectComponent
 */
class SMARTOBJECTSEDITORMODULE_API FSmartObjectComponentVisualizer : public FComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
