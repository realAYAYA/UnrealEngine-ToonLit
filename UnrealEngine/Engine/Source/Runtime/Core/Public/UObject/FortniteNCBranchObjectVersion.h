// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DevObjectVersion.h"
#include "Containers/Map.h"

// Custom serialization version for changes made in the //Fortnite/Dev-NC stream
struct CORE_API FFortniteNCBranchObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Added FWorldDataLayersActorDesc
		AddedWorldDataLayersActorDesc,

		// Fixed FDataLayerInstanceDesc
		FixedDataLayerInstanceDesc,

		// Serialize DataLayerAssets in WorldPartitionActorDesc
		WorldPartitionActorDescSerializeDataLayerAssets,

		// Remapped bEvaluateWorldPositionOffset to bEvaluateWorldPositionOffsetInRayTracing
		RemappedEvaluateWorldPositionOffsetInRayTracing,

		// Serialize native and base class for actor descriptors
		WorldPartitionActorDescNativeBaseClassSerialization,

		// Serialize tags for actor descriptors
		WorldPartitionActorDescTagsSerialization,

		// Serialize property map for actor descriptors
		WorldPartitionActorDescPropertyMapSerialization,

		// Added ability to mark shapes as probes
		AddShapeIsProbe,

		// Transfer PhysicsAsset SolverSettings (iteration counts etc) to new structure
		PhysicsAssetNewSolverSettings,
		
		// Chaos GeometryCollection now saves levels attribute values
		ChaosGeometryCollectionSaveLevelsAttribute,


		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

	static TMap<FGuid, FGuid> GetSystemGuids();

private:
	FFortniteNCBranchObjectVersion() {}
};