// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMoverManager.h"

#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Chaos/Character/CharacterGroundConstraint.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsMover/PhysicsMoverManagerAsyncCallback.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsMoverManager)

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (FPhysScene* PhysScene = InWorld.GetPhysicsScene())
	{
		PhysScenePreTickCallbackHandle = PhysScene->OnPhysScenePreTick.AddUObject(this, &ThisClass::PrePhysicsUpdate);
		AsyncCallback = PhysScene->GetSolver()->CreateAndRegisterSimCallbackObject_External<FPhysicsMoverManagerAsyncCallback>();
	}
}

void UPhysicsMoverManager::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			PhysScene->OnPhysScenePreTick.Remove(PhysScenePreTickCallbackHandle);
			if (AsyncCallback)
			{
				PhysScene->GetSolver()->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
			}

			PhysicsMoverComponents.Empty();
		}
	}

	Super::Deinitialize();
}

bool UPhysicsMoverManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::RegisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> InPhysicsMoverComp)
{
	PhysicsMoverComponents.AddUnique(InPhysicsMoverComp);
}

void UPhysicsMoverManager::UnregisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> InPhysicsMoverComp)
{
	PhysicsMoverComponents.Remove(InPhysicsMoverComp);
}

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::PrePhysicsUpdate(FPhysScene* PhysScene, float DeltaTime)
{
	// Clear invalid data before we attempt to use it.
	PhysicsMoverComponents.RemoveAllSwap([](const TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> MoverComp) {
		return (MoverComp.IsValid() == false) || (MoverComp->GetUniqueIdx().IsValid() == false);
		});

	while (Chaos::TSimCallbackOutputHandle<FPhysicsMoverManagerAsyncOutput> ManagerAsyncOutput = AsyncCallback->PopFutureOutputData_External())
	{
		const double OutputTime = ManagerAsyncOutput->InternalTime;
		for (TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> PhysicsMoverComp : PhysicsMoverComponents)
		{
			Chaos::FUniqueIdx Idx = PhysicsMoverComp->GetUniqueIdx();
			if (Idx.IsValid())
			{
				if (TUniquePtr<FPhysicsMoverAsyncOutput>* OutputData = ManagerAsyncOutput->PhysicsMoverToAsyncOutput.Find(Idx))
				{
					PhysicsMoverComp->ConsumeOutput_External(**OutputData, OutputTime);
				}
			}
		}
	}

	FPhysicsMoverManagerAsyncInput* ManagerAsyncInput = AsyncCallback->GetProducerInputData_External();
	ManagerAsyncInput->Reset();
	ManagerAsyncInput->AsyncInput.Reserve(PhysicsMoverComponents.Num());

	if (PhysScene && PhysScene->GetOwningWorld())
	{
		for (TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> PhysicsMoverComp : PhysicsMoverComponents)
		{
			TUniquePtr<FPhysicsMoverAsyncInput> InputData = MakeUnique<FPhysicsMoverAsyncInput>();
			PhysicsMoverComp->ProduceInput_External(DeltaTime, *InputData);
			ManagerAsyncInput->AsyncInput.Add(MoveTemp(InputData));
		}
	}
}