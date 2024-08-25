// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "ChaosModularVehicle/ChaosSimModuleManager.h"
#include "ChaosModularVehicle/VehicleSimBaseComponent.h"
#include "ChaosModularVehicle/ModularVehicleDefaultAsyncInput.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/ClusterUnionComponent.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsReplication.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "SimModule/SimModuleTree.h"
#include "Chaos/PBDSuspensionConstraints.h"

#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"

DEFINE_LOG_CATEGORY(LogModularBase);

bool bModularVehicle_SuspensionConstraint_Enabled = true;
FAutoConsoleVariableRef CVarModularVehicleSuspensionConstraintEnabled(TEXT("p.ModularVehicle.SuspensionConstraint.Enabled"), bModularVehicle_SuspensionConstraint_Enabled, TEXT("Enable/Disable suspension constraint falling back to simple forces when constraint disabled (requires restart)."));

bool bModularVehicle_DumpModuleTreeStructure_Enabled = false;
FAutoConsoleVariableRef CVarModularVehicleDumpModuleTreeStructureEnabled(TEXT("p.ModularVehicle.DumpModuleTreeStructure.Enabled"), bModularVehicle_DumpModuleTreeStructure_Enabled, TEXT("Enable/Disable logging of module tree structure every time there is a change."));

UModularVehicleBaseComponent::UModularVehicleBaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = false;
	bRequiresControllerForInputs = true;

	LinearDamping = 0.01f;
	AngularDamping = 0.0f;

	bAutoAddComponentsFromWorld = false;
	AutoAddOverlappingBoxSize = FVector(500, 500, 500);
	ClusteringCount = 0;
	DelayClusteringCount = 0;

	SetIsReplicatedByDefault(true);
	bUsingNetworkPhysicsPrediction = Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled();

	SuspensionTraceCollisionResponses = FCollisionResponseContainer::GetDefaultResponseContainer();
	SuspensionTraceCollisionResponses.Vehicle = ECR_Ignore;
	bSuspensionTraceComplex = true;
	bKeepVehicleAwake = true;

	CurrentGear = 0;
	EngineRPM = 0.0f;
	EngineTorque = 0.0f;

	//// #TODO: currently ordering of this must match EModularVehicleInputType
	//InputInterpolationRates.Reset();
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Throttle")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Brake")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Clutch")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Steering")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Handbrake")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Pitch")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Roll")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Yaw")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("Gear")));
	//InputInterpolationRates.Add(FModularVehicleInputRate(FString("DebugIndex")));

	if (bUsingNetworkPhysicsPrediction)
	{
		static const FName NetworkPhysicsComponentName(TEXT("PC_NetworkPhysicsComponent"));

		NetworkPhysicsComponent = CreateDefaultSubobject<UNetworkPhysicsComponent, UNetworkPhysicsComponent>(NetworkPhysicsComponentName);
		NetworkPhysicsComponent->SetNetAddressable();
		NetworkPhysicsComponent->SetIsReplicated(true);
	}

	bIsLocallyControlled = false;
}

UModularVehicleBaseComponent::~UModularVehicleBaseComponent()
{
}

APlayerController* UModularVehicleBaseComponent::GetPlayerController() const
{
	if (AController* Controller = GetController())
	{
		return Cast<APlayerController>(Controller);
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AController* Controller = Pawn->GetController())
		{
			return Cast<APlayerController>(Controller);
		}

		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetOwner()))
		{
			return PC;
		}

	}
	return nullptr;
}


bool UModularVehicleBaseComponent::IsLocallyControlled() const
{
	if (bIsLocallyControlled && !GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		return true;
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		return PlayerController->IsLocalController();
	}
	return false;
}


void UModularVehicleBaseComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	if (ClusterUnionComponent)
	{
		// piggyback on the Add/Remove component events from the cluster union to add/remove simulation modules
		ClusterUnionComponent->OnComponentAddedNativeEvent.AddUObject(this, &UModularVehicleBaseComponent::AddComponentToSimulation);
		ClusterUnionComponent->OnComponentRemovedNativeEvent.AddUObject(this, &UModularVehicleBaseComponent::RemoveComponentFromSimulation);

		// #TODO: this doesn't appear to be working
		if (bKeepVehicleAwake && ClusterUnionComponent->GetPhysicsProxy())
		{
			if (auto* Particle = ClusterUnionComponent->GetPhysicsProxy()->GetParticle_External())
			{
				Particle->SetSleepType(Chaos::ESleepType::NeverSleep);
			}
		}
	}

	// convert the game thread side UVehicleSimBaseComponent(s) to physics thread simulation ISimulationModuleBase(s)
	CreateVehicleSim();

	if (bUsingNetworkPhysicsPrediction)
	{
		if (NetworkPhysicsComponent)
		{
			// register interface to handle network prediction callbacks
			// #Note: in our case we don't yet know what the replication data will be since the modules are built after this point at runtime
			NetworkPhysicsComponent->CreateDataHistory<FPhysicsModularVehicleTraits>(this);

			if (bIsLocallyControlled)
			{
				NetworkPhysicsComponent->SetIsRelayingLocalInputs(bIsLocallyControlled);
			}

		}
	}

	NextConstructionIndex = 0;

	ActorsToIgnore.Add(GetOwner()); // vehicle ignore self in scene query

}

void UModularVehicleBaseComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();

	if (ClusterUnionComponent)
	{
		ClusterUnionComponent->OnComponentAddedNativeEvent.RemoveAll(this);
		ClusterUnionComponent->OnComponentRemovedNativeEvent.RemoveAll(this);
	}

	DestroyVehicleSim();

	if (bUsingNetworkPhysicsPrediction && NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->RemoveDataHistory();
	}

}


