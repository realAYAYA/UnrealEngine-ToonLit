// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IStageDataProvider;


/**
 * Interface for the stage data provider module.
 */
class STAGEDATAPROVIDER_API IStageDataProviderModule : public IModuleInterface
{
public:

	virtual ~IStageDataProviderModule() = default;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IStageDataProviderModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IStageDataProviderModule>(ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() during shutdown if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

private:
	static const FName ModuleName;
};

