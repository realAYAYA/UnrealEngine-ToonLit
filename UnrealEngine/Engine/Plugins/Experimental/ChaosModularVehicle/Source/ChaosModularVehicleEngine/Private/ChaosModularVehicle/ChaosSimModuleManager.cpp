//// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "ChaosModularVehicle/ChaosSimModuleManager.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "PBDRigidsSolver.h"
#include "GameFramework/HUD.h" // for ShowDebugInfo
#include "Physics/Experimental/PhysScene_Chaos.h"


TMap<FPhysScene*, FChaosSimModuleManager*> FChaosSimModuleManager::SceneToModuleManagerMap;

FDelegateHandle FChaosSimModuleManager::OnPostWorldInitializationHandle;
FDelegateHandle FChaosSimModuleManager::OnWorldCleanupHandle;

extern FSimModuleDebugParams GSimModuleDebugParams;

bool FChaosSimModuleManager::GInitialized = false;


FChaosSimModuleManager::FChaosSimModuleManager(FPhysScene* PhysScene)
	: Scene(*PhysScene)
	, AsyncCallback(nullptr)
	, Timestamp(0)

{
	check(PhysScene);
	
	if (!GInitialized)
	{
		GInitialized = true;
		// PhysScene->GetOwningWorld() is always null here, the world is being setup too late to be of use
		// therefore setup these global world delegates that will callback when everything is setup so registering
		// the physics solver Async Callback will succeed
		OnPostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddStatic(&FChaosSimModuleManager::OnPostWorldInitialization);
		OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&FChaosSimModuleManager::OnWorldCleanup);

		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddStatic(&FChaosSimModuleManager::OnShowDebugInfo);
		}
	}

	ensure(FChaosSimModuleManager::SceneToModuleManagerMap.Find(PhysScene) == nullptr);	// double registration with same scene, will cause a leak

	// Add to Scene-To-Manager map
	FChaosSimModuleManager::SceneToModuleManagerMap.Add(PhysScene, this);
}

FChaosSimModuleManager::~FChaosSimModuleManager()
{
	while (CUVehicles.Num() > 0)
	{
		RemoveVehicle(CUVehicles.Last());
	}
}

void FChaosSimModuleManager::OnPostWorldInitialization(UWorld* InWorld, const UWorld::InitializationValues)
{
	FChaosSimModuleManager* Manager = FChaosSimModuleManager::GetManagerFromScene(InWorld->GetPhysicsScene());
	if (Manager)
	{
		Manager->RegisterCallbacks(InWorld);
	}
}

void FChaosSimModuleManager::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	FChaosSimModuleManager* Manager = FChaosSimModuleManager::GetManagerFromScene(InWorld->GetPhysicsScene());
	if (Manager)
	{
		Manager->UnregisterCallbacks();
	}
}

void FChaosSimModuleManager::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_ModularVehicle("ModularVehicle");
	if (Canvas && HUD->ShouldDisplayDebug(NAME_ModularVehicle))
	{
		FChaosSimModuleManager* Manager = FChaosSimModuleManager::GetManagerFromScene(HUD->GetWorld()->GetPhysicsScene());

		int ShowVehicleIndex = 0;
		if (!Manager->CUVehicles.IsEmpty())
		{
			if (Manager->CUVehicles[ShowVehicleIndex].IsValid())
			{
				Manager->CUVehicles[ShowVehicleIndex]->ShowDebugInfo(HUD, Canvas, DisplayInfo, YL, YPos);
			}
		}
	}
}

void FChaosSimModuleManager::DetachFromPhysScene(FPhysScene* PhysScene)
{
	if (AsyncCallback)
	{
		UnregisterCallbacks();
	}

	if (PhysScene->GetOwningWorld())
	{
		PhysScene->GetOwningWorld()->OnWorldBeginPlay.RemoveAll(this);
	}

	FChaosSimModuleManager::SceneToModuleManagerMap.Remove(PhysScene);
}


void FChaosSimModuleManager::RegisterCallbacks(UWorld* InWorld)
{
	OnPhysScenePreTickHandle = Scene.OnPhysScenePreTick.AddRaw(this, &FChaosSimModuleManager::Update);
	OnPhysScenePostTickHandle = Scene.OnPhysScenePostTick.AddRaw(this, &FChaosSimModuleManager::PostUpdate);

	// Set up our async object manager to handle async ticking and marshaling
	check(AsyncCallback == nullptr);
	AsyncCallback = Scene.GetSolver()->CreateAndRegisterSimCallbackObject_External<FChaosSimModuleManagerAsyncCallback>();
}

void FChaosSimModuleManager::UnregisterCallbacks()
{
	Scene.OnPhysScenePreTick.Remove(OnPhysScenePreTickHandle);
	Scene.OnPhysScenePostTick.Remove(OnPhysScenePostTickHandle);
	
	if (AsyncCallback)
	{
		Scene.GetSolver()->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
		AsyncCallback = nullptr;
	}

}

FChaosSimModuleManager* FChaosSimModuleManager::GetManagerFromScene(FPhysScene* PhysScene)
{
	FChaosSimModuleManager* Manager = nullptr;
	FChaosSimModuleManager** ManagerPtr = SceneToModuleManagerMap.Find(PhysScene);
	if (ManagerPtr != nullptr)
	{
		Manager = *ManagerPtr;
	}
	return Manager;
}

