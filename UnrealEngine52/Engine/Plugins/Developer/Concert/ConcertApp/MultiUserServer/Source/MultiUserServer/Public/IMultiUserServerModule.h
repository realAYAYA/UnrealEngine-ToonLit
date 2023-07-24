// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IConcertSyncServer;
struct FConcertSyncServerLoopInitArgs;

class IMultiUserServerModule : public IModuleInterface
{
public:
	
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMultiUserServerModule& Get()
	{
		static const FName ModuleName = "MultiUserServer";
		return FModuleManager::LoadModuleChecked<IMultiUserServerModule>(ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() during shutdown if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName ModuleName = "MultiUserServer";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	/** Set-up callbacks for creating Slate UI when the MU server is started. Called before the server loop is initialized. */
	virtual void InitSlateForServer(FConcertSyncServerLoopInitArgs& InitArgs) = 0;
};