int GenerateNewGuid()
{
	static int Val = 0;
	return Val++;
}

void UModularVehicleBaseComponent::CreateAssociatedSimComponents(const UPrimitiveComponent* AttachedComponent, int ParentIndex, int TransformIndex, Chaos::FSimTreeUpdates& TreeUpdatesOut)
{
	using namespace Chaos;
	if (AttachedComponent == nullptr || ClusterUnionComponent == nullptr)
	{
		return;
	}

	UE_LOG(LogModularBase, Log, TEXT("CreateAssociatedSimComponents Attaching %s, TransformIndex %d"), *AttachedComponent->GetName(), TransformIndex);

	TArray<Chaos::FClusterUnionChildData> ChildParticles = ClusterUnionComponent->GetPhysicsProxy()->GetSyncedData_External().ChildParticles;

	ensure(TransformIndex < ChildParticles.Num());

	if (const UVehicleSimBaseComponent* Component = Cast<UVehicleSimBaseComponent>(AttachedComponent))
	{
		FTransform ClusterUnionComponentTransform = ClusterUnionComponent->GetComponentTransform();
		FTransform ComponentTransform = AttachedComponent->GetComponentTransform().GetRelativeTransform(ClusterUnionComponentTransform);

		int TreeIndex = INDEX_NONE;

		Chaos::ISimulationModuleBase* NewModule = Component->CreateNewCoreModule();

		TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree = VehicleSimulationPT->AccessSimComponentTree();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FString DebugString;
		NewModule->GetDebugString(DebugString);
		UE_LOG(LogModularBase, Log, TEXT("CreateAssociatedSimComponents Found Sim Component %s (%s)"), *NewModule->GetDebugName(), *DebugString);
#endif

		FVector LocalOffset(0, 0, 0);
		FVector LocalOffsetCOM(0, 0, 0);
		FTransform PhysicsTransform = FTransform::Identity;

		FTransform InitialTransform = ChildParticles[TransformIndex].ChildToParent;
		InitialTransform.SetLocation(InitialTransform.GetLocation());
		NewModule->SetSimModuleTree(SimModuleTree.Get());
		TreeIndex = TreeUpdatesOut.AddNodeBelow(ParentIndex, NewModule);
		NewModule->SetTransformIndex(TransformIndex);
		NewModule->SetParticleIndex(ChildParticles[TransformIndex].ParticleIdx);

		NewModule->SetIntactTransform(FTransform::Identity);
		NewModule->SetClusteredTransform(InitialTransform);
		NewModule->SetClustered(true);

		FVehicleComponentData& ComponentData = ComponentToPhysicsObjects.FindOrAdd(AttachedComponent);
		ComponentData.Guid = GenerateNewGuid();
		NewModule->SetGuid(ComponentData.Guid);

		NewModule->SetInitialParticleTransform(InitialTransform);
		NewModule->SetComponentTransform(ComponentTransform);

		FTransform ClusterredTransform(FQuat::Identity, InitialTransform.GetLocation());
		NewModule->SetClusteredTransform(ClusterredTransform);

		ParentIndex = TreeIndex;

		if (bModularVehicle_SuspensionConstraint_Enabled)
		{
			if (NewModule->GetSimType() == Chaos::eSimType::Suspension)
			{
				CreateConstraint(NewModule);
			}
		}
	}

	// get this components children, not all children
	TArray<USceneComponent*> Children;
	AttachedComponent->GetChildrenComponents(false, Children);

	// recurse down tree, converting all SimComponents to proper simulation modules
	for (USceneComponent* Child : Children)
	{
		if (UVehicleSimBaseComponent* ChildSimComponent = Cast<UVehicleSimBaseComponent>(Child))
		{
			CreateAssociatedSimComponents(ChildSimComponent, ParentIndex, TransformIndex, TreeUpdatesOut);
		}
	}

	UpdatePhysicalProperties();
}

void UModularVehicleBaseComponent::UpdatePhysicalProperties()
{
	if (Chaos::FClusterUnionPhysicsProxy* Proxy = ClusterUnionComponent->GetPhysicsProxy())
	{
		Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>();
		Solver->EnqueueCommandImmediate([Proxy, this]() mutable
			{
				Proxy->GetParticle_Internal()->SetLinearEtherDrag(LinearDamping);
				Proxy->GetParticle_Internal()->SetAngularEtherDrag(AngularDamping);
			});

		// updating external particle currently doesn't update physics particle
		//	if (auto* Particle = static_cast<Chaos::FClusterUnionPhysicsProxy*>(Proxy)->GetParticle_External())
		//	{
		//		Particle->SetLinearEtherDrag(LinearDamping);
		//		Particle->SetAngularEtherDrag(AngularDamping);
		//	}
	}
}

void UModularVehicleBaseComponent::BeginPlay()
{
	Super::BeginPlay();

	const bool bHasAuthority = GetOwner()->HasAuthority();
	if (bHasAuthority)
	{
		if (bAutoAddComponentsFromWorld && (DelayClusteringCount == 0))
		{
			AddOverlappingComponentsToCluster();
		}
		else
		{
			AddGeometryCollectionsFromOwnedActor();
		}
	}
}

void UModularVehicleBaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bHasAuthority = GetOwner()->HasAuthority();
	if (bAutoAddComponentsFromWorld && bHasAuthority && (++ClusteringCount == DelayClusteringCount))
	{
		AddOverlappingComponentsToCluster();
	}

}

