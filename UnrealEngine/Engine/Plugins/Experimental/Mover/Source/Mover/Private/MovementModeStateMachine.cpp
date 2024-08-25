// Copyright Epic Games, Inc. All Rights Reserved.


#include "MovementModeStateMachine.h"
#include "MoverDeveloperSettings.h"
#include "MoverLog.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoveLibrary/MovementMixer.h"
#include "Templates/SubclassOf.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MovementModeStateMachine)

const FName UNullMovementMode::NullModeName(TEXT("Null"));

UNullMovementMode::UNullMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNullMovementMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
}



UMovementModeStateMachine::UMovementModeStateMachine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UMovementModeStateMachine::RegisterMovementMode(FName ModeName, TObjectPtr<UBaseMovementMode> Mode, bool bIsDefaultMode)
{
	// JAH TODO: add validation and warnings for overwriting modes
	// JAH TODO: add validation of Mode

	Modes.Add(ModeName, Mode);

	if (bIsDefaultMode)
	{
		//JAH TODO: add validation that we are only overriding the default null mode
		DefaultModeName = ModeName;
	}

	Mode->DoRegister(ModeName);
}


void UMovementModeStateMachine::RegisterMovementMode(FName ModeName, TSubclassOf<UBaseMovementMode> ModeType, bool bIsDefaultMode)
{
	
	RegisterMovementMode(ModeName, NewObject<UBaseMovementMode>(GetOuter(), ModeType), bIsDefaultMode);

}



void UMovementModeStateMachine::UnregisterMovementMode(FName ModeName)
{
	TObjectPtr<UBaseMovementMode> ModeToUnregister = Modes.FindAndRemoveChecked(ModeName);

	if (ModeToUnregister)
	{
		ModeToUnregister->DoUnregister();
	}
}

void UMovementModeStateMachine::ClearAllMovementModes()
{
	for (TPair<FName, TObjectPtr<UBaseMovementMode>>& Element : Modes)
	{
		Element.Value->DoUnregister();
	}

	Modes.Empty();

	ConstructDefaultModes();	// Note that we're resetting to our defaults so we keep the null movement mode
}

void UMovementModeStateMachine::SetDefaultMode(FName NewDefaultModeName)
{
	Modes.FindChecked(NewDefaultModeName);

	DefaultModeName = NewDefaultModeName;
}


void UMovementModeStateMachine::QueueNextMode(FName DesiredNextModeName, bool bShouldReenter)
{
	if (DesiredNextModeName != NAME_None)
	{ 
		const FName NextModeName          = QueuedModeTransition->GetNextModeName();
		const bool bShouldNextModeReenter = QueuedModeTransition->ShoulReenter();

		if ((NextModeName != NAME_None) &&
		    (NextModeName != DesiredNextModeName || bShouldReenter != bShouldNextModeReenter))
		{
			const AActor* OwnerActor = GetOwnerActor();
			UE_LOG(LogMover, Log, TEXT("%s (%s) Overwriting of queued mode change (%s, reenter: %i) with (%s, reenter: %i)"), *GetNameSafe(OwnerActor), *UEnum::GetValueAsString(OwnerActor->GetLocalRole()), *NextModeName.ToString(), bShouldNextModeReenter, *DesiredNextModeName.ToString(), bShouldReenter);
		}

		if (Modes.Contains(DesiredNextModeName))
		{
			QueuedModeTransition->SetNextMode(DesiredNextModeName, bShouldReenter);
		}
		else
		{
			UE_LOG(LogMover, Warning, TEXT("Attempted to queue an unregistered movement mode: %s"), *DesiredNextModeName.ToString());
		}
	}
}


void UMovementModeStateMachine::SetModeImmediately(FName DesiredModeName, bool bShouldReenter)
{
	QueueNextMode(DesiredModeName, bShouldReenter);
	AdvanceToNextMode();
}


void UMovementModeStateMachine::ClearQueuedMode()
{
	QueuedModeTransition->Clear();
}


