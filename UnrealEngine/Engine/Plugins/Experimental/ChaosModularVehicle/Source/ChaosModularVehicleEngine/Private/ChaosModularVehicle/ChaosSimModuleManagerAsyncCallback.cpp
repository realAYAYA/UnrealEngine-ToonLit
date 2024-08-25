// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"

#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "PBDRigidsSolver.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"

FSimModuleDebugParams GSimModuleDebugParams;

DECLARE_CYCLE_STAT(TEXT("AsyncCallback:OnPreSimulate_Internal"), STAT_AsyncCallback_OnPreSimulate, STATGROUP_ChaosSimModuleManager);

FName FChaosSimModuleManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FChaosSimModuleManagerAsyncCallback");
	return StaticName;
}

/**
 * Callback from Physics thread
 */

void FChaosSimModuleManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{
	const FChaosSimModuleManagerAsyncInput* AsyncInput = GetConsumerInput_Internal();
	if (AsyncInput == nullptr)
	{
		return;
	}

	for (const TUniquePtr<FModularVehicleAsyncInput>& VehicleInput : AsyncInput->VehicleInputs)
	{
		VehicleInput->ProcessInputs();
	}
}

/**
 * Callback from Physics thread
 */
void FChaosSimModuleManagerAsyncCallback::OnPreSimulate_Internal()
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_AsyncCallback_OnPreSimulate);

	float DeltaTime = GetDeltaTime_Internal();
	float SimTime = GetSimTime_Internal();

	const FChaosSimModuleManagerAsyncInput* Input = GetConsumerInput_Internal();
	if (Input == nullptr)
	{
		return;
	}

	const int32 NumVehicles = Input->VehicleInputs.Num();

	UWorld* World = Input->World.Get();	//only safe to access for scene queries
	if (World == nullptr || NumVehicles == 0)
	{
		//world is gone so don't bother, or nothing to simulate.
		return;
	}

	Chaos::FPhysicsSolver* PhysicsSolver = static_cast<Chaos::FPhysicsSolver*>(GetSolver());
	if (PhysicsSolver == nullptr)
	{
		return;
	}

	FChaosSimModuleManagerAsyncOutput& Output = GetProducerOutputData_Internal();
	Output.VehicleOutputs.AddDefaulted(NumVehicles);
	Output.Timestamp = Input->Timestamp;

	const TArray<TUniquePtr<FModularVehicleAsyncInput>>& InputVehiclesBatch = Input->VehicleInputs;
	TArray<TUniquePtr<FModularVehicleAsyncOutput>>& OutputVehiclesBatch = Output.VehicleOutputs;

	// beware running the vehicle simulation in parallel, code must remain threadsafe
	auto LambdaParallelUpdate = [World, DeltaTime, SimTime, &InputVehiclesBatch, &OutputVehiclesBatch](int32 Idx)
	{
		const FModularVehicleAsyncInput& VehicleInput = *InputVehiclesBatch[Idx];

		if (VehicleInput.Proxy == nullptr)
		{
			return;
		}

		bool bWake = false;
		OutputVehiclesBatch[Idx] = VehicleInput.Simulate(World, DeltaTime, SimTime, bWake);

	};

	bool ForceSingleThread = !GSimModuleDebugParams.EnableMultithreading;
	PhysicsParallelFor(OutputVehiclesBatch.Num(), LambdaParallelUpdate, ForceSingleThread);

	// Delayed application of forces - This is separate from Simulate because forces cannot be executed multi-threaded
	for (const TUniquePtr<FModularVehicleAsyncInput>& VehicleInput : InputVehiclesBatch)
	{
		if (VehicleInput.IsValid())
		{
			VehicleInput->ApplyDeferredForces();
		}
	}
}

/**
 * Contact modification currently unused
 */
void FChaosSimModuleManagerAsyncCallback::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifications)
{

}