void UModularVehicleBaseComponent::CreateConstraint(Chaos::ISimulationModuleBase* NewModule)
{
	check(NewModule->GetSimType() == Chaos::eSimType::Suspension);
	Chaos::FSuspensionSimModule* SuspensionModule = static_cast<Chaos::FSuspensionSimModule*>(NewModule);

	const Chaos::FSuspensionSettings& Setup = SuspensionModule->Setup();
	const FVector& LocalOffset = SuspensionModule->GetInitialParticleTransform().GetLocation();

	if (ClusterUnionComponent && ClusterUnionComponent->GetPhysicsProxy())
	{
		const Chaos::FPhysicsObjectHandle& PhysicsObject = ClusterUnionComponent->GetPhysicsProxy()->GetPhysicsObjectHandle();

		if ( FChaosScene* Scene = static_cast<FChaosScene*>(FPhysicsObjectExternalInterface::GetScene({ &PhysicsObject, 1 })) )
		{
			FLockedWritePhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockWrite(Scene);
			if (const Chaos::FGeometryParticle* Particle = Interface->GetParticle(PhysicsObject))
			{
				FPhysicsConstraintHandle ConstraintHandle = FPhysicsInterface::CreateSuspension(PhysicsObject, LocalOffset);

				if (ConstraintHandle.IsValid())
				{
					ConstraintHandles.Add(ConstraintHandle);

					if (Chaos::FSuspensionConstraint* Constraint = static_cast<Chaos::FSuspensionConstraint*>(ConstraintHandle.Constraint))
					{
						Constraint->SetHardstopStiffness(1.0f);
						Constraint->SetSpringStiffness(Setup.SpringRate * 0.25f);
						Constraint->SetSpringPreload(Setup.SpringPreload);
						Constraint->SetSpringDamping(Setup.SpringDamping * 5.0f);
						Constraint->SetMinLength(-Setup.MaxRaise);
						Constraint->SetMaxLength(Setup.MaxDrop);
						Constraint->SetAxis(-Setup.SuspensionAxis);

						SuspensionModule->SetSuspensionConstraint(Constraint);
						SuspensionModule->SetConstraintIndex(ConstraintHandles.Num()-1);
					}
				}
			}
		}
	}
}

void UModularVehicleBaseComponent::DestroyConstraint(int ConstraintIndex)
{
	if (ConstraintIndex >= 0 && ConstraintIndex < ConstraintHandles.Num())
	{
		FPhysicsConstraintHandle ConstraintHandle = ConstraintHandles[ConstraintIndex];
		FPhysicsCommand::ExecuteWrite(ConstraintHandle, [&](const FPhysicsConstraintHandle& Constraint)
			{
				FPhysicsInterface::ReleaseConstraint(ConstraintHandle);
			});

		ConstraintHandles[ConstraintIndex].Reset();
	}

}

void UModularVehicleBaseComponent::DestroyAllConstraints()
{
	if (ConstraintHandles.Num() > 0)
	{
		for (FPhysicsConstraintHandle ConstraintHandle : ConstraintHandles)
		{
			if (ConstraintHandle.IsValid())
			{
				FPhysicsCommand::ExecuteWrite(ConstraintHandle, [&](const FPhysicsConstraintHandle& Constraint)
					{
						FPhysicsInterface::ReleaseConstraint(ConstraintHandle);
					});
			}
		}
	}
	ConstraintHandles.Empty();

}

void UModularVehicleBaseComponent::EnableConstraint(int ConstraintIndex, bool bEnabled)
{
	if (ConstraintIndex >= 0 && ConstraintIndex < ConstraintHandles.Num())
	{
		FPhysicsConstraintHandle ConstraintHandle = ConstraintHandles[ConstraintIndex];
		FPhysicsCommand::ExecuteWrite(ConstraintHandle, [&](const FPhysicsConstraintHandle& Constraint)
			{
				if (Chaos::FSuspensionConstraint* SuspensionConstraint = static_cast<Chaos::FSuspensionConstraint*>(ConstraintHandle.Constraint))
				{
					SuspensionConstraint->SetEnabled(bEnabled);
				}
			});

		ConstraintHandles[ConstraintIndex].Reset();
	}

}

int32 UModularVehicleBaseComponent::FindComponentAddOrder(UPrimitiveComponent* InComponent)
{
	int32 ComponentAddOrder = INDEX_NONE;
	for (const FConstructionData& Data : ConstructionDatas)
	{
		if (Data.Component == InComponent)
		{
			ComponentAddOrder = Data.ConstructionIndex;
			break;
		}
	}
	return ComponentAddOrder;
}

bool UModularVehicleBaseComponent::FindAndRemoveNextPendingUpdate(int32 NextIndex, Chaos::FSimTreeUpdates* OutData)
{
	bool bFound = false;
	for (const TPair<TObjectKey<UPrimitiveComponent>, Chaos::FSimTreeUpdates>& Kvp : PendingTreeUpdates)
	{
		if (FindComponentAddOrder(Kvp.Key.ResolveObjectPtr()) == NextIndex)
		{
			*OutData = PendingTreeUpdates.FindAndRemoveChecked(Kvp.Key.ResolveObjectPtr());
			bFound = true;
			break;
		}
	}

	return bFound;
}

void UModularVehicleBaseComponent::AddActorsToIgnore(TArray<AActor*>& ActorsIn)
{
	for (AActor* Actor : ActorsIn)
	{
		ActorsToIgnore.AddUnique(Actor);
	}
}

void UModularVehicleBaseComponent::RemoveActorsToIgnore(TArray<AActor*>& ActorsIn)
{
	for (AActor* Actor : ActorsIn)
	{
		ActorsToIgnore.Remove(Actor);
	}
}

