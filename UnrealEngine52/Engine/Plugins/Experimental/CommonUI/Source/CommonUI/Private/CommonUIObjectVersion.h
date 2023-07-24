// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

// Custom serialization version for changes made to the CommonUI module
namespace ECommonUIObjectVersion
{
	enum Type
	{
		Initial = 0,

		// Updated the UCommonUILoadGuard to expose its content in a ULoadGuardSlot
		CreatedLoadGuardSlot,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const extern FGuid Guid;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreTypes.h"
#endif
