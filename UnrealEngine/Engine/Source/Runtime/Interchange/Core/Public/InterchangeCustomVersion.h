// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Custom serialization version for changes to variant manager objects
struct FInterchangeCustomVersion
{
	enum Type
	{
		// Roughly corresponds to 5.2
		BeforeCustomVersionWasAdded = 0,

		SerializedInterchangeObjectStoring,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	INTERCHANGECORE_API const static FGuid GUID;

private:
	FInterchangeCustomVersion() {}
};