void UModularVehicleBaseComponent::PreTickGT(float DeltaTime)
{
	if (PendingTreeUpdates.Num() > 0)
	{
		Chaos::FSimTreeUpdates Updates;
		while (FindAndRemoveNextPendingUpdate(LastComponentAddIndex + 1, &Updates))
		{
			ActionTreeUpdates(&Updates);
			LastComponentAddIndex++;
		}
	}

	// process control inputs and other data
	UpdateState(DeltaTime);

}

void UModularVehicleBaseComponent::UpdateState(float DeltaTime)
{
	// update input values
	bool bProcessLocally = IsLocallyControlled();

	// IsLocallyControlled will fail if the owner is unpossessed (i.e. Controller == nullptr);
	// Should we remove input instead of relying on replicated state in that case?
	if (bProcessLocally && PVehicleOutput)
	{
		// Apply Inputs locally
		SteeringInput = SteeringInputRate.InterpInputValue(DeltaTime, SteeringInput, RawSteeringInput);
		ThrottleInput = ThrottleInputRate.InterpInputValue(DeltaTime, ThrottleInput, RawThrottleInput);
		BrakeInput = BrakeInputRate.InterpInputValue(DeltaTime, BrakeInput, RawBrakeInput);
		HandbrakeInput = HandbrakeInputRate.InterpInputValue(DeltaTime, HandbrakeInput, RawHandbrakeInput);
		PitchInput = PitchInputRate.InterpInputValue(DeltaTime, PitchInput, RawPitchInput);
		RollInput = RollInputRate.InterpInputValue(DeltaTime, RollInput, RawRollInput);
		YawInput = YawInputRate.InterpInputValue(DeltaTime, YawInput, RawYawInput);
		BoostInput = BoostInputRate.InterpInputValue(DeltaTime, BoostInput, RawBoostInput);
		DriftInput = DriftInputRate.InterpInputValue(DeltaTime, DriftInput, RawDriftInput);
		ReverseInput = RawReverseInput;

		if (!bUsingNetworkPhysicsPrediction)
		{
			// and send to server - (ServerUpdateState_Implementation below)
			ServerUpdateState(SteeringInput, ThrottleInput, BrakeInput, HandbrakeInput, -1, RollInput, PitchInput, YawInput, BoostInput, DriftInput, ReverseInput);
		}

		if (PawnOwner && PawnOwner->IsNetMode(NM_Client))
		{
			MarkForClientCameraUpdate();
		}
	}
	else if (!bUsingNetworkPhysicsPrediction)
	{
		// use replicated values for remote pawns
		ThrottleInput = ReplicatedState.Throttle;
		SteeringInput = ReplicatedState.Steering;
		BrakeInput = ReplicatedState.Brake;
		HandbrakeInput = ReplicatedState.Handbrake;
		PitchInput = ReplicatedState.Pitch;
		RollInput = ReplicatedState.Roll;
		YawInput = ReplicatedState.Yaw;
		BoostInput = ReplicatedState.Boost;
		DriftInput = ReplicatedState.Drift;
		ReverseInput = ReplicatedState.Reverse;
	}
}

bool UModularVehicleBaseComponent::ServerUpdateState_Validate(float InSteeringInput, float InThrottleInput, float InBrakeInput, float InHandbrakeInput, int32 InCurrentGear, float InRollInput, float InPitchInput, float InYawInput, float InBoostInput, float InDriftInput, bool InReverseInput)
{
	return true;
}

void UModularVehicleBaseComponent::ServerUpdateState_Implementation(float InSteeringInput, float InThrottleInput, float InBrakeInput
	, float InHandbrakeInput, int32 InCurrentGear, float InRollInput, float InPitchInput, float InYawInput, float InBoostInput, float InDriftInput, bool InReverseInput)
{
	SteeringInput = InSteeringInput;
	ThrottleInput = InThrottleInput;
	BrakeInput = InBrakeInput;
	HandbrakeInput = InHandbrakeInput;
	RollInput = InRollInput;
	PitchInput = InPitchInput;
	YawInput = InYawInput;
	BoostInput = InBoostInput;
	DriftInput = InDriftInput;
	ReverseInput = InReverseInput;

	// update state of inputs
	ReplicatedState.Steering = InSteeringInput;
	ReplicatedState.Throttle = InThrottleInput;
	ReplicatedState.Brake = InBrakeInput;
	ReplicatedState.Handbrake = InHandbrakeInput;
	ReplicatedState.Roll = InRollInput;
	ReplicatedState.Pitch = InPitchInput;
	ReplicatedState.Yaw = InYawInput;
	ReplicatedState.Boost = InBoostInput;
	ReplicatedState.Drift = InDriftInput;
	ReplicatedState.Reverse = InReverseInput;
}


TUniquePtr<FModularVehicleAsyncInput> UModularVehicleBaseComponent::SetCurrentAsyncData(int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp)
{
	TUniquePtr<FModularVehicleAsyncInput> CurInput = MakeUnique<FModularVehicleAsyncInput>();
	SetCurrentAsyncDataInternal(CurInput.Get(), InputIdx, CurOutput, NextOutput, Alpha, VehicleManagerTimestamp);
	return CurInput;
}

