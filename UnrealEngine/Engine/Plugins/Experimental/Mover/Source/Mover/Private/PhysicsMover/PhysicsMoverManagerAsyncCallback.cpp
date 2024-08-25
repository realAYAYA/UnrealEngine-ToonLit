// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMoverManagerAsyncCallback.h"

#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Framework/Parallel.h"

//////////////////////////////////////////////////////////////////////////

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

//////////////////////////////////////////////////////////////////////////

void FPhysicsMoverManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{
	const FPhysicsMoverManagerAsyncInput* ManagerAsyncInput = GetConsumerInput_Internal();
	if (ManagerAsyncInput == nullptr)
	{
		return;
	}

	if (ManagerAsyncInput->AsyncInput.Num() == 0)
	{
		return;
	}

	const TArray<TUniquePtr<FPhysicsMoverAsyncInput>>& InputDataArray = ManagerAsyncInput->AsyncInput;
	const float DeltaTime = GetDeltaTime_Internal();

	auto LambdaParallelUpdate = [PhysicsStep, DeltaTime, &InputDataArray](int32 Idx)
	{
		const FPhysicsMoverAsyncInput& AsyncInput = *InputDataArray[Idx];
		if (AsyncInput.IsValid())
		{
			AsyncInput.MoverSimulation->ProcessInputs_Internal(PhysicsStep, DeltaTime, AsyncInput);
		}
	};

	bool ForceSingleThread = GPhysicsDrivenMotionDebugParams.EnableMultithreading;
	Chaos::PhysicsParallelFor(InputDataArray.Num(), LambdaParallelUpdate, ForceSingleThread);
}

void FPhysicsMoverManagerAsyncCallback::OnPreSimulate_Internal()
{
	const FPhysicsMoverManagerAsyncInput* ManagerAsyncInput = GetConsumerInput_Internal();

	if (!ManagerAsyncInput)
	{
		return;
	}

	if (ManagerAsyncInput->AsyncInput.Num() == 0)
	{
		return;
	}

	const TArray<TUniquePtr<FPhysicsMoverAsyncInput>>& InputDataArray = ManagerAsyncInput->AsyncInput;
	const FPhysicsMoverSimulationTickParams TickParams = { GetSimTime_Internal(), GetDeltaTime_Internal() };
	FPhysicsMoverManagerAsyncOutput& ManagerAsyncOutput = GetProducerOutputData_Internal();

	auto LambdaParallelUpdate = [TickParams, &InputDataArray, &ManagerAsyncOutput](int32 Idx)
	{
		const FPhysicsMoverAsyncInput& AsyncInput = *InputDataArray[Idx];
		if (AsyncInput.IsValid())
		{
			TUniquePtr<FPhysicsMoverAsyncOutput> AsyncOutput = MakeUnique<FPhysicsMoverAsyncOutput>();
			AsyncInput.MoverSimulation->OnPreSimulate_Internal(TickParams, AsyncInput, *AsyncOutput);
			ManagerAsyncOutput.PhysicsMoverToAsyncOutput.Add(AsyncInput.MoverIdx, MoveTemp(AsyncOutput));
		}
	};

	bool ForceSingleThread = GPhysicsDrivenMotionDebugParams.EnableMultithreading;
	Chaos::PhysicsParallelFor(InputDataArray.Num(), LambdaParallelUpdate, ForceSingleThread);
}

void FPhysicsMoverManagerAsyncCallback::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier)
{
	const FPhysicsMoverManagerAsyncInput* ManagerAsyncInput = GetConsumerInput_Internal();

	if (!ManagerAsyncInput)
	{
		return;
	}

	if (ManagerAsyncInput->AsyncInput.Num() == 0)
	{
		return;
	}

	const TArray<TUniquePtr<FPhysicsMoverAsyncInput>>& InputDataArray = ManagerAsyncInput->AsyncInput;

	auto LambdaParallelUpdate = [&InputDataArray, &Modifier](int32 Idx)
	{
		const FPhysicsMoverAsyncInput& AsyncInput = *InputDataArray[Idx];
		if (AsyncInput.IsValid())
		{
			AsyncInput.MoverSimulation->OnContactModification_Internal(AsyncInput, Modifier);
		}
	};

	bool ForceSingleThread = GPhysicsDrivenMotionDebugParams.EnableMultithreading;
	Chaos::PhysicsParallelFor(InputDataArray.Num(), LambdaParallelUpdate, ForceSingleThread);
}