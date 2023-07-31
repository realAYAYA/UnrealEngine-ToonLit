// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PhysicsInterfaceDeclaresCore.h"


/**
 * The public interface to this module
 */
class IChaosVehiclesEditorPlugin : public IModuleInterface
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
	static inline IChaosVehiclesEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IChaosVehiclesEditorPlugin >( "ChaosVehiclesEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "ChaosVehiclesEditor" );
	}

private:

	void PhysSceneInit(FPhysScene* PhysScene);
	void PhysSceneTerm(FPhysScene* PhysScene);

	class FAssetTypeActions_ChaosVehicles* AssetTypeActions_ChaosVehicles;
	//FDelegateHandle OnUpdatePhysXMaterialHandle;
//FDelegateHandle OnPhysicsAssetChangedHandle;
	FDelegateHandle OnPhysSceneInitHandle;
	FDelegateHandle OnPhysSceneTermHandle;


};