/************************************************************************/
/* Setup the current async I/O data                                     */
/************************************************************************/
void UModularVehicleBaseComponent::SetCurrentAsyncDataInternal(FModularVehicleAsyncInput* CurInput, int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp)
{
	ensure(CurAsyncInput == nullptr);	//should be reset after it was filled
	ensure(CurAsyncOutput == nullptr);	//should get reset after update is done

	CurAsyncInput = CurInput;
	CurAsyncInput->SetVehicle(this);
	NextAsyncOutput = nullptr;
	OutputInterpAlpha = 0.f;

	// We need to find our vehicle in the output given
	if (CurOutput)
	{
		for (int32 PendingOutputIdx = 0; PendingOutputIdx < OutputsWaitingOn.Num(); ++PendingOutputIdx)
		{
			// Found the correct pending output, use index to get the vehicle.
			if (OutputsWaitingOn[PendingOutputIdx].Timestamp == CurOutput->Timestamp)
			{
				const int32 VehicleIdx = OutputsWaitingOn[PendingOutputIdx].Idx;
				FModularVehicleAsyncOutput* VehicleOutput = CurOutput->VehicleOutputs[VehicleIdx].Get();
				if (VehicleOutput && VehicleOutput->bValid && VehicleOutput->Type == CurAsyncType)
				{
					CurAsyncOutput = VehicleOutput;

					if (NextOutput && NextOutput->Timestamp == CurOutput->Timestamp)
					{
						// This can occur when sub-stepping - in this case, VehicleOutputs will be in the same order in NextOutput and CurOutput.
						FModularVehicleAsyncOutput* VehicleNextOutput = NextOutput->VehicleOutputs[VehicleIdx].Get();
						if (VehicleNextOutput && VehicleNextOutput->bValid && VehicleNextOutput->Type == CurAsyncType)
						{
							NextAsyncOutput = VehicleNextOutput;
							OutputInterpAlpha = Alpha;
						}
					}
				}

				// these are sorted by timestamp, we are using latest, so remove entries that came before it.
				TArray<FAsyncOutputWrapper> NewOutputsWaitingOn;
				for (int32 CopyIndex = PendingOutputIdx; CopyIndex < OutputsWaitingOn.Num(); ++CopyIndex)
				{
					NewOutputsWaitingOn.Add(OutputsWaitingOn[CopyIndex]);
				}

				OutputsWaitingOn = MoveTemp(NewOutputsWaitingOn);
				break;
			}
		}

	}

	if (NextOutput && CurOutput)
	{
		if (NextOutput->Timestamp != CurOutput->Timestamp)
		{
			// NextOutput and CurOutput occurred in different steps, so we need to search for our specific vehicle.
			for (int32 PendingOutputIdx = 0; PendingOutputIdx < OutputsWaitingOn.Num(); ++PendingOutputIdx)
			{
				// Found the correct pending output, use index to get the vehicle.
				if (OutputsWaitingOn[PendingOutputIdx].Timestamp == NextOutput->Timestamp)
				{
					FModularVehicleAsyncOutput* VehicleOutput = NextOutput->VehicleOutputs[OutputsWaitingOn[PendingOutputIdx].Idx].Get();
					if (VehicleOutput && VehicleOutput->bValid && VehicleOutput->Type == CurAsyncType)
					{
						NextAsyncOutput = VehicleOutput;
						OutputInterpAlpha = Alpha;
					}
					break;
				}
			}
		}
	}

	FAsyncOutputWrapper& NewOutput = OutputsWaitingOn.AddDefaulted_GetRef();
	NewOutput.Timestamp = VehicleManagerTimestamp;
	NewOutput.Idx = InputIdx;
}

inline IPhysicsProxyBase* UModularVehicleBaseComponent::GetPhysicsProxy() const { return ClusterUnionComponent ? ClusterUnionComponent->GetPhysicsProxy() : nullptr; }


void UModularVehicleBaseComponent::SetClusterComponent(UClusterUnionComponent* InPhysicalComponent)
{
	ClusterUnionComponent = InPhysicalComponent;
}

/***************************************************************************/
/* READ OUTPUT DATA - Access the async output data from the Physics Thread */
/***************************************************************************/
void UModularVehicleBaseComponent::ParallelUpdate(float DeltaTime)
{
	if (FModularVehicleAsyncOutput* CurrentOutput = static_cast<FModularVehicleAsyncOutput*>(CurAsyncOutput))
	{
		if (CurrentOutput->bValid && PVehicleOutput)
		{
			if (const FModularVehicleAsyncOutput* NextOutput = static_cast<FModularVehicleAsyncOutput*>(NextAsyncOutput))
			{
				PVehicleOutput->Clean();

				int NumItems = CurrentOutput->VehicleSimOutput.SimTreeOutputData.Num();
				PVehicleOutput->SimTreeOutputData.Reserve(NumItems);

				for (int I = 0; I < NumItems; I++)
				{
					// #TODO: check we are lerping current/next of the same thing - the number can grow/shrink when construction/destruction happens
					if (I < NextOutput->VehicleSimOutput.SimTreeOutputData.Num())
					{
						Chaos::FSimOutputData* CurrentSimData = CurrentOutput->VehicleSimOutput.SimTreeOutputData[I];
						Chaos::FSimOutputData* NextSimData = NextOutput->VehicleSimOutput.SimTreeOutputData[I];
						PVehicleOutput->SimTreeOutputData.EmplaceAt(I, CurrentSimData->MakeNewData());
						PVehicleOutput->SimTreeOutputData[I]->Lerp(*CurrentSimData, *NextSimData, OutputInterpAlpha);

						// extract/cache some generally useful values as we go as trying to locate this data later requires a search
						if (PVehicleOutput->SimTreeOutputData[I]->GetType() == Chaos::eSimType::Transmission)
						{
							// if there is more than one transmission then the last one will inform us of the current gear
							CurrentGear = static_cast<Chaos::FTransmissionOutputData*>(PVehicleOutput->SimTreeOutputData[I])->CurrentGear;
						}
						else if (PVehicleOutput->SimTreeOutputData[I]->GetType() == Chaos::eSimType::Engine)
						{
							// if there is more than one engine then the last one will inform us of the engine RPM
							Chaos::FEngineOutputData* Engine = static_cast<Chaos::FEngineOutputData*>(PVehicleOutput->SimTreeOutputData[I]);

							EngineRPM = Engine->RPM;
							EngineTorque = Engine->Torque;
						}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
						PVehicleOutput->SimTreeOutputData[I]->DebugString = NextOutput->VehicleSimOutput.SimTreeOutputData[I]->DebugString;
#endif
					}
				}
			}

		}
	}
}