void UMovementModeStateMachine::OnSimulationTick(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverBlackboard* SimBlackboard, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FMoverTickEndData& OutputState)
{
	const float EndingSimTimeMs = TimeStep.BaseSimTimeMs + TimeStep.StepMs;

	//GEngine->AddOnScreenDebugMessage(-1, -0.1f, FColor::White, FString::Printf(TEXT("Mode Tick: %s  Queued: %s"), *CurrentModeName.ToString(), *NextModeName.ToString()));

	FMoverTimeStep SubTimeStep = TimeStep;
	FMoverTickStartData SubstepStartData = StartState;
	FMoverDefaultSyncState* SubstepStartSyncState = SubstepStartData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();

	UMoverComponent* MoverComp = CastChecked<UMoverComponent>(GetOuter());
	check(MoverComp->MovementMixer);

	if (!QueuedModeTransition->IsSet())
	{
		QueueNextMode(SubstepStartData.SyncState.MovementMode);
	}

	AdvanceToNextMode();

	const int32 MaxConsecutiveFullRefundedSubsteps = GetDefault<UMoverDeveloperSettings>()->MaxTimesToRefundSubstep;
	int32 NumConsecutiveFullRefundedSubsteps = 0;
	while (SubTimeStep.BaseSimTimeMs < EndingSimTimeMs)
	{
		SubstepStartSyncState = SubstepStartData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
		FMoverDefaultSyncState* OutputSyncState = &OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		OutputState.SyncState.MovementMode = CurrentModeName;

		OutputState.MovementEndState.ResetToDefaults();

		SubTimeStep.StepMs = EndingSimTimeMs - SubTimeStep.BaseSimTimeMs;		// TODO: convert this to an overridable function that can support MaxStepTime, MaxIterations, etc.

		// Transfer any queued moves into the starting state. They'll be started during the move generation.
		FlushQueuedMovesToGroup(SubstepStartData.SyncState.LayeredMoves);
		OutputState.SyncState.LayeredMoves = SubstepStartData.SyncState.LayeredMoves;
		FLayeredMoveGroup& CurrentLayeredMoves = OutputState.SyncState.LayeredMoves;

		// Gather any layered move contributions
		FProposedMove CombinedLayeredMove;
		CombinedLayeredMove.MixMode = EMoveMixMode::AdditiveVelocity;
		bool bHasLayeredMoveContributions = false;
		MoverComp->MovementMixer->ResetMixerState();
		
		TArray<TSharedPtr<FLayeredMoveBase>> ActiveMoves = CurrentLayeredMoves.GenerateActiveMoves(SubTimeStep, MoverComp, SimBlackboard);

		// Tick and accumulate all active moves
		// Gather all proposed moves and distill this into a cumulative movement report. May include separate additive vs override moves.
		// TODO: may want to sort by priority or other factors
		for (TSharedPtr<FLayeredMoveBase>& ActiveMove : ActiveMoves)
		{
			FProposedMove MoveStep;
			if (ActiveMove->GenerateMove(StartState, TimeStep, MoverComp, SimBlackboard, MoveStep))
			{
				bHasLayeredMoveContributions = true;
				MoverComp->MovementMixer->MixLayeredMove(*ActiveMove, MoveStep, CombinedLayeredMove);
			}
		}

		if (bHasLayeredMoveContributions && !CombinedLayeredMove.PreferredMode.IsNone())
		{
			SetModeImmediately(CombinedLayeredMove.PreferredMode);
			OutputState.SyncState.MovementMode = CurrentModeName;
		}

		// Merge proposed movement from the current mode with movement from layered moves
		if (!CurrentModeName.IsNone() && Modes.Contains(CurrentModeName))
		{
			UBaseMovementMode* CurrentMode = Modes[CurrentModeName];
			FProposedMove CombinedMove;
			CurrentMode->DoGenerateMove(SubstepStartData, SubTimeStep, OUT CombinedMove);

			if (bHasLayeredMoveContributions)
			{
				MoverComp->MovementMixer->MixProposedMoves(CombinedLayeredMove, CombinedMove);
			}

			// Apply any layered move finish velocity settings
			if (CurrentLayeredMoves.bApplyResidualVelocity)
			{
				CombinedMove.LinearVelocity = CurrentLayeredMoves.ResidualVelocity;
			}
			if (CurrentLayeredMoves.ResidualClamping >= 0.0f)
			{
				CombinedMove.LinearVelocity = CombinedMove.LinearVelocity.GetClampedToMaxSize2D(CurrentLayeredMoves.ResidualClamping);
				CombinedMove.LinearVelocity.Z = FMath::Min<FVector::FReal>(CombinedMove.LinearVelocity.Z, CurrentLayeredMoves.ResidualClamping);
			}
			CurrentLayeredMoves.ResetResidualVelocity();

			// Execute the combined proposed move
			{
				FSimulationTickParams SimTickParams;
				SimTickParams.StartState = SubstepStartData;
				SimTickParams.UpdatedComponent = UpdatedComponent;
				SimTickParams.UpdatedPrimitive = UpdatedPrimitive;
				SimTickParams.MoverComponent = MoverComp;
				SimTickParams.TimeStep = SubTimeStep;
				SimTickParams.ProposedMove = CombinedMove;

				// Check for any transitions, first those registered with the current movement mode, then global ones that could occur from any mode
				FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
				TObjectPtr<UBaseMovementModeTransition> TransitionToTrigger;

				for (UBaseMovementModeTransition* Transition : CurrentMode->Transitions)
				{
					if (IsValid(Transition))
					{
						EvalResult = Transition->DoEvaluate(SimTickParams);

						if (!EvalResult.NextMode.IsNone())
						{
							TransitionToTrigger = Transition;
							break;
						}
					}
				}

				if (EvalResult.NextMode.IsNone())
				{
					for (UBaseMovementModeTransition* Transition : MoverComp->Transitions)
					{
						if (IsValid(Transition))
						{
							EvalResult = Transition->DoEvaluate(SimTickParams);

							if (!EvalResult.NextMode.IsNone())
							{
								TransitionToTrigger = Transition;
								break;
							}
						}
					}
				}

				if (!EvalResult.NextMode.IsNone())
				{
					OutputState.MovementEndState.NextModeName = EvalResult.NextMode;
					OutputState.MovementEndState.RemainingMs = SimTickParams.TimeStep.StepMs; 	// Pass all remaining time to next mode
					TransitionToTrigger->DoTrigger(SimTickParams);
				}
				else
				{
					CurrentMode->DoSimulationTick(SimTickParams, OutputState);
				}
			}

			QueueNextMode(OutputState.MovementEndState.NextModeName);

			// Check if all of the time for this Substep was refunded
			if (FMath::IsNearlyEqual(SubTimeStep.StepMs, OutputState.MovementEndState.RemainingMs, UE_KINDA_SMALL_NUMBER))
			{
				NumConsecutiveFullRefundedSubsteps++;
				// if we've done this sub step a lot before go ahead and just advance time to avoid freezing editor
				if (NumConsecutiveFullRefundedSubsteps >= MaxConsecutiveFullRefundedSubsteps)
				{
					UE_LOG(LogMover, Warning, TEXT("Movement mode %s and %s on %s are stuck giving time back to eachother - Advancing substep."), *CurrentModeName.ToString(), *OutputState.MovementEndState.NextModeName.ToString(), *MoverComp->GetOwner()->GetName());
					SubTimeStep.BaseSimTimeMs += SubTimeStep.StepMs;
				}
			}
			else
			{
				NumConsecutiveFullRefundedSubsteps = 0;
			}

			//GEngine->AddOnScreenDebugMessage(-1, -0.1f, FColor::White, FString::Printf(TEXT("NextModeName: %s  Queued: %s"), *Output.MovementEndState.NextModeName.ToString(), *NextModeName.ToString()));
		}

		// Switch modes if necessary (note that this will allow exit/enter on the same state)
		AdvanceToNextMode();
		OutputState.SyncState.MovementMode = CurrentModeName;

		const float RemainingMs = FMath::Clamp(OutputState.MovementEndState.RemainingMs, 0.0f, SubTimeStep.StepMs);
		SubTimeStep.BaseSimTimeMs += (SubTimeStep.StepMs - RemainingMs);
		SubTimeStep.StepMs = EndingSimTimeMs - SubTimeStep.BaseSimTimeMs;

		SubstepStartData.SyncState = OutputState.SyncState;
		SubstepStartData.AuxState  = OutputState.AuxState;
	}

}


