// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVehicleManagerAsyncCallback.h"
#include "ChaosVehicleMovementComponent.h"
#include "PBDRigidsSolver.h"
#include "TransmissionSystem.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

extern FVehicleDebugParams GVehicleDebugParams;

DECLARE_CYCLE_STAT(TEXT("AsyncCallback:OnPreSimulate_Internal"), STAT_AsyncCallback_OnPreSimulate, STATGROUP_ChaosVehicleManager);

FName FChaosVehicleManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FChaosVehicleManagerAsyncCallback");
	return StaticName;
}

/**
 * Callback from Physics thread
 */

void FChaosVehicleManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep) 
{
	const FChaosVehicleManagerAsyncInput* AsyncInput = GetConsumerInput_Internal();
	if (AsyncInput == nullptr)
	{
		return;
	}

	for (const TUniquePtr<FChaosVehicleAsyncInput>& VehicleInput : AsyncInput->VehicleInputs)
	{
		UChaosVehicleSimulation* VehicleSim = VehicleInput->Vehicle->VehicleSimulationPT.Get();

		if (VehicleSim == nullptr || !VehicleInput->Vehicle->bUsingNetworkPhysicsPrediction)
		{
			continue;
		}
		bool bIsResimming = false;
		if (FPhysScene* PhysScene = VehicleInput->Vehicle->GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* LocalSolver = PhysScene->GetSolver())
			{
				bIsResimming = LocalSolver->GetEvolution()->IsResimming();
			}
		}

		APlayerController* PlayerController = VehicleInput->Vehicle->GetPlayerController();
		if(PlayerController && PlayerController->IsLocalController() && !bIsResimming)
		{ 
			VehicleSim->VehicleInputs = VehicleInput->PhysicsInputs.NetworkInputs.VehicleInputs;
		}
		else
		{
			VehicleInput->PhysicsInputs.NetworkInputs.VehicleInputs = VehicleSim->VehicleInputs;
		}
	}
}

void FChaosVehicleManagerAsyncCallback::OnPreSimulate_Internal()
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_AsyncCallback_OnPreSimulate);

	float DeltaTime = GetDeltaTime_Internal();
	float SimTime = GetSimTime_Internal();

	const FChaosVehicleManagerAsyncInput* Input = GetConsumerInput_Internal();
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

	FChaosVehicleManagerAsyncOutput& Output = GetProducerOutputData_Internal();
	Output.VehicleOutputs.AddDefaulted(NumVehicles);
	Output.Timestamp = Input->Timestamp;

	const TArray<TUniquePtr<FChaosVehicleAsyncInput>>& InputVehiclesBatch = Input->VehicleInputs;
	TArray<TUniquePtr<FChaosVehicleAsyncOutput>>& OutputVehiclesBatch = Output.VehicleOutputs;

	// beware running the vehicle simulation in parallel, code must remain threadsafe
	auto LambdaParallelUpdate = [World, DeltaTime, SimTime, &InputVehiclesBatch, &OutputVehiclesBatch](int32 Idx)
	{
		const FChaosVehicleAsyncInput& VehicleInput = *InputVehiclesBatch[Idx];

		if (VehicleInput.Proxy == nullptr || VehicleInput.Proxy->GetPhysicsThreadAPI() == nullptr)
		{
			return;
		}

		Chaos::FRigidBodyHandle_Internal* Handle = VehicleInput.Proxy->GetPhysicsThreadAPI();
		if (Handle->ObjectState() != Chaos::EObjectStateType::Dynamic)
		{
			return;
		}

		bool bWake = false;
		OutputVehiclesBatch[Idx] = VehicleInput.Simulate(World, DeltaTime, SimTime, bWake);
	};

	bool ForceSingleThread = !GVehicleDebugParams.EnableMultithreading;
	PhysicsParallelFor(OutputVehiclesBatch.Num(), LambdaParallelUpdate, ForceSingleThread);

	// Delayed application of forces - This is separate from Simulate because forces cannot be executed multi-threaded
	for (const TUniquePtr<FChaosVehicleAsyncInput>& VehicleInput : InputVehiclesBatch)
	{
		if (VehicleInput.IsValid() && VehicleInput->Proxy)
		{
			if (Chaos::FRigidBodyHandle_Internal* Handle = VehicleInput->Proxy->GetPhysicsThreadAPI())
			{
				VehicleInput->ApplyDeferredForces(Handle);
			}
		}
	}
}

