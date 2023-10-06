// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

struct FGuid;

// Custom serialization version for changes made for a private stream
struct FReflectionCaptureObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Allows uncompressed reflection captures for cooked builds
		MoveReflectionCaptureDataToMapBuildData,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	CORE_API const static FGuid GUID;

private:
	FReflectionCaptureObjectVersion() {}
};
