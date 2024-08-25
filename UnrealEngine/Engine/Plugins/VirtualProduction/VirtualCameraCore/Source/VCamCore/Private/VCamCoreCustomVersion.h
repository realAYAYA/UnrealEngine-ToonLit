// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"

namespace UE::VCamCore
{
	struct FVCamCoreCustomVersion
	{
		enum Type
		{
			/** Before any version changes were made in the plugin */
			BeforeCustomVersionWasAdded = 0,

			/**
			 * The following properties were changed (for 5.2):
			 * - UVCamComponent::TargetViewport was moved to UVCamOutputProviderBase
			 * - UVCamComponent::bLockViewportToCamera was reworked to work for multiple viewports in FVCamViewportLocker (now part of UVCamComponent).
			 */
			MoveTargetViewportFromComponentToOutput = 1,

			// -----<new versions can be added above this line>-------------------------------------------------
			VersionPlusOne,
			LatestVersion = VersionPlusOne - 1
		};

		const static FGuid GUID;

	private:
		FVCamCoreCustomVersion() = delete;
	};
}
