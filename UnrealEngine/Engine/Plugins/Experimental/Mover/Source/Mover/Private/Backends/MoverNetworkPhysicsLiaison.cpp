// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPhysicsLiaison.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Components/ShapeComponent.h"
#include "Framework/Threading.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "MovementModeStateMachine.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"
#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/PhysicsMoverManager.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"

//////////////////////////////////////////////////////////////////////////

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverInputs

void FNetworkPhysicsMoverInputs::ApplyData(UActorComponent* NetworkComponent) const
{
	if (NetworkComponent)
	{
		if (UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->SetCurrentInputData(InputCmdContext);
		}
	}
}

void FNetworkPhysicsMoverInputs::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->GetCurrentInputData(InputCmdContext);
		}
	}
}

bool FNetworkPhysicsMoverInputs::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	if (PackageMap)
	{
		InputCmdContext.NetSerialize(FNetSerializeParams(Ar));
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}
	
	return bOutSuccess;
}

void FNetworkPhysicsMoverInputs::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverInputs& MinDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MinData);
	const FNetworkPhysicsMoverInputs& MaxDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MaxData);

	const float LerpFactor = (LocalFrame - MinDataInput.LocalFrame) / (MaxDataInput.LocalFrame - MinDataInput.LocalFrame);

	const FCharacterDefaultInputs* MinInput = MinDataInput.InputCmdContext.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FCharacterDefaultInputs* MaxInput = MaxDataInput.InputCmdContext.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	FCharacterDefaultInputs& LocalInput = InputCmdContext.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (MinInput && MaxInput)
	{
		// Note, this ignores movement base as this is not used by the physics mover
		const FCharacterDefaultInputs* ClosestInputs = LerpFactor < 0.5f ? MinInput : MaxInput;
		LocalInput.bIsJumpJustPressed = ClosestInputs->bIsJumpJustPressed;
		LocalInput.bIsJumpPressed = ClosestInputs->bIsJumpPressed;
		LocalInput.SuggestedMovementMode = ClosestInputs->SuggestedMovementMode;

		LocalInput.SetMoveInput(ClosestInputs->GetMoveInputType(), FMath::Lerp(MinInput->GetMoveInput(), MaxInput->GetMoveInput(), LerpFactor));
		LocalInput.OrientationIntent = FMath::Lerp(MinInput->OrientationIntent, MaxInput->OrientationIntent, LerpFactor);
		LocalInput.ControlRotation = FMath::Lerp(MinInput->ControlRotation, MaxInput->ControlRotation, LerpFactor);

	}
	else if (MinInput)
	{
		LocalInput = *MinInput;
	}
	else if (MaxInput)
	{
		LocalInput = *MaxInput;
	}
}

void FNetworkPhysicsMoverInputs::MergeData(const FNetworkPhysicsData& FromData)
{
	const FNetworkPhysicsMoverInputs& FromDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(FromData);

	if (const FCharacterDefaultInputs* FromInput = FromDataInput.InputCmdContext.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		FCharacterDefaultInputs& LocalInputs = InputCmdContext.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

		LocalInputs.bIsJumpJustPressed |= FromInput->bIsJumpJustPressed;
		LocalInputs.bIsJumpPressed |= FromInput->bIsJumpPressed;
	}
}

void FNetworkPhysicsMoverInputs::ValidateData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->ValidateInputData(InputCmdContext);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverState

void FNetworkPhysicsMoverState::ApplyData(UActorComponent* NetworkComponent) const
{
	if (NetworkComponent)
	{
		if (UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->SetCurrentStateData(SyncStateContext);
		}
	}
}

void FNetworkPhysicsMoverState::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->GetCurrentStateData(SyncStateContext);
		}
	}
}

bool FNetworkPhysicsMoverState::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	if (PackageMap)
	{
		FNetSerializeParams Params(Ar);
		SyncStateContext.NetSerialize(Params);
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}

	return bOutSuccess;
}

