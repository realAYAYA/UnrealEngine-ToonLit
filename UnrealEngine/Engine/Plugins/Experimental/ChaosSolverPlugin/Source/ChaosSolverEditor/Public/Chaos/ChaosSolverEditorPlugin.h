// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * The public interface to this module
 */
class IChaosSolverEditorPlugin : public IModuleInterface
{
	TArray<IConsoleObject*> EditorCommands;

public:
	virtual void StartupModule();
	virtual void ShutdownModule();


	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IChaosSolverEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IChaosSolverEditorPlugin >( "ChaosSolverEditorPlugin" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "ChaosSolverEditorPlugin" );
	}

private:
	class FAssetTypeActions_ChaosSolver* AssetTypeActions_ChaosSolver;

};

