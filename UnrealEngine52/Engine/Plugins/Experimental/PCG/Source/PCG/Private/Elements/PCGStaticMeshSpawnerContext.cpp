// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGStaticMeshSpawnerContext.h"

void FPCGStaticMeshSpawnerContext::ResetInputIterationData()
{
	bCurrentInputSetup = false;
	bSelectionDone = false;
	CurrentPointData = nullptr;
	CurrentOutputPointData = nullptr;
	MaterialOverrideHelper.Reset();
	CurrentPointIndex = 0;
	ValueKeyToMesh.Reset();
	WeightedMeshInstances.Reset();
	MeshToValueKey.Reset();
	CumulativeWeights.Reset();
	CategoryEntryToInstancesAndWeights.Reset();
}