TUniquePtr<FChaosVehicleAsyncOutput> FChaosVehicleAsyncInput::Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const
{
	TUniquePtr<FChaosVehicleAsyncOutput> Output = MakeUnique<FChaosVehicleAsyncOutput>();

	//UE_LOG(LogChaos, Warning, TEXT("Vehicle Physics Thread Tick %f"), DeltaSeconds);

	//support nullptr because it allows us to go wide on filling the async inputs
	if (Proxy == nullptr)
	{
		return Output;
	}

	// We now have access to the physics representation of the chassis on the physics thread async tick
	Chaos::FRigidBodyHandle_Internal* Handle = Proxy->GetPhysicsThreadAPI();

	// FILL OUTPUT DATA HERE THAT WILL GET PASSED BACK TO THE GAME THREAD
	Vehicle->VehicleSimulationPT->TickVehicle(World, DeltaSeconds, *this, *Output.Get(), Handle);

	Output->bValid = true;

	return MoveTemp(Output);
}

void FChaosVehicleAsyncInput::ApplyDeferredForces(Chaos::FRigidBodyHandle_Internal* RigidHandle) const
{
	check(Vehicle);
	check(Vehicle->VehicleSimulationPT);
	Vehicle->VehicleSimulationPT->ApplyDeferredForces(RigidHandle);
}

bool FNetworkVehicleInputs::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsDatas::SerializeFrames(Ar);

	Ar << VehicleInputs.SteeringInput;
	Ar << VehicleInputs.ThrottleInput;
	Ar << VehicleInputs.BrakeInput;
	Ar << VehicleInputs.PitchInput;
	Ar << VehicleInputs.RollInput;
	Ar << VehicleInputs.YawInput;
	Ar << VehicleInputs.HandbrakeInput;

	Ar << TransmissionChangeTime;
	Ar << TransmissionCurrentGear;
	Ar << TransmissionTargetGear;

	bOutSuccess = true;
	return bOutSuccess;
}

void FNetworkVehicleInputs::ApplyDatas(UActorComponent* NetworkComponent) const
{
	if (UChaosVehicleSimulation* VehicleSimulation = Cast<UChaosVehicleMovementComponent>(NetworkComponent)->VehicleSimulationPT.Get())
	{
		VehicleSimulation->VehicleInputs = VehicleInputs;

		if (TUniquePtr<Chaos::FSimpleWheeledVehicle>& Vehicle = VehicleSimulation->PVehicle)
		{
			if (Vehicle->HasTransmission())
			{
				Chaos::FSimpleTransmissionSim& Transmission = VehicleSimulation->PVehicle->GetTransmission();
				Transmission.SetCurrentGear(TransmissionCurrentGear);
				Transmission.SetTargetGear(TransmissionTargetGear);
				Transmission.SetCurrentGearChangeTime(TransmissionChangeTime);
			}
		}
#if DEBUG_NETWORK_PHYSICS
		int32 SolverFrame = INDEX_NONE;
		if (NetworkComponent->GetWorld() && NetworkComponent->GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* PhysicsSolver = NetworkComponent->GetWorld()->GetPhysicsScene()->GetSolver())
			{
				SolverFrame = PhysicsSolver->GetCurrentFrame();
			}
		}

		if (NetworkComponent->GetWorld()->IsNetMode(NM_ListenServer))
		{
			UE_LOG(LogTemp, Log, TEXT("SERVER | PT | ApplyDatas | Report replicated inputs at frame %d %d: Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f Gear = %d | VehicleInputs size = %d | ControlInputs size = %d | NetworkInputs = %d"),
				LocalFrame, SolverFrame, VehicleInputs.ThrottleInput, VehicleInputs.BrakeInput, VehicleInputs.RollInput, VehicleInputs.PitchInput,
				VehicleInputs.YawInput, VehicleInputs.SteeringInput, VehicleInputs.HandbrakeInput, TransmissionTargetGear, sizeof(FVehicleInputs) * 8, sizeof(FControlInputs) * 8, sizeof(FNetworkVehicleInputs) * 8);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | ApplyDatas | Report replicated inputs at frame %d %d: Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f Gear = %d"),
				LocalFrame, SolverFrame, VehicleInputs.ThrottleInput, VehicleInputs.BrakeInput, VehicleInputs.RollInput, VehicleInputs.PitchInput,
				VehicleInputs.YawInput, VehicleInputs.SteeringInput, VehicleInputs.HandbrakeInput, TransmissionTargetGear);
		}
#endif
	}
}