void UMovementModeStateMachine::OnSimulationRollback(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	ClearQueuedMode();
	QueuedLayeredMoves.Empty();
}


const UBaseMovementMode* UMovementModeStateMachine::GetCurrentMode() const
{
	if (CurrentModeName != NAME_None && Modes.Contains(CurrentModeName))
	{
		return Modes[CurrentModeName];
	}

	return nullptr;
}

const UBaseMovementMode* UMovementModeStateMachine::FindMovementMode(FName ModeName) const
{
	if (ModeName != NAME_None && Modes.Contains(ModeName))
	{
		return Modes[ModeName];
	}

	return nullptr;
}

void UMovementModeStateMachine::QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move)
{
	QueuedLayeredMoves.Add(Move);
}


void UMovementModeStateMachine::ConstructDefaultModes()
{
	RegisterMovementMode(UNullMovementMode::NullModeName, UNullMovementMode::StaticClass(), true);

	DefaultModeName = NAME_None;
	CurrentModeName = UNullMovementMode::NullModeName;

	ClearQueuedMode();
}

void UMovementModeStateMachine::AdvanceToNextMode()
{
	const FName NextModeName = QueuedModeTransition->GetNextModeName();
	const bool bShouldNextModeReenter = QueuedModeTransition->ShoulReenter();

	if ((NextModeName != NAME_None && Modes.Contains(NextModeName)) &&
		(CurrentModeName != NextModeName || bShouldNextModeReenter))
	{
		const AActor* OwnerActor = GetOwnerActor();
		UE_LOG(LogMover, Verbose, TEXT("AdvanceToNextMode: %s (%s) from %s to %s"), 
			*GetNameSafe(OwnerActor), *UEnum::GetValueAsString(OwnerActor->GetLocalRole()), *CurrentModeName.ToString(), *NextModeName.ToString());

		const FName PreviousModeName = CurrentModeName;
		CurrentModeName = NextModeName;

		// signal movement mode change event
		const UMoverComponent* MoverComp = CastChecked<UMoverComponent>(GetOuter());
		MoverComp->OnMovementModeChanged.Broadcast(PreviousModeName, NextModeName);
	}

	ClearQueuedMode();
}

