// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

// Custom serialization version for SoundSubmixes.
struct ENGINE_API FSoundSubmixCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
			
		// Migrated deprecated properties OutputVolume, WetLevel, DryLevel 
		MigrateModulatedSendProperties, 

		// Convert modulated properties to dB.
		ConvertLinearModulatorsToDb,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSoundSubmixCustomVersion() {}
};
