// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Interfaces/ITargetPlatform.h"

/**
 * Interface for target platform modules.
 */
class ITargetPlatformModule
	: public IModuleInterface
{

public:

	/** Virtual destructor. */
	virtual ~ITargetPlatformModule()
	{
		for (ITargetPlatform* TP : AllTargetPlatforms)
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
	TArray<ITargetPlatform*> GetTargetPlatforms()
	{
		if (AllTargetPlatforms.Num() == 0)
		{
			GetTargetPlatforms(AllTargetPlatforms);
		}

		return AllTargetPlatforms;
	}

protected:

	/**
	 * This is where each platform module will fill out an array
	*/
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) = 0;

private:
	/** Holds the target platforms. */
	TArray<ITargetPlatform*> AllTargetPlatforms;
};