/************************************************************************/
/* PASS ANY INPUTS TO THE PHYSICS THREAD SIMULATION IN HERE             */
/************************************************************************/
void UModularVehicleBaseComponent::Update(float DeltaTime)
{
	if (CurAsyncInput && ClusterUnionComponent && ClusterUnionComponent->GetPhysicsProxy())
	{

		CurAsyncInput->Proxy = ClusterUnionComponent->GetPhysicsProxy();

		FModularVehicleAsyncInput* AsyncInput = static_cast<FModularVehicleAsyncInput*>(CurAsyncInput);

		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Throttle = ThrottleInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Boost = BoostInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Drift = DriftInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Brake = BrakeInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Steering = SteeringInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Handbrake = HandbrakeInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Roll = RollInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Pitch = PitchInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Yaw = YawInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.Reverse = ReverseInput;
		AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.KeepAwake = bKeepVehicleAwake;

		FCollisionQueryParams TraceParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(), false, nullptr);
		TraceParams.bReturnPhysicalMaterial = true;	// we need this to get the surface friction coefficient
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		TraceParams.bTraceComplex = bSuspensionTraceComplex;
		AsyncInput->PhysicsInputs.TraceParams = TraceParams;
		AsyncInput->PhysicsInputs.TraceCollisionResponse = SuspensionTraceCollisionResponses;
	}


}

void UModularVehicleBaseComponent::FinalizeSimCallbackData(FChaosSimModuleManagerAsyncInput& Input)
{
	CurAsyncInput = nullptr;
	CurAsyncOutput = nullptr;
}

const FTransform& UModularVehicleBaseComponent::GetComponentTransform() const
{
	return MyComponent->GetComponentTransform();
}


void UModularVehicleBaseComponent::ActionTreeUpdates(Chaos::FSimTreeUpdates* NextTreeUpdates)
{
	Chaos::FClusterUnionPhysicsProxy* Proxy = ClusterUnionComponent->GetPhysicsProxy();
	Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>();
	Solver->EnqueueCommandImmediate([Proxy, this, NextTreeUpdates = *NextTreeUpdates]() mutable
		{
			if (VehicleSimulationPT)
			{
				TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree = VehicleSimulationPT->AccessSimComponentTree();
				SimModuleTree->AppendTreeUpdates(NextTreeUpdates);
				FModularVehicleBuilder::FixupTreeLinks(SimModuleTree);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bModularVehicle_DumpModuleTreeStructure_Enabled)
				{
					UE_LOG(LogTemp, Warning, TEXT("SimTreeModules:") );
					for (int I = 0; I < SimModuleTree->GetNumNodes(); I++)
					{
						if (Chaos::ISimulationModuleBase* Module = SimModuleTree->GetNode(I).SimModule)
						{
							FString String;
							Module->GetDebugString(String);
							UE_LOG(LogTemp, Warning, TEXT("..%s"), *String);
						}
					}
				}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

				// Network replication data needs to be updated, this is currently studily slow
				if (NetworkPhysicsComponent)
				{
					TSharedPtr<Chaos::FBaseRewindHistory>& History = NetworkPhysicsComponent->GetStateHistory();
					Chaos::TDataRewindHistory<FNetworkModularVehicleStates>* StateHistory = static_cast<Chaos::TDataRewindHistory<FNetworkModularVehicleStates>*>(History.Get());
					if (StateHistory)
					{
						// #TODO: we are rebuilding from scratch every time there is a single change, there must be a better way!
						// not sure of it is safe to update the data at this time?
						for (int I = 0; I < StateHistory->GetDataHistory().Num(); I++)
						{
							FNetworkModularVehicleStates& State = StateHistory->GetDataHistory()[I];
							State.ModuleData.Empty();

							VehicleSimulationPT->AccessSimComponentTree()->GenerateReplicationStructure(State.ModuleData);
						}
					}
				}
			}
		});
}

