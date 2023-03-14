// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AI/AISystemBase.h"
#if WITH_EDITOR
#include "AssetTypeCategories.h"
#endif // WITH_EDITOR

/**
 * The public interface to this module
 */
class IAIModule : public IAISystemModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IAIModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IAIModule >( "AIModule" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "AIModule" );
	}

#if WITH_EDITOR
	virtual EAssetTypeCategories::Type GetAIAssetCategoryBit() const = 0;
#endif // WITH_EDITOR
};