TUniquePtr<FModularVehicleAsyncOutput> FModularVehicleAsyncInput::Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const
{
	TUniquePtr<FModularVehicleAsyncOutput> Output = MakeUnique<FModularVehicleAsyncOutput>();

	//support nullptr because it allows us to go wide on filling the async inputs
	if (Proxy == nullptr)
	{
		return Output;
	}

	if (Vehicle && Vehicle->VehicleSimulationPT)
	{
		// FILL OUTPUT DATA HERE THAT WILL GET PASSED BACK TO THE GAME THREAD
		Vehicle->VehicleSimulationPT->Simulate(World, DeltaSeconds, *this, *Output.Get(), Proxy);

		FModularVehicleAsyncOutput& OutputData = *Output.Get();
		Vehicle->VehicleSimulationPT->FillOutputState(OutputData);
	}


	Output->bValid = true;

	return MoveTemp(Output);
}

void FModularVehicleAsyncInput::ApplyDeferredForces() const
{
	if (Vehicle && Proxy)
	{
		if (Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
		{
			Vehicle->VehicleSimulationPT->ApplyDeferredForces(static_cast<Chaos::FClusterUnionPhysicsProxy*>(Proxy));
		}
		else if (Proxy->GetType() == EPhysicsProxyType::GeometryCollectionType)
		{
			Vehicle->VehicleSimulationPT->ApplyDeferredForces(static_cast<FGeometryCollectionPhysicsProxy*>(Proxy));
		}

	}

}

void FModularVehicleAsyncInput::ProcessInputs()
{
	if (!GetVehicle())
	{
		return;
	}

	FModularVehicleSimulationCU* VehicleSim = GetVehicle()->VehicleSimulationPT.Get();

	if (VehicleSim == nullptr || !GetVehicle()->bUsingNetworkPhysicsPrediction || GetVehicle()->GetWorld() == nullptr)
	{
		return;
	}
	bool bIsResimming = false;
	if (FPhysScene* PhysScene = GetVehicle()->GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* LocalSolver = PhysScene->GetSolver())
		{
			bIsResimming = LocalSolver->GetEvolution()->IsResimming();
		}
	}

	if (GetVehicle()->IsLocallyControlled() && !bIsResimming)
	{
		VehicleSim->VehicleInputs = PhysicsInputs.NetworkInputs.VehicleInputs;
	}
	else
	{
		PhysicsInputs.NetworkInputs.VehicleInputs = VehicleSim->VehicleInputs;
	}

}

bool FNetworkModularVehicleInputs::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	Ar << VehicleInputs.Steering;
	Ar << VehicleInputs.Throttle;
	Ar << VehicleInputs.Brake;
	Ar << VehicleInputs.Handbrake;
	Ar << VehicleInputs.Pitch;
	Ar << VehicleInputs.Roll;
	Ar << VehicleInputs.Yaw;
	Ar << VehicleInputs.Boost;
	Ar << VehicleInputs.Drift;
	Ar << VehicleInputs.Reverse;
	Ar << VehicleInputs.KeepAwake;

	bOutSuccess = true;
	return bOutSuccess;
}

void FNetworkModularVehicleInputs::ApplyData(UActorComponent* NetworkComponent) const
{
	if (GSimModuleDebugParams.EnableNetworkStateData)
	{
		if (FModularVehicleSimulationCU* VehicleSimulation = Cast<UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleSimulation->VehicleInputs = VehicleInputs;
		}
	}
}

void FNetworkModularVehicleInputs::BuildData(const UActorComponent* NetworkComponent)
{
	if (GSimModuleDebugParams.EnableNetworkStateData && NetworkComponent)
	{
		if (const FModularVehicleSimulationCU* VehicleSimulation = Cast<const UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleInputs = VehicleSimulation->VehicleInputs;
		}
	}
}