void FNetworkPhysicsMoverState::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverState& MinState = static_cast<const FNetworkPhysicsMoverState&>(MinData);
	const FNetworkPhysicsMoverState& MaxState = static_cast<const FNetworkPhysicsMoverState&>(MaxData);

	const float LerpFactor = (LocalFrame - MinState.LocalFrame) / (MaxState.LocalFrame - MinState.LocalFrame);
	SyncStateContext.Interpolate(&MinState.SyncStateContext, &MaxState.SyncStateContext, LerpFactor);
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent

void UMoverNetworkPhysicsLiaisonComponent::GetCurrentInputData(OUT FMoverInputCmdContext& InputCmd) const
{
	InputCmd = NetInputCmd;
}

void UMoverNetworkPhysicsLiaisonComponent::GetCurrentStateData(OUT FMoverSyncState& SyncState) const
{
	SyncState = NetSyncState;
}

void UMoverNetworkPhysicsLiaisonComponent::SetCurrentInputData(const FMoverInputCmdContext& InputCmd)
{
	NetInputCmd = InputCmd;
}

void UMoverNetworkPhysicsLiaisonComponent::SetCurrentStateData(const FMoverSyncState& SyncState)
{
	NetSyncState = SyncState;
}

bool UMoverNetworkPhysicsLiaisonComponent::ValidateInputData(FMoverInputCmdContext& InputCmd) const
{
	// TODO - proper data validation
	return true;
}

UMoverNetworkPhysicsLiaisonComponent::UMoverNetworkPhysicsLiaisonComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene_Chaos* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bUsingAsyncPhysics = Solver->IsUsingAsyncResults();
			}
		}
	}

	if (Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
	{
		// Network physics relies on movement being replicated
		SetIsReplicatedByDefault(true);

		if (AActor* MyActor = GetOwner())
		{
			MyActor->SetReplicatingMovement(true);
			MyActor->SetReplicateMovement(true);
		}

		NetworkPhysicsComponent = CreateDefaultSubobject<UNetworkPhysicsComponent>(TEXT("PhysMover_NetworkPhysicsComponent"));
		NetworkPhysicsComponent->SetNetAddressable(); // Make DSO components net addressable
		NetworkPhysicsComponent->SetIsReplicated(true);
		NetworkPhysicsComponent->RegisterComponent();
		NetworkPhysicsComponent->InitializeComponent();
	}
}

//////////////////////////////////////////////////////////////////////////
//  UMoverNetworkPhysicsLiaisonComponent IMoverBackendLiaisonInterface

float UMoverNetworkPhysicsLiaisonComponent::GetCurrentSimTimeMs()
{
	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			return bUsingAsyncPhysics ? Solver->GetAsyncDeltaTime() * GetCurrentSimFrame() * 1000.0f : Solver->GetSolverTime() * 1000.0f;
		}
	}

	return 0.0f;
}