void FNetworkVehicleInputs::BuildDatas(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UChaosVehicleSimulation* VehicleSimulation = Cast<const UChaosVehicleMovementComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleInputs = VehicleSimulation->VehicleInputs;

			if (const TUniquePtr<Chaos::FSimpleWheeledVehicle>& Vehicle = VehicleSimulation->PVehicle)
			{
				if (Vehicle->HasTransmission())
				{
					Chaos::FSimpleTransmissionSim& Transmission = Vehicle->GetTransmission();
					TransmissionCurrentGear = Transmission.GetCurrentGear();
					TransmissionTargetGear = Transmission.GetTargetGear();
					TransmissionChangeTime = Transmission.GetCurrentGearChangeTime();
				}
			}
#if DEBUG_NETWORK_PHYSICS
			if(NetworkComponent->GetWorld()->IsNetMode(NM_ListenServer))
			{
				UE_LOG(LogTemp, Log, TEXT("SERVER | PT | BuildDatas | Extract local inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f Gear = %d"),
					LocalFrame, VehicleInputs.ThrottleInput, VehicleInputs.BrakeInput, VehicleInputs.RollInput, VehicleInputs.PitchInput,
					VehicleInputs.YawInput, VehicleInputs.SteeringInput, VehicleInputs.HandbrakeInput, TransmissionTargetGear);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | BuildDatas | Extract local inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f Gear = %d"),
					LocalFrame, VehicleInputs.ThrottleInput, VehicleInputs.BrakeInput, VehicleInputs.RollInput, VehicleInputs.PitchInput,
					VehicleInputs.YawInput, VehicleInputs.SteeringInput, VehicleInputs.HandbrakeInput, TransmissionTargetGear);
			}
#endif
		}
	}
}

void FNetworkVehicleInputs::InterpolateDatas(const FNetworkVehicleInputs& MinDatas, const FNetworkVehicleInputs& MaxDatas)
{
	const float LerpFactor = (LocalFrame - MinDatas.LocalFrame) / (MaxDatas.LocalFrame - MinDatas.LocalFrame);

	TransmissionChangeTime = FMath::Lerp(MinDatas.TransmissionChangeTime, MaxDatas.TransmissionChangeTime, LerpFactor);
	TransmissionCurrentGear = LerpFactor < 0.5 ? MinDatas.TransmissionCurrentGear : MaxDatas.TransmissionCurrentGear;
	TransmissionTargetGear = LerpFactor < 0.5 ? MinDatas.TransmissionTargetGear : MaxDatas.TransmissionTargetGear;

	VehicleInputs.BrakeInput = FMath::Lerp(MinDatas.VehicleInputs.BrakeInput, MaxDatas.VehicleInputs.BrakeInput, LerpFactor);
	VehicleInputs.HandbrakeInput = FMath::Lerp(MinDatas.VehicleInputs.HandbrakeInput, MaxDatas.VehicleInputs.HandbrakeInput, LerpFactor);
	VehicleInputs.PitchInput = FMath::Lerp(MinDatas.VehicleInputs.PitchInput, MaxDatas.VehicleInputs.PitchInput, LerpFactor);
	VehicleInputs.RollInput = FMath::Lerp(MinDatas.VehicleInputs.RollInput, MaxDatas.VehicleInputs.RollInput, LerpFactor);
	VehicleInputs.ThrottleInput = FMath::Lerp(MinDatas.VehicleInputs.ThrottleInput, MaxDatas.VehicleInputs.ThrottleInput, LerpFactor);
	VehicleInputs.SteeringInput = FMath::Lerp(MinDatas.VehicleInputs.SteeringInput, MaxDatas.VehicleInputs.SteeringInput, LerpFactor);
	VehicleInputs.YawInput = FMath::Lerp(MinDatas.VehicleInputs.YawInput, MaxDatas.VehicleInputs.YawInput, LerpFactor);

	VehicleInputs.ParkingEnabled = LerpFactor < 0.5 ? MinDatas.VehicleInputs.ParkingEnabled : MaxDatas.VehicleInputs.ParkingEnabled;
	VehicleInputs.GearDownInput = LerpFactor < 0.5 ? MinDatas.VehicleInputs.GearDownInput : MaxDatas.VehicleInputs.GearDownInput;
	VehicleInputs.GearUpInput = LerpFactor < 0.5 ? MinDatas.VehicleInputs.GearUpInput : MaxDatas.VehicleInputs.GearUpInput;
	VehicleInputs.TransmissionType = LerpFactor < 0.5 ? MinDatas.VehicleInputs.TransmissionType : MaxDatas.VehicleInputs.TransmissionType;
}

