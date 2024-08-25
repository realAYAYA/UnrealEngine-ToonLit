// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DevObjectVersion.h"
#include "Containers/Map.h"

// Custom serialization version for changes made in the //Fortnite/Dev-Valkyrie stream
struct FFortniteValkyrieBranchObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Added new serialization for AtomPrimitives
		AtomPrimitiveNewSerialization,

		// Removing FAtomColor struct, now only using the color id.
		AtomDeprecatingRedundantColorStructs,

		// switch the physics implicit objects unique/shared ptrs to be ref counted
		RefCountedOImplicitObjects,

		// Add density to FChaosPhysicsMaterial
		ChaosAddDensityToPhysicsMaterial,

		// improved UX of the exclusion volumes by adding a new mode enum
		WaterBodyExclusionVolumeMode,

		// Fix rest transforms wrongly stored in geometry collection components
		FixRestTransformsInGeometryCollectionComponent,

		// Distinct version required to properly handle legacy serialization of FActorInstanceHandle to avoid hitting replay deserialization perf 
		ActorInstanceHandleSwitchedToInterfaces,

		// Add weight maps to Chaos cloth asset mass and gravity scale
		ChaosClothAssetWeightedMassAndGravity,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	CORE_API const static FGuid GUID;

private:
	FFortniteValkyrieBranchObjectVersion() {}
};
