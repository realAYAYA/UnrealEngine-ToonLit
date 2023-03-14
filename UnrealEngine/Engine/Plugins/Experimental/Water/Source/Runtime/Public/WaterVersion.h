// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


// Custom serialization version for Water plugin
struct WATER_API FWaterCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Refactor of AWaterBody into sub-classes, waves refactor, etc.
		WaterBodyRefactor,
		// Transfer of TerrainCarvingSettings from landmass to water
		MoveTerrainCarvingSettingsToWater,
		// WaterBrushManager now can specify its own brush materials instead of using those from the default water editor settings : 
		MoveBrushMaterialsToWaterBrushManager,
		// Deprecate pontoons data on UBuoyancyComponent
		UpdateBuoyancyComponentPontoonsData,
		// Move JumpFlood materials into AWaterBrushManager
		MoveJumpFloodMaterialsToWaterBrushManager,
		// Fixup Gerstner waves that were not recomputed at the right moment
		FixupUnserializedGerstnerWaves,
		// Move responsability of updating the Water MPC params to WaterMesh
		MoveWaterMPCParamsToWaterMesh,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FWaterCustomVersion() {}
};