bool FNetworkVehicleStates::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsDatas::SerializeFrames(Ar);

	Ar << StateLastVelocity;
	Ar << EngineOmega;

	int32 NumWheels = WheelsOmega.Num();
	Ar << NumWheels;

	int32 NumLength = SuspensionAveragedLength.Num();
	Ar << NumLength;

	if (Ar.IsLoading())
	{
		WheelsOmega.SetNum(NumWheels);
		WheelsAngularPosition.SetNum(NumWheels);
		SuspensionLastDisplacement.SetNum(NumWheels);
		SuspensionLastSpringLength.SetNum(NumWheels);
		SuspensionAveragedCount.SetNum(NumWheels);
		SuspensionAveragedNum.SetNum(NumWheels);
		SuspensionAveragedLength.SetNum(NumLength);
	}

	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		Ar << WheelsOmega[WheelIdx];
		Ar << WheelsAngularPosition[WheelIdx];
		Ar << SuspensionLastDisplacement[WheelIdx];
		Ar << SuspensionLastSpringLength[WheelIdx];
		Ar << SuspensionAveragedCount[WheelIdx];
		Ar << SuspensionAveragedNum[WheelIdx];
	}

	for (int32 LengthIdx = 0; LengthIdx < NumLength; ++LengthIdx)
	{
		Ar << SuspensionAveragedLength[LengthIdx];
	}
	return true;
}

void FNetworkVehicleStates::ApplyDatas(UActorComponent* NetworkComponent) const
{
	if (UChaosVehicleSimulation* VehicleSimulation = Cast<UChaosVehicleMovementComponent>(NetworkComponent)->VehicleSimulationPT.Get())
	{
		VehicleSimulation->VehicleState.LastFrameVehicleLocalVelocity = StateLastVelocity;
		
		if (TUniquePtr<Chaos::FSimpleWheeledVehicle>& Vehicle = VehicleSimulation->PVehicle)
		{
			Vehicle->GetEngine().SetEngineOmega(EngineOmega);

#if DEBUG_NETWORK_PHYSICS
			if (NetworkComponent->GetWorld()->IsNetMode(NM_ListenServer))
			{
				UE_LOG(LogTemp, Log, TEXT("SERVER | PT | ApplyDatas | Report replicated states at frame %d %d : Omega = %f"),
					LocalFrame, ServerFrame, EngineOmega);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | ApplyDatas| Report replicated states at frame %d %d : Omega = %f"),
					LocalFrame, ServerFrame, EngineOmega);
			}
#endif
		
			int32 LengthCount = 0;
			for (int32 WheelIdx = 0, NumWheels = Vehicle->Wheels.Num(); WheelIdx < NumWheels; ++WheelIdx)
			{
				Chaos::FSimpleSuspensionSim& Suspension = Vehicle->GetSuspension(WheelIdx);
				Suspension.SetLastSpringLength(SuspensionLastSpringLength[WheelIdx]);
				Suspension.SetLastDisplacement(SuspensionLastDisplacement[WheelIdx]);
				Suspension.SetAveragingCount(SuspensionAveragedCount[WheelIdx]);
				Suspension.SetAveragingNum(SuspensionAveragedNum[WheelIdx]);
		
				for (int32 LengthIdx = 0; LengthIdx < Suspension.GetAveragingNum(); ++LengthIdx)
				{
					Suspension.SetAveragingLength(LengthIdx, SuspensionAveragedLength[LengthCount++]);
				}
		
				Chaos::FSimpleWheelSim& Wheel = Vehicle->GetWheel(WheelIdx);
				Wheel.Omega = WheelsOmega[WheelIdx];
				Wheel.AngularPosition = WheelsAngularPosition[WheelIdx];
			}
		}
	}
}

