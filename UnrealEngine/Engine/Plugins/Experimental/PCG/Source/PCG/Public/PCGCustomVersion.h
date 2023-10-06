// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

struct FGuid;

// Custom serialization version for assets/classes in the PCG plugin
struct PCG_API FPCGCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Split projection nodes inputs to separate source edges and target edge
		SplitProjectionNodeInputs = 1,
		
		MoveSelfPruningParamsOffFirstPin = 2,

		MoveParamsOffFirstPinDensityNodes = 3,

		// Split samplers to give a sampling shape and a bounding shape inputs
		SplitSamplerNodesInputs = 4,

		MovePointFilterParamsOffFirstPin = 5,

		// Add param pin for all nodes that have override and were using the default input pin.
		AddParamPinToOverridableNodes = 6,

		// Sampling shape and bounding shape inputs.
		SplitVolumeSamplerNodeInputs = 7,

		// Renamed spline input and added bounding shape input.
		SplineSamplerUpdatedNodeInputs = 8,

		// Renamed params to override.
		RenameDefaultParamsToOverride = 9,

		// Behavior change for SplineSampler which now defaults to being bounded
		SplineSamplerBoundedByDefault = 10,

		// StaticMeshSpawner now defaults to modify point bounds based on StaticMesh bounds
		StaticMeshSpawnerApplyMeshBoundsToPointsByDefault = 11,

		// Update of Input Selectors. Previous versions should default on @LastCreated
		UpdateAttributePropertyInputSelector = 12,

		// Difference node now iterates on the source pin and unions the differences pin
		DifferenceNodeIterateOnSourceAndUnionDifferences = 13,

		// Update AddAttribute with selectors
		UpdateAddAttributeWithSelectors = 14,

		// Update TransferAttribute with selectors
		UpdateTransferAttributeWithSelectors = 15,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FPCGCustomVersion() {}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#endif
