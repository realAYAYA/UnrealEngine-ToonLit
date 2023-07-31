// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Profile/IMediaProfileManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMediaFrameworkUtilities, Log, All);

class IMediaProfileManager;

/**
 * Implements the MediaFrameworkUtilitiesModule module.
 */
class IMediaFrameworkUtilitiesModule : public IModuleInterface
{
public:
	virtual IMediaProfileManager& GetProfileManager() = 0;
};
