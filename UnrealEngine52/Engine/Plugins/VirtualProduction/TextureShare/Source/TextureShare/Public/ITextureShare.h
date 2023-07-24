// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ITextureShareObject;
class ITextureShareObjectProxy;

/**
 * TextureShare module public API
 */
class ITextureShare
	: public IModuleInterface
{
public:
	static constexpr auto ModuleName = TEXT("TextureShare");
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */

	static inline ITextureShare& Get()
	{
		return FModuleManager::LoadModuleChecked<ITextureShare>(ITextureShare::ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ITextureShare::ModuleName);
	}

public:
	/**
	 * Returns API Interface
	 * Put headers in an Internal folder.
	 * This limits visibility to other engine modules, and allows implementations to be changed in hotfixes.
	 */
	virtual class ITextureShareAPI& GetTextureShareAPI() = 0;
};