void FChaosSimModuleManager::AddVehicle(TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle)
{
	check(Vehicle != NULL);
	check(Vehicle->PhysicsVehicleOutput());
	check(AsyncCallback);
	CUVehicles.Add(Vehicle);
}

void FChaosSimModuleManager::RemoveVehicle(TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle)
{
	check(Vehicle != NULL);

	CUVehicles.Remove(Vehicle);
}

void FChaosSimModuleManager::ScenePreTick(FPhysScene* PhysScene, float DeltaTime)
{
	for (int32 i = 0; i < CUVehicles.Num(); ++i)
	{
		CUVehicles[i]->PreTickGT(DeltaTime);
	}
}

void FChaosSimModuleManager::Update(FPhysScene* PhysScene, float DeltaTime)
{
	UWorld* World = Scene.GetOwningWorld();

	SubStepCount = 0;

	ScenePreTick(PhysScene, DeltaTime);

	ParallelUpdateVehicles(DeltaTime);

	if (World)
	{
		FChaosSimModuleManagerAsyncInput* AsyncInput = AsyncCallback->GetProducerInputData_External();

		for (TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle : CUVehicles)
		{
			Vehicle->Update(DeltaTime);
			Vehicle->FinalizeSimCallbackData(*AsyncInput);
		}
	}
}


void FChaosSimModuleManager::PostUpdate(FChaosScene* PhysScene)
{
}


void FChaosSimModuleManager::ParallelUpdateVehicles(float DeltaSeconds)
{

	FChaosSimModuleManagerAsyncInput* AsyncInput = AsyncCallback->GetProducerInputData_External();

	AsyncInput->Reset();	// only want latest frame's data

	{
		// We pass pointers from TArray so this reserve is critical. Otherwise realloc happens
		AsyncInput->VehicleInputs.Reserve(CUVehicles.Num() + GCVehicles.Num());
		AsyncInput->Timestamp = Timestamp;
		AsyncInput->World = Scene.GetOwningWorld();
	}

	// Grab all outputs for processing, even future ones for interpolation.
	{
		Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput> AsyncOutputLatest;
		while ((AsyncOutputLatest = AsyncCallback->PopFutureOutputData_External()))
		{
			PendingOutputs.Emplace(MoveTemp(AsyncOutputLatest));
		}
	}

	// Since we are in pre-physics, delta seconds is not accounted for in external time yet
	const float ResultsTime = AsyncCallback->GetSolver()->GetPhysicsResultsTime_External() + DeltaSeconds;

	// Find index of first non-consumable output (first one after current time)
	int32 LastOutputIdx = 0;
	for (; LastOutputIdx < PendingOutputs.Num(); ++LastOutputIdx)
	{
		if (PendingOutputs[LastOutputIdx]->InternalTime > ResultsTime)
		{
			break;
		}
	}

	// Cache the last consumed output for interpolation
	if (LastOutputIdx > 0)
	{
		LatestOutput = MoveTemp(PendingOutputs[LastOutputIdx - 1]);
	}

	// Remove all consumed outputs
	{
		TArray<Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput>> NewPendingOutputs;
		for (int32 OutputIdx = LastOutputIdx; OutputIdx < PendingOutputs.Num(); ++OutputIdx)
		{
			NewPendingOutputs.Emplace(MoveTemp(PendingOutputs[OutputIdx]));
		}
		PendingOutputs = MoveTemp(NewPendingOutputs);
	}

	// It's possible we will end up multiple frames ahead of output, take the latest ready output.
	Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput> AsyncOutput;
	Chaos::TSimCallbackOutputHandle<FChaosSimModuleManagerAsyncOutput> AsyncOutputLatest;
	while ((AsyncOutputLatest = AsyncCallback->PopOutputData_External()))
	{
		AsyncOutput = MoveTemp(AsyncOutputLatest);
	}

	if (UWorld* World = Scene.GetOwningWorld())
	{
		int32 NumVehiclesInActiveBatch = 0;

		for (TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle : CUVehicles)
		{
			auto NextOutput = PendingOutputs.Num() > 0 ? PendingOutputs[0].Get() : nullptr;
			float Alpha = 0.f;
			if (NextOutput && LatestOutput)
			{
				const float Denom = NextOutput->InternalTime - LatestOutput->InternalTime;
				if (Denom > SMALL_NUMBER)
				{
					Alpha = (ResultsTime - LatestOutput->InternalTime) / Denom;
				}
			}

			AsyncInput->VehicleInputs.Add(Vehicle->SetCurrentAsyncData(AsyncInput->VehicleInputs.Num(), LatestOutput.Get(), NextOutput, Alpha, Timestamp));
		}

	}

	++Timestamp;

	bool ForceSingleThread = !GSimModuleDebugParams.EnableMultithreading;

	{
		const auto& AwakeVehiclesBatch = CUVehicles;
		auto LambdaParallelUpdate2 = [DeltaSeconds, &AwakeVehiclesBatch](int32 Idx)
		{
			TWeakObjectPtr<UModularVehicleBaseComponent> Vehicle = AwakeVehiclesBatch[Idx];
			Vehicle->ParallelUpdate(DeltaSeconds); // gets output state from PT
		};

		ParallelFor(AwakeVehiclesBatch.Num(), LambdaParallelUpdate2, ForceSingleThread);
	}

}