int32 UMoverNetworkPhysicsLiaisonComponent::GetCurrentSimFrame()
{
	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			return Solver->GetCurrentFrame() + GetNetworkPhysicsTickOffset();
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent UObject interface

void UMoverNetworkPhysicsLiaisonComponent::OnRegister()
{
	Super::OnRegister();

	// Need to set this here as physics creation requires access to MoverComp
	
	if ((MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>()) != nullptr)
	{
	
		CommonMovementSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
		check(CommonMovementSettings);

		if (MoverComp->UpdatedCompAsPrimitive)
		{
			MoverComp->UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &ThisClass::OnComponentPhysicsStateChanged);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnUnregister()
{
	if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
	{
		MoverComp->UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.RemoveDynamic(this, &ThisClass::OnComponentPhysicsStateChanged);
	}

	Super::OnUnregister();
}

void UMoverNetworkPhysicsLiaisonComponent::SetupConstraint()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
				{
					if (FBodyInstance* BI = MoverComp->UpdatedCompAsPrimitive->GetBodyInstance())
					{
						if (Chaos::FSingleParticlePhysicsProxy* CharacterProxy = BI->ActorHandle)
						{
							// Create and register the constraint
							Constraint = MakeUnique<Chaos::FCharacterGroundConstraint>();
							Constraint->Init(CharacterProxy);
							Solver->RegisterObject(Constraint.Get());

							// Set the common settings
							// The rest get set every frame depending on the current movement mode
							Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);
							Constraint->SetVerticalAxis(MoverComp->GetUpDirection());

							// Enable Physics Simulation
							MoverComp->UpdatedCompAsPrimitive->SetSimulatePhysics(true);
							
							// Turn off sleeping
							Chaos::FRigidBodyHandle_External& PhysicsBody = CharacterProxy->GetGameThreadAPI();
							PhysicsBody.SetSleepType(Chaos::ESleepType::NeverSleep);
						}
					}
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::DestroyConstraint()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && HasValidPhysicsState())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				// Note: Proxy gets destroyed when the constraint is deregistered and that deletes the constraint
				Solver->UnregisterObject(Constraint.Release());
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (StateChange == EComponentPhysicsStateChange::Destroyed)
	{
		DestroyConstraint();
	}
	else if (StateChange == EComponentPhysicsStateChange::Created)
	{
		SetupConstraint();

		if (MoverComp && MoverComp->ModeFSM)
		{
			MoverComp->ModeFSM->SetModeImmediately(MoverComp->StartingMovementMode);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (ensureAlwaysMsgf(MoverComp, TEXT("UMoverNetworkPhysicsLiaisonComponent on actor %s failed to find associated Mover component. This actor's movement will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		MoverComp->InitMoverSimulation();
		MoverComp->ModeFSM->SetModeImmediately(MoverComp->StartingMovementMode);
	}

	// Register network data for recording and rewind/resim
	if (NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->CreateDataHistory<FNetworkPhysicsMoverTraits>(this);
	}
}

void UMoverNetworkPhysicsLiaisonComponent::UninitializeComponent()
{
	if (NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->RemoveDataHistory();
	}

	Super::UninitializeComponent();
}

bool UMoverNetworkPhysicsLiaisonComponent::ShouldCreatePhysicsState() const
{
	if (!IsRegistered() || IsBeingDestroyed())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World->IsGameWorld())
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();

		if (PhysScene && CanCreatePhysics())
		{
			return true;
		}
	}

	return false;
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidPhysicsState() const
{
	return Constraint.IsValid() && Constraint->IsValid();
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidState() const
{
	return HasValidPhysicsState() && MoverComp && MoverComp->UpdatedCompAsPrimitive && MoverComp->UpdatedComponent
		&& MoverComp->ModeFSM->IsValidLowLevelFast() && MoverComp->SimBlackboard->IsValidLowLevelFast()
		&& MoverComp->InputProducer && MoverComp->MovementMixer;
}

void UMoverNetworkPhysicsLiaisonComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	SetupConstraint();
}

void UMoverNetworkPhysicsLiaisonComponent::OnDestroyPhysicsState()
{
	DestroyConstraint();
	
	Super::OnDestroyPhysicsState();
}

bool UMoverNetworkPhysicsLiaisonComponent::CanCreatePhysics() const
{
	check(GetOwner());
	FString ActorName = GetOwner()->GetName();

	if (!IsValid(MoverComp->UpdatedComponent))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not set."), *ActorName, *GetPathName());
		return false;
	}

	if (!IsValid(MoverComp->UpdatedCompAsPrimitive))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not a PrimitiveComponent."), *ActorName, *GetPathName());
		return false;
	}

	return true;
}

void UMoverNetworkPhysicsLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register with the physics mover manager
	if (UWorld* World = GetWorld())
	{
		if (UPhysicsMoverManager* Manager = World->GetSubsystem<UPhysicsMoverManager>())
		{
			Manager->RegisterPhysicsMoverComponent(this);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister with the physics mover manager
	if (UWorld* World = GetWorld())
	{
		if (UPhysicsMoverManager* Manager = World->GetSubsystem<UPhysicsMoverManager>())
		{
			Manager->UnregisterPhysicsMoverComponent(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////

Chaos::FUniqueIdx UMoverNetworkPhysicsLiaisonComponent::GetUniqueIdx() const
{
	if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
	{
		if (FBodyInstance* BI = MoverComp->UpdatedCompAsPrimitive->GetBodyInstance())
		{
			if (FPhysicsActorHandle ActorHandle = BI->ActorHandle)
			{
				return ActorHandle->GetGameThreadAPI().UniqueIdx();
			}
		}
	}

	return Chaos::FUniqueIdx();
}

void UMoverNetworkPhysicsLiaisonComponent::UpdateConstraintSettings()
{
	if (HasValidState())
	{
		Constraint->SetVerticalAxis(MoverComp->GetUpDirection());
		Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);

		const UBaseMovementMode* CurrentMode = MoverComp->ModeFSM->GetCurrentMode();
		if (CurrentMode && CurrentMode->Implements<UPhysicsCharacterMovementModeInterface>())
		{
			const IPhysicsCharacterMovementModeInterface* PhysicsMode = CastChecked<IPhysicsCharacterMovementModeInterface>(CurrentMode);
			PhysicsMode->UpdateConstraintSettings(*Constraint);
		}
	}
}

int32 UMoverNetworkPhysicsLiaisonComponent::GetNetworkPhysicsTickOffset() const
{
	if (NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
	{
		const APlayerController* PlayerController = NetworkPhysicsComponent->GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}

		if (PlayerController)
		{
			return PlayerController->GetNetworkPhysicsTickOffset();
		}
	}

	return 0;
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponent::GetCurrentAsyncMoverTimeStep_Internal() const
{
	Chaos::EnsureIsInPhysicsThreadContext();
	ensure(bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;

	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			TimeStep.ServerFrame = Solver->GetCurrentFrame() + GetNetworkPhysicsTickOffset();
			TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
			TimeStep.BaseSimTimeMs = TimeStep.ServerFrame * TimeStep.StepMs;
			TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
		}
	}

	return TimeStep;
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponent::GetCurrentAsyncMoverTimeStep_External() const
{
	Chaos::EnsureIsInGameThreadContext();
	ensure(bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;

	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			const int32 Offset = GetNetworkPhysicsTickOffset();
			TimeStep.ServerFrame = Solver->GetCurrentFrame() + Offset;
			TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
			TimeStep.BaseSimTimeMs = Solver->GetPhysicsResultsTime_External() * 1000.0f + Offset * TimeStep.StepMs;
			TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
		}
	}

	return TimeStep;
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponent::GetCurrentMoverTimeStep(float DeltaSeconds) const
{
	ensure(!bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;

	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			TimeStep.ServerFrame = Solver->GetCurrentFrame();
			TimeStep.StepMs = DeltaSeconds * 1000.0f;
			TimeStep.BaseSimTimeMs = Solver->GetSolverTime() * 1000.0f;
			TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
		}
	}

	return TimeStep;
}

void UMoverNetworkPhysicsLiaisonComponent::ProduceInput_External(float DeltaSeconds, OUT FPhysicsMoverAsyncInput& Input)
{
	Chaos::EnsureIsInGameThreadContext();

	if (HasValidState())
	{
		Input.MoverIdx = GetUniqueIdx();
		Input.MoverSimulation = this;

		// Produce input
		if (!MoverComp)
		{
			return;
		}

		// If in a networked game only produce input for the locally controlled character
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		bool bProduceInput = PawnOwner ? PawnOwner->IsLocallyControlled() : false;

		if (bProduceInput)
		{
			if (!bCachedInputIsValid)
			{
				const int DeltaTimeMS = FMath::RoundToInt(DeltaSeconds * 1000.0f);
				MoverComp->ProduceInput(DeltaTimeMS, &Input.InputCmd);

				// We only want to consume one input per physics frame
				// so if there is already a valid cached input we use that.
				// Input is set invalid when the async output is consumed
				bCachedInputIsValid = MoverComp->CachedLastProducedInputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>() != nullptr;
			}
			else
			{
				Input.InputCmd = MoverComp->CachedLastProducedInputCmd;
			}
		}

		Input.SyncState.SyncStateCollection.Empty();
		if (MoverComp->bHasValidCachedState)
		{
			Input.SyncState = MoverComp->CachedLastSyncState;
		}
		else
		{
			Input.SyncState.MovementMode = MoverComp->StartingMovementMode;
		}

		const FMoverTimeStep MoverTimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_External() : GetCurrentMoverTimeStep(DeltaSeconds);
		MoverComp->CachedLastSimTickTimeStep = MoverTimeStep;

		if (bCachedInputIsValid)
		{
			MoverComp->OnPreSimulationTick.Broadcast(MoverTimeStep, Input.InputCmd);
		}
		else
		{
			MoverComp->OnPreSimulationTick.Broadcast(MoverTimeStep, NetInputCmd);
		}

		// This is required so that the physics thread can have a copy of the data to access
		NetInputCmd = Input.InputCmd;
		NetSyncState = Input.SyncState;
	}
}

void UMoverNetworkPhysicsLiaisonComponent::ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds)
{
	Chaos::EnsureIsInGameThreadContext();

	if (Output.bIsValid && MoverComp)
	{
		const float DeltaSeconds = MoverComp->CachedLastSimTickTimeStep.StepMs * 0.001f;
		const FMoverTimeStep MoverTimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_External() : GetCurrentMoverTimeStep(DeltaSeconds);

		if (bUsingAsyncPhysics)
		{
			if (bCachedLastPhysicsSyncStateIsValid)
			{
				const double PhysicsResultsTime = GetWorld()->GetPhysicsScene()->GetSolver()->GetPhysicsResultsTime_External();

				if (PhysicsResultsTime < OutputTimeInSeconds)
				{

					// Output is in the future. Interpolate from the cached previous state to the next state
					const float Denom = (OutputTimeInSeconds - CachedLastPhysicsSyncStateOutputTime);
					if (Denom > 0)
					{
						const float Alpha = FMath::Clamp((PhysicsResultsTime - CachedLastPhysicsSyncStateOutputTime) / Denom, 0.0f, 1.0f);
						FMoverSyncState InterpolatedSyncState;
						InterpolatedSyncState.Interpolate(&CachedLastPhysicsSyncState, &Output.SyncState, Alpha);

						// The physics mover does not set the sync state transform, it uses the physics transform,
						// so get the world transform from the physics particle.
						if (const Chaos::FSingleParticlePhysicsProxy* CharacterProxy = Constraint->GetCharacterParticleProxy())
						{
							const Chaos::FRigidBodyHandle_External& Body = CharacterProxy->GetGameThreadAPI();
							if (FMoverDefaultSyncState* SyncState = InterpolatedSyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>())
							{
								SyncState->SetTransforms_WorldSpace(Body.X(), FRotator(Body.R()), Body.V());
							}
						}

						// If landed, broadcast OnLanded
						// TODO: Generalize handling of events generated on physics thread
						if (MoverComp->HasValidCachedState())
						{
							if ((MoverComp->GetSyncState().MovementMode == DefaultModeNames::Falling) && (InterpolatedSyncState.MovementMode == DefaultModeNames::Walking))
							{
								if (UFallingMode* FallingMode = MoverComp->FindMode_Mutable<UFallingMode>())
								{
									FHitResult HitResult;
									MoverComp->TryGetFloorCheckHitResult(HitResult);
									FallingMode->OnLanded.Broadcast(InterpolatedSyncState.MovementMode, HitResult);
								}
							}
						}

						// TODO: Consider moving to a util function
						MoverComp->CachedLastSyncState = InterpolatedSyncState;
						MoverComp->CachedLastSimTickTimeStep = MoverTimeStep;
						MoverComp->bHasValidCachedState = true;
					}

					// Call on the last output for the frame
					UpdateConstraintSettings();
					MoverComp->OnPostSimulationTick.Broadcast(MoverTimeStep);
					bCachedInputIsValid = false;
				}
			}

			CachedLastPhysicsSyncState = Output.SyncState;
			CachedLastPhysicsSyncStateOutputTime = OutputTimeInSeconds;
			bCachedLastPhysicsSyncStateIsValid = true;
		}
		else
		{
			MoverComp->CachedLastSyncState = Output.SyncState;
			MoverComp->CachedLastSimTickTimeStep = MoverTimeStep;
			MoverComp->bHasValidCachedState = true;

			// Call on the last output for the frame
			UpdateConstraintSettings();
			MoverComp->OnPostSimulationTick.Broadcast(MoverTimeStep);
			bCachedInputIsValid = false;
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::ProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// Override input data unless player is local client
	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (NetworkPhysicsComponent)
		{
			if (NetworkPhysicsComponent->HasServerWorld())
			{
				// Server remote player
				GetCurrentInputData(Input.InputCmd);
			}
			else
			{
				GetCurrentStateData(Input.SyncState);

				bool bIsSolverResim = false;
				bool bIsFirstResimFrame = false;
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					bIsSolverResim = Solver->GetEvolution()->IsResimming();
					bIsFirstResimFrame = Solver->GetEvolution()->IsResetting();
				}

				bool bLocalPlayer = false;
				APlayerController* PlayerController = NetworkPhysicsComponent->GetPlayerController();
				if (PlayerController && PlayerController->IsLocalController())
				{
					bLocalPlayer = true;
				}

				if (!bLocalPlayer || bIsSolverResim)
				{
					ensureMsgf(bUsingAsyncPhysics, TEXT("Physics mover requires async physics to work in multiplayer."));
					GetCurrentInputData(Input.InputCmd);
				}

				// Rollback mover state if on the first resimulation frame
				if (bLocalPlayer && bIsSolverResim && bIsFirstResimFrame)
				{
					FMoverAuxStateContext UnusedAuxState;
					MoverComp->OnSimulationRollback(&Input.SyncState, &UnusedAuxState);
				}
			}
		}
	}

	// Override common settings data with data from FMovementSettingsInputs if present in the input cmd
	if (const FMovementSettingsInputs* MovementSettings = Input.InputCmd.InputCollection.FindDataByType<FMovementSettingsInputs>())
	{
		CommonMovementSettings->MaxSpeed = MovementSettings->MaxSpeed;
		CommonMovementSettings->Acceleration = MovementSettings->Acceleration;
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& Output) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// Sync state should carry over to the next sim frame by default unless something modifies it
	Output.SyncState = Input.SyncState;

	if (!HasValidState() || !Input.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		return;
	}

	// Exit if Physics is not enabled on the CollisionShape
	const UShapeComponent* CollisionShape = Cast<const UShapeComponent>(MoverComp->UpdatedComponent);
	if (!CollisionShape || !CollisionShape->IsSimulatingPhysics())
	{
		return;
	}
	
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
	if (!ConstraintHandle || !ConstraintHandle->IsEnabled() || !ConstraintHandle->GetCharacterParticle())
	{
		return;
	}

	// Update sync state from physics
	Chaos::FPBDRigidParticleHandle* CharacterParticle = ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	if (!CharacterParticle || CharacterParticle->Disabled())
	{
		return;
	}

	// Check that the input movement mode is valid
	if (MoverComp->MovementModes.Contains(Input.SyncState.MovementMode))
	{
		const IPhysicsCharacterMovementModeInterface* PhysicsMode = Cast<const IPhysicsCharacterMovementModeInterface>(MoverComp->MovementModes[Input.SyncState.MovementMode]);
		if (!PhysicsMode)
		{
			UE_LOG(LogMover, Verbose, TEXT("Attempting to run non-physics movement mode %s in physics mover update."), *Input.SyncState.MovementMode.ToString());
			return;
		}
	}
	else
	{
		return;
	}

	// Make the sync state velocity relative to the ground if walking
	FVector LocalGroundVelocity = FVector::ZeroVector;
	if (Input.SyncState.MovementMode == DefaultModeNames::Walking)
	{
		if (const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard())
		{
			FFloorCheckResult LastFloorResult;
			if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult))
			{
				LocalGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(CharacterParticle->GetX(), LastFloorResult.HitResult, TickParams.DeltaTimeSeconds);
				LocalGroundVelocity -= LocalGroundVelocity.ProjectOnToNormal(LastFloorResult.HitResult.ImpactNormal);
			}
		}
	}

	FMoverDefaultSyncState& SyncState = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	SyncState.SetTransforms_WorldSpace(CharacterParticle->GetX(), FRotator(CharacterParticle->GetR()), CharacterParticle->GetV() - LocalGroundVelocity);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Update the simulation

	FMoverTickStartData TickStartData(Input.InputCmd, Input.SyncState, FMoverAuxStateContext());
	FMoverTickEndData TickEndData;

	// Sync state should carry over to the next sim frame by default unless something modifies it
	TickEndData.SyncState = TickStartData.SyncState;

	const FMoverDefaultSyncState* StartingSyncState = TickStartData.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FCharacterDefaultInputs* InputCmd = TickStartData.InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();
	check(InputCmd);

	const FMoverTimeStep TimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_Internal() : GetCurrentMoverTimeStep(TickParams.DeltaTimeSeconds);

	// Update movement state machine

	if (MoverComp->bHasRolledBack)
	{
		MoverComp->ProcessFirstSimTickAfterRollback(TimeStep);
	}

	if (InputCmd && !InputCmd->SuggestedMovementMode.IsNone())
	{
		MoverComp->QueueNextMode(InputCmd->SuggestedMovementMode);
	}

	// Tick the actual simulation. This is where the proposed moves are queried and executed, affecting change to the moving actor's gameplay state and captured in the output sim state
	MoverComp->ModeFSM->OnSimulationTick(MoverComp->UpdatedComponent, MoverComp->UpdatedCompAsPrimitive, MoverComp->SimBlackboard.Get(), TickStartData, TimeStep, TickEndData);

	// Set the output sync state and fill in the movement mode
	Output.SyncState = TickEndData.SyncState;
	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const FName MovementModeAfterTick = MoverComp->ModeFSM->GetCurrentModeName();
	Output.SyncState.MovementMode = MovementModeAfterTick;

	MoverComp->SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, Output.FloorResult);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Update physics constraint from output sync state

	FVector TargetDeltaPos = OutputSyncState.GetLocation_WorldSpace() - CharacterParticle->GetX();

	if (TargetDeltaPos.SizeSquared2D() > GPhysicsDrivenMotionDebugParams.TeleportThreshold * GPhysicsDrivenMotionDebugParams.TeleportThreshold)
	{
		TeleportParticle(CharacterParticle, OutputSyncState.GetLocation_WorldSpace(), OutputSyncState.GetOrientation_WorldSpace().Quaternion());
	}

	// Add back the ground velocity that was subtracted to but the movement velocity in local space
	FVector TargetVelocity = OutputSyncState.GetVelocity_WorldSpace() + LocalGroundVelocity;

	// Landed so add the new ground velocity
	if ((Output.SyncState.MovementMode == DefaultModeNames::Walking) && (Input.SyncState.MovementMode != DefaultModeNames::Walking))
	{
		if (const UPhysicsDrivenWalkingMode* WalkingMode = Cast<UPhysicsDrivenWalkingMode>(MoverComp->FindMovementMode(UPhysicsDrivenWalkingMode::StaticClass())))
		{
			LocalGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(CharacterParticle->GetX(), Output.FloorResult.HitResult, TickParams.DeltaTimeSeconds);
			LocalGroundVelocity -= LocalGroundVelocity.ProjectOnToNormal(Output.FloorResult.HitResult.ImpactNormal);
			TargetVelocity += WalkingMode->FractionalVelocityToTarget * LocalGroundVelocity;
		}
	}

	CharacterParticle->SetV(TargetVelocity);

	// Note: Output sync state does not have a target angular velocity so
	// use the target orientation
	FRotator DeltaRotation = OutputSyncState.GetOrientation_WorldSpace() - FRotator(CharacterParticle->GetR());
	FRotator Winding, Remainder;
	DeltaRotation.GetWindingAndRemainder(Winding, Remainder);
	float TargetDeltaFacing = FMath::DegreesToRadians(Remainder.Yaw);
	if (TickParams.DeltaTimeSeconds > UE_SMALL_NUMBER)
	{
		CharacterParticle->SetW((TargetDeltaFacing / TickParams.DeltaTimeSeconds) * Chaos::FVec3::ZAxisVector);
	}

	// Update the constraint data based on the floor result
	if (Output.FloorResult.bBlockingHit)
	{
		// Set the ground particle on the constraint
		Chaos::FGeometryParticleHandle* GroundParticle = nullptr;

		if (IPhysicsComponent* PhysicsComp = Cast<IPhysicsComponent>(Output.FloorResult.HitResult.Component))
		{
			if (Chaos::FPhysicsObjectHandle PhysicsObject = PhysicsComp->GetPhysicsObjectById(Output.FloorResult.HitResult.Item))
			{
				Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
				GroundParticle = Interface.GetParticle(PhysicsObject);

				// Wake the ground particle if it is sleeping
				WakeParticleIfSleeping(GroundParticle);
			}
		}
		ConstraintHandle->SetGroundParticle(GroundParticle);

		// Set the max walkable slope angle using any override from the hit component
		float WalkableSlopeCosine = ConstraintHandle->GetSettings().CosMaxWalkableSlopeAngle;
		if (Output.FloorResult.HitResult.Component != nullptr)
		{
			const FWalkableSlopeOverride& SlopeOverride = Output.FloorResult.HitResult.Component->GetWalkableSlopeOverride();
			WalkableSlopeCosine = SlopeOverride.ModifyWalkableFloorZ(WalkableSlopeCosine);
		}

		if (!Output.FloorResult.bWalkableFloor)
		{
			WalkableSlopeCosine = 2.0f;
		}

		ConstraintHandle->SetData({
			Output.FloorResult.HitResult.ImpactNormal,
			TargetDeltaPos,
			TargetDeltaFacing,
			Output.FloorResult.FloorDist,
			WalkableSlopeCosine
			});
	}
	else
	{
		ConstraintHandle->SetData({
			Chaos::FVec3::ZAxisVector,
			Chaos::FVec3::ZeroVector,
			0.0,
			1.0e10,
			0.5f
			});
	}

	// Physics can tick multiple times using the same input data from the game thread
	// so make sure to update it here using the results of this update
	Input.SyncState = Output.SyncState;
	InputCmd->SuggestedMovementMode = NAME_None; // Should have already been set this frame

	Output.bIsValid = true;
}

void UMoverNetworkPhysicsLiaisonComponent::TeleportParticle(Chaos::FGeometryParticleHandle* Particle, const FVector& Position, const FQuat& Rotation) const
{
	if (!Particle)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				if (Solver->GetEvolution())
				{
					Solver->GetEvolution()->SetParticleTransform(Particle, Position, Rotation, true);
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::WakeParticleIfSleeping(Chaos::FGeometryParticleHandle* Particle) const
{
	if (Particle)
	{
		Chaos::FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle();
		if (Rigid && (Rigid->ObjectState() == Chaos::EObjectStateType::Sleeping))
		{
			if (UWorld* World = GetWorld())
			{
				if (FPhysScene* Scene = World->GetPhysicsScene())
				{
					if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
					{
						if (Solver->GetEvolution())
						{
							Solver->GetEvolution()->SetParticleObjectState(Rigid, Chaos::EObjectStateType::Dynamic);
						}
					}
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (!HasValidState())
	{
		return;
	}

	if (MoverComp->MovementModes.Contains(Input.SyncState.MovementMode))
	{
		if (const IPhysicsCharacterMovementModeInterface* PhysicsMode = Cast<const IPhysicsCharacterMovementModeInterface>(MoverComp->MovementModes[Input.SyncState.MovementMode]))
		{
			Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
			if (!ConstraintHandle || !ConstraintHandle->IsEnabled() || !ConstraintHandle->GetCharacterParticle())
			{
				return;
			}

			const FPhysicsMoverSimulationContactModifierParams Params { ConstraintHandle, MoverComp->UpdatedCompAsPrimitive };
			PhysicsMode->OnContactModification_Internal(Params, Modifier);
		}
	}
}
