// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"


struct FDisplayClusterCustomVersion
{
	FDisplayClusterCustomVersion() = delete;

	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		ComponentParentChange_4_27,
		
		// -----<new versions can be added above this line>------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;
};
