// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

struct HARMONIXDSP_API FFusionPatchCustomVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded = 0,

		KeyzonesUseMappedParameters = 1,

		PitchShifterNameRedirects = 2,

		PanImportingFromDTAFixed = 3,

		DeprecatedPresets = 4,

		DeprecatedUnusedEffectsSettings = 5,

		DeprecateTypedSettingsArray = 6,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static const FGuid GUID;

private:

	FFusionPatchCustomVersion() {}
};