// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "Engine/World.h"

#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"

class UModularVehicleComponent;
class UModularVehicleBaseComponent;
class AHUD;
class FPhysScene_Chaos;

class CHAOSMODULARVEHICLEENGINE_API FChaosSimModuleManager
{
public:
	// Updated when vehicles need to recreate their physics state.
	// Used when values tweaked while the game is running.
	//static uint32 VehicleSetupTag;

	FChaosSimModuleManager(FPhysScene* PhysScene);
	~FChaosSimModuleManager();

	static void OnPostWorldInitialization(UWorld* InWorld, const UWorld::InitializationValues);
	static void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	/** Get Physics Scene */
	FPhysScene_Chaos& GetScene() const { return Scene; }

	/**
	 * Register a Physics vehicle for processing
	 */
	void AddVehicle(TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle);

	/**
	 * Unregister a Physics vehicle from processing
	 */
	void RemoveVehicle(TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle);

	/**
	 * Update vehicle tuning and other state such as input
	 */
	void ScenePreTick(FPhysScene* PhysScene, float DeltaTime);

	/** Detach this vehicle manager from a FPhysScene (remove delegates, remove from map etc) */
	void DetachFromPhysScene(FPhysScene* PhysScene);

	/** Update simulation of registered vehicles */
	void Update(FPhysScene* PhysScene, float DeltaTime);

	/** Post update step */
	void PostUpdate(FChaosScene* PhysScene);

	void ParallelUpdateVehicles(float DeltaSeconds);

	/** Find a vehicle manager from an FPhysScene */
	static FChaosSimModuleManager* GetManagerFromScene(FPhysScene* PhysScene);

protected:
	void RegisterCallbacks(UWorld* InWorld);
	void UnregisterCallbacks();

private:
	/** Map of physics scenes to corresponding vehicle manager */
	static TMap<FPhysScene*, FChaosSimModuleManager*> SceneToModuleManagerMap;
	
	// The physics scene we belong to
	FPhysScene_Chaos& Scene;

	static bool GInitialized;

	// All instanced vehicles
	TArray<TWeakObjectPtr<UModularVehicleComponent>> GCVehicles;

	// Test out new vehicle bass class
	TArray<TWeakObjectPtr<UModularVehicleBaseComponent>> CUVehicles;

	// callback delegates
	FDelegateHandle OnPhysScenePreTickHandle;
	FDelegateHandle OnPhysScenePostTickHandle;

	static FDelegateHandle OnPostWorldInitializationHandle;
	static FDelegateHandle OnWorldCleanupHandle;

	// Async callback from the physics engine - we can run our simulation here
	FChaosSimModuleManagerAsyncCallback* AsyncCallback;	
	int32 Timestamp;
	int32 SubStepCount;

	// async input/output state
	TArray<Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput>> PendingOutputs;
	Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput> LatestOutput;

};

