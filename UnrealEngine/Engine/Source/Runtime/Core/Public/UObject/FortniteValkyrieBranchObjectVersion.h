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

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	CORE_API const static FGuid GUID;

private:
	FFortniteValkyrieBranchObjectVersion() {}
};
