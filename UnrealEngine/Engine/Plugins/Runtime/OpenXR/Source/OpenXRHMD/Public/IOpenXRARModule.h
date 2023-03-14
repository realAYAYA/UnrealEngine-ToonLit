// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeature.h"

class IOpenXRARModule : public IModuleInterface, public IModularFeature
{
public:
	/** Used to init our AR system */
	virtual class IARSystemSupport* CreateARSystem() = 0;
	virtual void SetTrackingSystem(TSharedPtr<class FXRTrackingSystemBase, ESPMode::ThreadSafe> InTrackingSystem) = 0;
	virtual bool GetExtensions(TArray<const ANSICHAR*>& OutExtensions) = 0;

	virtual class IOpenXRARTrackedMeshHolder * GetTrackedMeshHolder() = 0;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOpenXRARModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IOpenXRARModule >("OpenXRAR");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenXRAR");
	}

};
