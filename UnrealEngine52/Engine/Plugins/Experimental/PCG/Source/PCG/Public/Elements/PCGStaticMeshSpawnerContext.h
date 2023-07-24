// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "InstancePackers/PCGInstancePackerBase.h"

#include "PCGStaticMeshSpawnerContext.generated.h"

class AActor;
class UPCGSpatialData;

struct FPCGInstancesAndWeights
{
	TArray<TArray<FPCGMeshInstanceList>> MeshInstances;
	TArray<int> CumulativeWeights;
};

USTRUCT(BlueprintType)
struct FPCGStaticMeshSpawnerContext : public FPCGContext
{
	GENERATED_BODY()

	struct FPackedInstanceListData
	{
		FPackedInstanceListData();
		~FPackedInstanceListData();
		AActor* TargetActor = nullptr;
		const UPCGSpatialData* SpatialData = nullptr;
		TArray<FPCGMeshInstanceList> MeshInstances;
		TArray<FPCGPackedCustomData> PackedCustomData;
	};

	TArray<FPackedInstanceListData> MeshInstancesData;
	// Index of input in the context (for selection)
	int32 CurrentInputIndex = 0;
	// Index of MeshInstances/PackedCustomData in last MeshInstancesData element
	int32 CurrentDataIndex = 0;

	// Whole-execution variables
	bool bReuseCheckDone = false;
	bool bSkippedDueToReuse = false;

	// Per-input context variables
	bool bCurrentInputSetup = false;
	bool bSelectionDone = false;

	const UPCGPointData* CurrentPointData = nullptr;
	UPCGPointData* CurrentOutputPointData = nullptr;
	FPCGMeshMaterialOverrideHelper MaterialOverrideHelper;
	int32 CurrentPointIndex = 0;

	// Used in by-attribute selector
	TMap<PCGMetadataValueKey, TSoftObjectPtr<UStaticMesh>> ValueKeyToMesh;

	// Used in weighted selector
	TArray<TArray<FPCGMeshInstanceList>> WeightedMeshInstances;
	TMap<TSoftObjectPtr<UStaticMesh>, PCGMetadataValueKey> MeshToValueKey;
	TArray<int> CumulativeWeights;

	// Used in the weighted by category selector
	TMap<PCGMetadataValueKey, FPCGInstancesAndWeights> CategoryEntryToInstancesAndWeights;

	void ResetInputIterationData();
};