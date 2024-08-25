// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassRepresentationFragments.h"

FMassVisualizationLODSharedFragment::FMassVisualizationLODSharedFragment(const FMassVisualizationLODParameters& LODParams)
{
	LODCalculator.Initialize(LODParams.BaseLODDistance, LODParams.BufferHysteresisOnDistancePercentage / 100.f, LODParams.LODMaxCount
		, /*InLODMaxCountPerViewer=*/nullptr, LODParams.DistanceToFrustum, LODParams.DistanceToFrustumHysteresis, LODParams.VisibleLODDistance);
	FilterTag = LODParams.FilterTag;
}

FMassDistanceLODSharedFragment::FMassDistanceLODSharedFragment(const FMassDistanceLODParameters& LODParams)
{
	LODCalculator.Initialize(LODParams.LODDistance, LODParams.BufferHysteresisOnDistancePercentage / 100.f, /*InLODMaxCount=*/nullptr);
	FilterTag = LODParams.FilterTag;
}
