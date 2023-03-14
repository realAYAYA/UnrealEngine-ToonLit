// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace DisasterRecoveryUtil
{
	/** Returns the container name for the settings. */
	static inline FName GetSettingsContainerName() { return "Project"; }

	/** Returns the category name for the settings. */
	static inline FName GetSettingsCategoryName() { return "Plugins"; }

	/** Returns the section name for the settings. */
	static inline FName GetSettingsSectionName() { return "Disaster Recovery"; }
}