void FNetworkVehicleStates::BuildDatas(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UChaosVehicleSimulation* VehicleSimulation = Cast<const UChaosVehicleMovementComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			StateLastVelocity = VehicleSimulation->VehicleState.LastFrameVehicleLocalVelocity;
			if (const TUniquePtr<Chaos::FSimpleWheeledVehicle>& Vehicle = VehicleSimulation->PVehicle)
			{
				EngineOmega = Vehicle->GetEngine().GetEngineOmega();
#if DEBUG_NETWORK_PHYSICS
				if (NetworkComponent->GetWorld()->IsNetMode(NM_ListenServer))
				{
					UE_LOG(LogTemp, Log, TEXT("SERVER | PT | BuildDatas | Extract local states at frame %d %d : Omega = %f"),
						LocalFrame, ServerFrame, EngineOmega);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | BuildDatas| Extract local states at frame %d %d : Omega = %f"),
						LocalFrame, ServerFrame, EngineOmega);
				}
#endif
				
				const int32 NumWheels = Vehicle->Wheels.Num();
				
				int32 NumLength = 0;
				for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
				{
					NumLength += Vehicle->GetSuspension(WheelIdx).GetAveragingNum();
				}
				
				SuspensionLastSpringLength.SetNum(NumWheels);
				SuspensionLastDisplacement.SetNum(NumWheels);
				SuspensionAveragedCount.SetNum(NumWheels);
				SuspensionAveragedNum.SetNum(NumWheels);
				SuspensionAveragedLength.SetNum(NumLength);
				WheelsAngularPosition.SetNum(NumWheels);
				WheelsOmega.SetNum(NumWheels);
				
				int32 LengthCount = 0;
				for (int32 WheelIdx = 0; WheelIdx < Vehicle->Wheels.Num(); ++WheelIdx)
				{
					Chaos::FSimpleSuspensionSim& Suspension = Vehicle->GetSuspension(WheelIdx);
					SuspensionLastSpringLength[WheelIdx] = Suspension.GetLastSpringLength();
					SuspensionLastDisplacement[WheelIdx] = Suspension.GetLastDisplacement();
					SuspensionAveragedCount[WheelIdx] = Suspension.GetAveragingCount();
					SuspensionAveragedNum[WheelIdx] = Suspension.GetAveragingNum();
				
					for (int32 LengthIdx = 0; LengthIdx < Suspension.GetAveragingNum(); ++LengthIdx)
					{
						SuspensionAveragedLength[LengthCount++] = Suspension.GetAveragingLength(LengthIdx);
					}
				
					Chaos::FSimpleWheelSim& Wheel = Vehicle->GetWheel(WheelIdx);
					WheelsOmega[WheelIdx] = Wheel.Omega;
					WheelsAngularPosition[WheelIdx] = Wheel.AngularPosition;
				}
			}
		}
	}
}
void FNetworkVehicleStates::InterpolateDatas(const FNetworkVehicleStates& MinDatas, const FNetworkVehicleStates& MaxDatas)
{
	const float LerpFactor = (LocalFrame - MinDatas.LocalFrame) / (MaxDatas.LocalFrame - MinDatas.LocalFrame);

	StateLastVelocity = FMath::Lerp(MinDatas.StateLastVelocity, MaxDatas.StateLastVelocity, LerpFactor);
	EngineOmega = FMath::Lerp(MinDatas.EngineOmega, MaxDatas.EngineOmega, LerpFactor);

	int32 NumWheels = FMath::Min(MinDatas.WheelsOmega.Num(), MaxDatas.WheelsOmega.Num());

	WheelsOmega.SetNum(NumWheels, false);
	WheelsAngularPosition.SetNum(NumWheels, false);

	SuspensionLastDisplacement.SetNum(NumWheels, false);
	SuspensionLastSpringLength.SetNum(NumWheels, false);
	SuspensionAveragedCount.SetNum(NumWheels, false);
	SuspensionAveragedNum.SetNum(NumWheels, false);

	int32 NumLength = 0;
	for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		WheelsOmega[WheelIdx] = FMath::Lerp(MinDatas.WheelsOmega[WheelIdx], MaxDatas.WheelsOmega[WheelIdx], LerpFactor);
		WheelsAngularPosition[WheelIdx] = FMath::Lerp(MinDatas.WheelsAngularPosition[WheelIdx], MaxDatas.WheelsAngularPosition[WheelIdx], LerpFactor);

		SuspensionLastDisplacement[WheelIdx] = FMath::Lerp(MinDatas.SuspensionLastDisplacement[WheelIdx], MaxDatas.SuspensionLastDisplacement[WheelIdx], LerpFactor);
		SuspensionLastSpringLength[WheelIdx] = FMath::Lerp(MinDatas.SuspensionLastSpringLength[WheelIdx], MaxDatas.SuspensionLastSpringLength[WheelIdx], LerpFactor);
		SuspensionAveragedCount[WheelIdx] = FMath::Min( MinDatas.SuspensionAveragedCount[WheelIdx], MaxDatas.SuspensionAveragedCount[WheelIdx]);
		SuspensionAveragedNum[WheelIdx] = FMath::Min(MinDatas.SuspensionAveragedNum[WheelIdx], MaxDatas.SuspensionAveragedNum[WheelIdx]);

		NumLength += SuspensionAveragedNum[WheelIdx];
	}
	SuspensionAveragedLength.SetNum(NumLength, false);
	for (int32 LengthIdx = 0; LengthIdx < NumLength; ++LengthIdx)
	{
		SuspensionAveragedLength[LengthIdx] = FMath::Lerp(MinDatas.SuspensionAveragedLength[LengthIdx], MaxDatas.SuspensionAveragedLength[LengthIdx], LerpFactor);
	}
}