void UMovementModeStateMachine::FlushQueuedMovesToGroup(FLayeredMoveGroup& Group)
{
	if (!QueuedLayeredMoves.IsEmpty())
	{
		for (TSharedPtr<FLayeredMoveBase>& QueuedMove : QueuedLayeredMoves)
		{
			Group.QueueLayeredMove(QueuedMove);
		}
		
		QueuedLayeredMoves.Empty();
	}
}

AActor* UMovementModeStateMachine::GetOwnerActor() const
{
	if (const UActorComponent* OwnerMoverComp = Cast<UActorComponent>(GetOuter()))
	{
		return OwnerMoverComp->GetOwner();
	}

	return nullptr;
}

void UMovementModeStateMachine::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		QueuedModeTransition = NewObject<UImmediateMovementModeTransition>(this, TEXT("QueuedModeTransition"), RF_Transient);
	}
}




UImmediateMovementModeTransition::UImmediateMovementModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Clear();
}

FTransitionEvalResult UImmediateMovementModeTransition::OnEvaluate(const FSimulationTickParams& Params) const
{
	if (NextMode != NAME_None)
	{
		if (bShouldNextModeReenter)
		{
			return FTransitionEvalResult(NextMode);
		}
		else if (NextMode != Params.StartState.SyncState.MovementMode)
		{
			return FTransitionEvalResult(NextMode);
		}
	}

	return FTransitionEvalResult::NoTransition;
}

void UImmediateMovementModeTransition::OnTrigger(const FSimulationTickParams& Params)
{
	Clear();
}

void UImmediateMovementModeTransition::SetNextMode(FName DesiredModeName, bool bShouldReenter)
{
	NextMode = DesiredModeName;
	bShouldNextModeReenter = bShouldReenter;
}

void UImmediateMovementModeTransition::Clear()
{
	NextMode = NAME_None;
	bShouldNextModeReenter = false;
}