void FNetworkModularVehicleInputs::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkModularVehicleInputs& MinInput = static_cast<const FNetworkModularVehicleInputs&>(MinData);
	const FNetworkModularVehicleInputs& MaxInput = static_cast<const FNetworkModularVehicleInputs&>(MaxData);

	const float LerpFactor = (LocalFrame - MinInput.LocalFrame) / (MaxInput.LocalFrame - MinInput.LocalFrame);

	VehicleInputs.Steering = FMath::Lerp(MinInput.VehicleInputs.Steering, MaxInput.VehicleInputs.Steering, LerpFactor);
	VehicleInputs.Throttle = FMath::Lerp(MinInput.VehicleInputs.Throttle, MaxInput.VehicleInputs.Throttle, LerpFactor);
	VehicleInputs.Brake = FMath::Lerp(MinInput.VehicleInputs.Brake, MaxInput.VehicleInputs.Brake, LerpFactor);
	VehicleInputs.Handbrake = FMath::Lerp(MinInput.VehicleInputs.Handbrake, MaxInput.VehicleInputs.Handbrake, LerpFactor);
	VehicleInputs.Pitch = FMath::Lerp(MinInput.VehicleInputs.Pitch, MaxInput.VehicleInputs.Pitch, LerpFactor);
	VehicleInputs.Roll = FMath::Lerp(MinInput.VehicleInputs.Roll, MaxInput.VehicleInputs.Roll, LerpFactor);
	VehicleInputs.Yaw = FMath::Lerp(MinInput.VehicleInputs.Yaw, MaxInput.VehicleInputs.Yaw, LerpFactor);
	VehicleInputs.Boost = FMath::Lerp(MinInput.VehicleInputs.Boost, MaxInput.VehicleInputs.Boost, LerpFactor);
	VehicleInputs.Drift = FMath::Lerp(MinInput.VehicleInputs.Drift, MaxInput.VehicleInputs.Drift, LerpFactor);
	VehicleInputs.Reverse = MinInput.VehicleInputs.Reverse;
	VehicleInputs.KeepAwake = MinInput.VehicleInputs.KeepAwake;
}

bool FNetworkModularVehicleStates::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	int32 NumNetModules = ModuleData.Num();
	Ar << NumNetModules;

	for (int I = 0; I < NumNetModules; I++)
	{
		if (Ar.IsLoading())
		{
			if (NumNetModules > 0)
			{
				int32 ModuleType = Chaos::eSimType::Undefined;
				int32 SimArrayIndex = 0;
				Ar << ModuleType;
				Ar << SimArrayIndex;

				if (!ModuleData.IsEmpty())
				{
					ensure(I <= ModuleData.Num());
				}

				if (ModuleData.Num() != NumNetModules)
				{
					ModuleData.Reserve(NumNetModules);
					switch (ModuleType)
					{
					case Chaos::eSimType::Suspension:
					{
						ModuleData.Emplace(MakeShared<Chaos::FSuspensionSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Transmission:
					{
						ModuleData.Emplace(MakeShared<Chaos::FTransmissionSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Engine:
					{
						ModuleData.Emplace(MakeShared<Chaos::FEngineSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Clutch:
					{
						ModuleData.Emplace(MakeShared<Chaos::FClutchSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Wheel:
					{
						ModuleData.Emplace(MakeShared<Chaos::FWheelSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					default:
					{
						checkf(false, TEXT("Unhandled NetModuleType case"));
					}
					}
				}
			}
		}
		else
		{
			int32 ModuleType = (int32)ModuleData[I]->GetType();
			Ar << ModuleType;
			Ar << ModuleData[I]->SimArrayIndex;
		}

		ModuleData[I]->Serialize(Ar);
	}

	return true;
}

void FNetworkModularVehicleStates::ApplyData(UActorComponent* NetworkComponent) const
{
	if (FModularVehicleSimulationCU* VehicleSimulation = Cast<UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
	{
		VehicleSimulation->AccessSimComponentTree()->SetSimState(ModuleData);
	}
}

void FNetworkModularVehicleStates::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const FModularVehicleSimulationCU* VehicleSimulation = Cast<const UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleSimulation->GetSimComponentTree()->SetNetState(ModuleData);
		}
	}
}

void FNetworkModularVehicleStates::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkModularVehicleStates& MinState = static_cast<const FNetworkModularVehicleStates&>(MinData);
	const FNetworkModularVehicleStates& MaxState = static_cast<const FNetworkModularVehicleStates&>(MaxData);

	const float LerpFactor = (LocalFrame - MinState.LocalFrame) / (MaxState.LocalFrame - MinState.LocalFrame);

	for (int I = 0; I < ModuleData.Num(); I++)
	{
		// if these don't match then something has gone terribly wrong
		check(ModuleData[I]->GetType() == MinState.ModuleData[I]->GetType());
		check(ModuleData[I]->GetType() == MaxState.ModuleData[I]->GetType());

		ModuleData[I]->Lerp(LerpFactor, *MinState.ModuleData[I].Get(), *MaxState.ModuleData[I].Get());
	}
}

