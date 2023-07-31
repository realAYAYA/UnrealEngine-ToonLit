// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for backwards compatibility during de-serialization
struct IKRIG_API FIKRigObjectVersion
{
	FIKRigObjectVersion() = delete;
	
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,

		// Retarget pose quaternions changed from pre to post multiplied
		RetargetPoseQuatPostMultiplied,

		// Chain settings moved to struct to be used in profiles
		ChainSettingsConvertedToStruct,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;
};