void UModularVehicleBaseComponent::AddComponentToSimulation(UPrimitiveComponent* InComponent, const TArray<FClusterUnionBoneData>& BonesData, const TArray<FClusterUnionBoneData>& RemovedBoneIDs, bool bIsNew)
{
	check(ClusterUnionComponent);

	int32 ComponentAddOrder = INDEX_NONE;
	if (InComponent && bIsNew)
	{
		UE_LOG(LogModularBase, Log, TEXT("AddComponentToSimulation %s, NetMode %d"), *InComponent->GetName(), InComponent->GetNetMode());
		
		if (ClusterUnionComponent->IsAuthority())
		{
			// retain the order that the components were constructed
			FConstructionData ConstructionData;
			ConstructionData.Component = InComponent;
			ConstructionData.ConstructionIndex = NextConstructionIndex;
			ConstructionDatas.Add(ConstructionData);
			ComponentAddOrder = NextConstructionIndex++;
		}
		else
		{
			ComponentAddOrder = FindComponentAddOrder(InComponent);
		}

		int ParentID = INDEX_NONE; // always at root
		Chaos::FSimTreeUpdates LatestTreeUpdates;
		CreateAssociatedSimComponents(InComponent, ParentID, NextTransformIndex, LatestTreeUpdates);

		Chaos::FSimTreeUpdates* NextTreeUpdates = nullptr;
		if (LastComponentAddIndex + 1 == ComponentAddOrder)
		{
			// has the next in line just come in..
			NextTreeUpdates = &LatestTreeUpdates;
			LastComponentAddIndex = ComponentAddOrder;
			ActionTreeUpdates(NextTreeUpdates);
		}
		else
		{
			// add to pending and action later
			PendingTreeUpdates.Add(InComponent, MoveTemp(LatestTreeUpdates));
		}

		NextTransformIndex++;
	}
}

void UModularVehicleBaseComponent::RemoveComponentFromSimulation(UPrimitiveComponent* InComponent, const TArray<FClusterUnionBoneData>& RemovedBonesData)
{
	if (InComponent && VehicleSimulationPT)
	{
		UE_LOG(LogModularBase, Log, TEXT("UModularVehicleBaseComponent::RemoveComponentFromSimulation: %s, NetMode %d"), *InComponent->GetFullName(), InComponent->GetNetMode());

		Chaos::FSimTreeUpdates LatestTreeUpdates;

		TArray<USceneComponent*> Components;
		InComponent->GetChildrenComponents(true, Components);

		for (USceneComponent* ComponentPart : Components)
		{
			if (UVehicleSimBaseComponent* ChangedComponent = Cast<UVehicleSimBaseComponent>(ComponentPart))
			{
				if (FVehicleComponentData* ComponentData = ComponentToPhysicsObjects.Find(ChangedComponent))
				{
					LatestTreeUpdates.RemoveNode(ComponentData->Guid);
					ComponentToPhysicsObjects.Remove(ChangedComponent);
				}
			}
		}

		TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree = VehicleSimulationPT->AccessSimComponentTree();
		for (const Chaos::FPendingModuleDeletions& TreeUpdate : LatestTreeUpdates.GetDeletedModules())
		{
			for (int Index = 0; Index < SimModuleTree->GetNumNodes(); Index++)
			{
				if (Chaos::ISimulationModuleBase* SimModule = SimModuleTree->GetNode(Index).SimModule)
				{
					if (SimModule->GetGuid() == TreeUpdate.Guid)
					{
						SimModule->SetAnimationEnabled(false);
						SimModule->SetStateFlags(Chaos::eSimModuleState::Disabled);

						if (SimModule->GetSimType() == Chaos::eSimType::Suspension)
						{
							Chaos::FSuspensionSimModule* SuspensionModule = static_cast<Chaos::FSuspensionSimModule*>(SimModule);
							DestroyConstraint(SuspensionModule->GetConstraintIndex());
						}

						break;
					}
				}
			}
		}

		Chaos::FClusterUnionPhysicsProxy* Proxy = ClusterUnionComponent->GetPhysicsProxy();
		Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>();
		Solver->EnqueueCommandImmediate([Proxy, this, LatestTreeUpdates = LatestTreeUpdates]() mutable
			{

				if (VehicleSimulationPT)
				{
					TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree = VehicleSimulationPT->AccessSimComponentTree();
					SimModuleTree->AppendTreeUpdates(LatestTreeUpdates);
				}
			});

		NextTransformIndex--;
	}

}

void UModularVehicleBaseComponent::SetLocallyControlled(bool bLocallyControlledIn)
{
	bIsLocallyControlled = false;
	if (UWorld* World = GetWorld())
	{
		if (!World->IsNetMode(NM_DedicatedServer))
		{
			bIsLocallyControlled = bLocallyControlledIn;
		}
	}

	if (bUsingNetworkPhysicsPrediction && NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->SetIsRelayingLocalInputs(bLocallyControlledIn);
	}

}

void UModularVehicleBaseComponent::CreateVehicleSim()
{
	UWorld* World = GetWorld();

	// register our vehicle with the modular vehicle Sim Manager
	if (World)
	{
		if (World->IsGameWorld())
		{
			// create the simulation class
			VehicleSimulationPT = MakeUnique<FModularVehicleSimulationCU>(bUsingNetworkPhysicsPrediction, (int8)World->GetNetMode());

			// create physics output container
			PVehicleOutput = MakeUnique<FPhysicsVehicleOutput>();

			// generate the simulation tree with initial components
			FModularVehicleBuilder::GenerateSimTree(this);

			FPhysScene* PhysScene = World->GetPhysicsScene();

			if (FChaosSimModuleManager* SimManager = FChaosSimModuleManager::GetManagerFromScene(PhysScene))
			{
				SimManager->AddVehicle(this);
			}
		}
	}

}

void UModularVehicleBaseComponent::DestroyVehicleSim()
{
	UWorld* World = GetWorld();
	if (World->IsGameWorld())
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();
		check(PhysScene);

		if (Chaos::FPhysicsSolver* LocalSolver = PhysScene->GetSolver())
		{
			if (FChaosSimModuleManager* SimManager = FChaosSimModuleManager::GetManagerFromScene(PhysScene))
			{
				SimManager->RemoveVehicle(this);
			}

			LocalSolver->EnqueueCommandImmediate([this, PhysScene = PhysScene]() mutable
				{
					if (PVehicleOutput.IsValid())
					{
						PVehicleOutput.Reset(nullptr);
					}

					if (VehicleSimulationPT)
					{
						VehicleSimulationPT->Terminate();
						VehicleSimulationPT.Reset(nullptr);
					}
				});
		}

	}

}

