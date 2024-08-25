// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

struct INSTANCEDACTORS_API FInstancedActorsCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		InitialVersion = 0,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FInstancedActorsCustomVersion() {}
};
