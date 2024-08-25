// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "Modules/ModuleManager.h"
//#include "AI/NavigationSystemBase.h"
#if WITH_EDITOR
#include "AssetTypeCategories.h"
#endif // WITH_EDITOR

/**
 * The public interface to this module
 */
class INavSysModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline INavSysModule& Get()
	{
		return FModuleManager::LoadModuleChecked<INavSysModule>("NavigationSystem");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NavigationSystem");
	}
};