void UModularVehicleBaseComponent::AddOverlappingComponentsToCluster()
{
	if (UWorld* World = GetWorld())
	{
		FVector HalfSize = AutoAddOverlappingBoxSize * 0.5f;
		FBox LocalBox = FBox(-HalfSize, HalfSize);
		TArray<FOverlapResult> OverlapResults;
		FCollisionShape CollisionBox;
		CollisionBox.SetBox((FVector3f)LocalBox.GetExtent());

		const FCollisionQueryParams QueryParams;
		const FCollisionResponseParams ResponseParams;
		const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldDynamic;
		const bool bOverlapHit = World->OverlapMultiByChannel(OverlapResults, GetActorLocation(), FQuat::Identity, CollisionChannel, CollisionBox, QueryParams, ResponseParams);

		TArray<int32> BoneIds;
		BoneIds.Add(0);
		// Test each overlapped object for a hit result
		for (FOverlapResult OverlapResult : OverlapResults)
		{
			if (UGeometryCollectionComponent* OverlapGCComponent = Cast<UGeometryCollectionComponent>(OverlapResult.Component.Get()))
			{
				ClusterUnionComponent->AddComponentToCluster(OverlapGCComponent, BoneIds);
			}
		}
	}
}

void UModularVehicleBaseComponent::AddGeometryCollectionsFromOwnedActor()
{
	TArray<int32> BoneIds;
	BoneIds.Add(0);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (UClusterUnionComponent* ClusterUnion = Pawn->GetComponentByClass<UClusterUnionComponent>())
		{
			TArray<USceneComponent*> ChildComponents;
			ClusterUnion->GetChildrenComponents(true, ChildComponents);

			for (USceneComponent* Component : ChildComponents)
			{
				if (UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(Component))
				{
					ClusterUnionComponent->AddComponentToCluster(GCComponent, BoneIds);
				}
			}
		}
	}
}

void UModularVehicleBaseComponent::SetThrottleInput(float Throttle)
{
	RawThrottleInput = FMath::Clamp(Throttle, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetBoostInput(float Boost)
{
	RawBoostInput = FMath::Clamp(Boost, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetDriftInput(float Drift)
{
	RawDriftInput = FMath::Clamp(Drift, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::IncreaseThrottleInput(float ThrottleDelta)
{
	RawThrottleInput = FMath::Clamp(RawThrottleInput + ThrottleDelta, 0.f, 1.0f);
}

void UModularVehicleBaseComponent::DecreaseThrottleInput(float ThrottleDelta)
{
	RawThrottleInput = FMath::Clamp(RawThrottleInput - ThrottleDelta, 0.f, 1.0f);
}

void UModularVehicleBaseComponent::SetBrakeInput(float Brake)
{
	RawBrakeInput = FMath::Clamp(Brake, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetSteeringInput(float Steering)
{
	RawSteeringInput = FMath::Clamp(Steering, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetPitchInput(float Pitch)
{
	RawPitchInput = FMath::Clamp(Pitch, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetRollInput(float Roll)
{
	RawRollInput = FMath::Clamp(Roll, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetYawInput(float Yaw)
{
	RawYawInput = FMath::Clamp(Yaw, -1.0f, 1.0f);
}

void UModularVehicleBaseComponent::SetHandbrakeInput(float Handbrake)
{
	RawHandbrakeInput = Handbrake;
}

void UModularVehicleBaseComponent::SetReverseInput(bool Reverse)
{
	RawReverseInput = Reverse;
}


void UModularVehicleBaseComponent::SetGearInput(int32 Gear)
{
	RawGearInput = Gear;
}


int32 UModularVehicleBaseComponent::GetCurrentGear()
{
	return CurrentGear;
}

bool UModularVehicleBaseComponent::IsReversing()
{
	return (GetCurrentGear() < 0);
}


//-=====================================================
// Networking Replication

void UModularVehicleBaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UModularVehicleBaseComponent, ReplicatedState);
	DOREPLIFETIME(UModularVehicleBaseComponent, ConstructionDatas);
}

void UModularVehicleBaseComponent::ShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	UFont* RenderFont = GEngine->GetMediumFont();

	// draw input values
	Canvas->SetDrawColor(FColor::White);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Throttle Raw  (%3.2f) %3.2f"), RawThrottleInput, ThrottleInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Boost Raw     (%3.2f) %3.2f"), RawBoostInput, BoostInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Drift Raw     (%3.2f) %3.2f"), RawDriftInput, DriftInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Brake Raw     (%3.2f) %3.2f"), RawBrakeInput, BrakeInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Steering Raw  (%3.2f) %3.2f"), RawSteeringInput, SteeringInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Handbrake Raw (%3.2f) %3.2f"), RawHandbrakeInput, HandbrakeInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Roll Raw      (%3.2f) %3.2f"), RawRollInput, RollInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Pitch Raw     (%3.2f) %3.2f"), RawPitchInput, PitchInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Yaw Raw       (%3.2f) %3.2f"), RawYawInput, YawInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Reverse Raw   (%3.2f) %3.2f"), RawReverseInput, ReverseInput), 4, YPos);

	YPos += 10;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (Chaos::FSimOutputData* Data : PVehicleOutput->SimTreeOutputData)
	{
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("%s"), *Data->ToString()), 4, YPos);
	}
#endif

}
