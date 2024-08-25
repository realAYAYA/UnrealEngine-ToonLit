// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Interfaces/ITargetPlatformSettings.h"

/**
 * Interface for target platform modules.
 */
class ITargetPlatformSettingsModule
	: public IModuleInterface
{

public:

	/** Virtual destructor. */
	virtual ~ITargetPlatformSettingsModule()
	{
		for (ITargetPlatformSettings* TP : AllTargetPlatforms)
		{
			delete TP;
		}
		AllTargetPlatforms.Empty();
	}

	/**
	 * Gets the module's target platforms. This should be overridden by each platform, but
	 * currently, we are re-using the single internal GetTargetPlatform method the old TPModules will implement
	 *
	 * @return The target platform.
	 */
	TArray<ITargetPlatformSettings*> GetTargetPlatformSettings()
	{
		if (AllTargetPlatforms.Num() == 0)
		{
			GetTargetPlatformSettings(AllTargetPlatforms);
		}

		return AllTargetPlatforms;
	}

protected:

	/**
	 * This is where each platform module will fill out an array
	*/
	virtual void GetTargetPlatformSettings(TArray<ITargetPlatformSettings*>& TargetPlatforms) = 0;

private:
	/** Holds the target platforms. */
	TArray<ITargetPlatformSettings*> AllTargetPlatforms;
};
