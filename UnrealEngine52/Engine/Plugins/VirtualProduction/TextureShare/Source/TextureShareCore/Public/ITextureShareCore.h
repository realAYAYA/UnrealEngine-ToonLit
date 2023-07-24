// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * Low level API for UE and SDK
 */
class ITextureShareCore
	: public IModuleInterface
{
	static constexpr auto ModuleName = TEXT("TextureShareCore");

public:
	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton object, loading the module on demand if needed
	*/
	static inline ITextureShareCore& Get()
	{
		return FModuleManager::LoadModuleChecked<ITextureShareCore>(ITextureShareCore::ModuleName);
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ITextureShareCore::ModuleName);
	}

public:
	/**
	 * Returns Core API Interface
	 * Put headers in an Internal folder.
	 * This limits visibility to other engine modules, and allows implementations to be changed in hotfixes.
	 */
	virtual class ITextureShareCoreAPI& GetTextureShareCoreAPI() = 0;
};
