// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputLibrary.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogEnhancedInput, Log, All);

/**
 * The public interface to this module
 */
class IEnhancedInputModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IEnhancedInputModule& Get()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IEnhancedInputModule_Get);
		static IEnhancedInputModule& Singleton = FModuleManager::LoadModuleChecked< IEnhancedInputModule >("EnhancedInput");
		return Singleton;
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IEnhancedInputModule_IsAvailable);
		return FModuleManager::Get().IsModuleLoaded("EnhancedInput");
	}

	virtual UEnhancedInputLibrary* GetLibrary() = 0;

};


