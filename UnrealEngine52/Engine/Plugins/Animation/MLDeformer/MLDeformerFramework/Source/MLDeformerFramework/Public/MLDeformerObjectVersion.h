// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

namespace UE::MLDeformer
{
	// Custom serialization version for backwards compatibility during de-serialization
	struct MLDEFORMERFRAMEWORK_API FMLDeformerObjectVersion
	{
		FMLDeformerObjectVersion() = delete;
	
		enum Type
		{
			// Before any version changes were made.
			BeforeCustomVersionWasAdded,

			// ----- New versions can be added above this line -----
			VersionPlusOne,
			LatestVersion = VersionPlusOne - 1
		};

		// The GUID for this custom version number.
		const static FGuid GUID;
	};
}	// namespace UE::MLDeformer
