// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Math/Transform.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

struct FChaosVDParticleDataWrapper;
class FChaosVDScene;

/** Context needed to be able to visualize data in the viewport */
struct FChaosVDVisualizationContext
{
	FTransform SpaceTransform;
	TWeakPtr<FChaosVDScene> CVDScene;
	int32 SolverID = INDEX_NONE;
	uint32 VisualizationFlags = 0;
};

/** Interface to be used by any object that contains Particle Data that needs to be visualized.
 * @note As we are still not settled on if we will stick with having Actors to represent each particle, this interface allows us to abstract the Particle data access
 * instead of using directly AChaosVDParticleActor
 */
class IChaosVDParticleVisualizationDataProvider
{
public:
	virtual ~IChaosVDParticleVisualizationDataProvider() = default;

	virtual void GetVisualizationContext(FChaosVDVisualizationContext& OutVisualizationContext) {}
	virtual const FChaosVDParticleDataWrapper* GetParticleData() { return nullptr; }
};
