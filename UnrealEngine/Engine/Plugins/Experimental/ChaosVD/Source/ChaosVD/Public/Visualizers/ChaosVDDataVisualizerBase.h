// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/Interface.h"

#include "ChaosVDDataVisualizerBase.generated.h"

class FSceneView;
class FPrimitiveDrawInterface;

/** Interface to be used by any object that contains a visualizer*/
UINTERFACE(MinimalAPI)
class UChaosVDVisualizerContainerInterface : public UInterface
{
	GENERATED_BODY()
};

class IChaosVDVisualizerContainerInterface
{
	GENERATED_BODY()
public:
	virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI){};
};

/**
 * Base class that represents a Visualizer (anything that will debug draw in the Chaos VD viewport).
 * It allows add visualizers to anything and has a way limited API, just with what we need.
 * @note UE Components Visualizers should also be supported and can be used instead if needed.
 */
class FChaosVDDataVisualizerBase
{
public:
	FChaosVDDataVisualizerBase()
	{
	}

	virtual ~FChaosVDDataVisualizerBase() = default;

	virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) {};

	/** Replaces the local visualization flags with new ones
	 * @param NewFlags Flags to use as replacement
	 */
	void UpdateVisualizationFlags(uint32 NewFlags) { LocalVisualizationFlags = NewFlags; }

protected:

	/** Flags to override the Global visualization flags.
	 * Chaos VD has a set of global visualization flags that affect all objects, but objects can override these by setting their own flags.
	 */
	uint32 LocalVisualizationFlags = 0;
};
