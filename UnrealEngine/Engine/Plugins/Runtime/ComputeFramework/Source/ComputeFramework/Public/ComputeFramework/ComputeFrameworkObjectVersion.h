// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

struct COMPUTEFRAMEWORK_API FComputeFrameworkObjectVersion
{
	// Not instantiable.
	FComputeFrameworkObjectVersion() = delete;

	enum Type
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;
};