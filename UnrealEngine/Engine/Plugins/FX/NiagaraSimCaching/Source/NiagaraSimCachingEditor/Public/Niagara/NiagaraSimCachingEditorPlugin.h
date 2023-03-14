// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Delegates/IDelegateInstance.h"
#include "Sequencer/MovieSceneNiagaraTrackRecorder.h"

class FAssetTypeActions_NiagaraCacheCollection;

class UToolMenu;
struct FToolMenuSection;
class AActor;

/**
 * The public interface to this module
 */
class INiagaraSimCachingEditorPlugin : public IModuleInterface
{
	TArray<IConsoleObject*> EditorCommands;

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static INiagaraSimCachingEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<INiagaraSimCachingEditorPlugin>("NiagaraSimCachingEditorPlugin");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded("NiagaraSimCachingEditorPlugin"); }

private:

	FDelegateHandle TrackEditorBindingHandle;

	FMovieSceneNiagaraTrackRecorderFactory MovieSceneNiagaraCacheTrackRecorder;
};
