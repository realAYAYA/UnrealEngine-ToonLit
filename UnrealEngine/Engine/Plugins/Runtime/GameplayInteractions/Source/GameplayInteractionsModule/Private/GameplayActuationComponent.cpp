// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayActuationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VisualLogger/VisualLogger.h"
#include "GameplayTasksComponent.h"
#include "ContextualAnimSceneInstance.h"
#include "GameplayInteractionsTypes.h"
#include "GameplayTaskTransition.h"
#include "GameplayActuationStateProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayActuationComponent)

UGameplayActuationComponent::UGameplayActuationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UGameplayTask* UGameplayActuationComponent::TryMakeTransitionTask(const FConstStructView NextState)
{
	UGameplayTask* Result = nullptr;
	if (bEnableTransitions == false || Transitions.IsEmpty())
	{
		return nullptr;
	}
	UGameplayTasksComponent* TasksComponent = GetOwner()->FindComponentByClass<UGameplayTasksComponent>();
	UCharacterMovementComponent* MovementComponent = GetOwner()->FindComponentByClass<UCharacterMovementComponent>();
	if (TasksComponent == nullptr)
	{
		UE_VLOG(this, LogGameplayTasks, Error, TEXT("Failed to find UGameplayTasksComponent"));
		return nullptr;
	}
	if (MovementComponent == nullptr)
	{
		UE_VLOG(this, LogGameplayTasks, Error, TEXT("Failed to find UCharacterMovementComponent"));
		return nullptr;
	}

	FMakeGameplayTransitionTaskContext TransitionContext;
	TransitionContext.Actor = GetOwner();
	TransitionContext.TasksComponent = TasksComponent;
	TransitionContext.MovementComponent = MovementComponent;
	TransitionContext.ActuationComponent = this;
	TransitionContext.CurrentActuationState = ActuationState;
	TransitionContext.NextActuationState = NextState;
	
	for (const FInstancedStruct& Transition : Transitions)
	{
		if (const FGameplayTransitionDesc* Desc = Transition.GetMutablePtr<FGameplayTransitionDesc>())
		{
			Result = Desc->MakeTransitionTask(TransitionContext);
			if (Result != nullptr)
			{
				break;
			}
		}
	}

	return Result;
}

void UGameplayActuationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	REDIRECT_TO_VLOG(GetOwner());
}

void UGameplayActuationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FGameplayActuationStateBase* State = ActuationState.GetMutablePtr<FGameplayActuationStateBase>())
	{
		State->OnStateDeactivated(FConstStructView());
	}
	ActuationState.Reset();
}

void UGameplayActuationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Poll movement state
	// @todo: this would be better off as a push API, but done like this to support old and new.

	FConstStructView NextState; 
	const UGameplayTasksComponent* TaskComponent = GetOwner()->FindComponentByClass<UGameplayTasksComponent>();
	uint32 NextStateOwnerID = 0;
	
	if (TaskComponent != nullptr)
	{
		auto FindNextState = [TaskComponent, &NextState, &NextStateOwnerID](FConstGameplayTaskIterator It)
		{
			for (; It; ++It)
			{
				UGameplayTask* Task = (*It).Get(); 
				if (const IGameplayActuationStateProvider* StateProvider = Cast<IGameplayActuationStateProvider>(Task))
				{
					NextState = StateProvider->GetActuationState();
					if (NextState.IsValid())
					{
						NextStateOwnerID = TaskComponent->GetUniqueID();
						break;
					}
				}
			}
		};
		
		if (TaskComponent->GetNetMode() == NM_Client)
		{
			FindNextState(TaskComponent->GetSimulatedTaskIterator());
		}
		else
		{
			FindNextState(TaskComponent->GetPriorityQueueIterator());
		}
	}

	if ((NextStateOwnerID != LastStateOwnerID)
		|| (ActuationState.GetScriptStruct() != NextState.GetScriptStruct()))
	{
		// Signal state change
		if (FGameplayActuationStateBase* State = ActuationState.GetMutablePtr<FGameplayActuationStateBase>())
		{
			UE_VLOG(this, LogGameplayTasks, Log, TEXT("OnStateDeactivated on %s"), *ActuationState.GetScriptStruct()->GetName());
			State->OnStateDeactivated(NextState);
		}

		// Take copy of the new state so that both states exists during the transition.
		FInstancedStruct NewState = NextState;
		
		if (FGameplayActuationStateBase* State = NewState.GetMutablePtr<FGameplayActuationStateBase>())
		{
			UE_VLOG(this, LogGameplayTasks, Log, TEXT("OnStateActivated on %s"), *NewState.GetScriptStruct()->GetName());
			State->OnStateActivated(ActuationState);
		}
		
		ActuationState = MoveTemp(NewState);

#if WITH_GAMEPLAYTASK_DEBUG
		StateCounter++;
#endif
	}
	else
	{
		// Update the current state.
		// This is done as by pulling the state (rather than tasks pushing the states)
		// so that we can have mixed gameplay tasks that might not store any state (e.g. tasks for gameplay abilities).
		ActuationState = NextState;
	}

	LastStateOwnerID = NextStateOwnerID;

#if WITH_GAMEPLAYTASK_DEBUG
	// Debug
	const UCharacterMovementComponent* MovementComponent = GetOwner()->FindComponentByClass<UCharacterMovementComponent>();
	if (TaskComponent != nullptr && MovementComponent != nullptr)
	{
		FLinearColor Color1(FColor(64, 192, 64));
		FLinearColor Color2(FColor(64, 64, 192));
		if (TaskComponent->GetNetMode() == NM_Client)
		{
			Color1 *= 5;
			Color2 *= 5;
		}
	
		const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();

		constexpr double LocationUpdateThreshold = 15.0;
		constexpr int32 MaxTrajectorySamples = 300;

		if (DebugTrajectory.IsEmpty() || FVector::Dist2D(DebugTrajectory.Last().Location, CurrentLocation) > LocationUpdateThreshold)
		{
			DebugTrajectory.Emplace(CurrentLocation, ((StateCounter & 1) ? Color1 : Color2).ToFColorSRGB());
		}
		if (DebugTrajectory.Num() > MaxTrajectorySamples)
		{
			DebugTrajectory.RemoveAt(0);
		}

		if (DebugTrajectory.Num() > 1)
		{
			const FVector Offset(0,0,10);
			for (int32 Index = DebugTrajectory.Num() - 1; Index > 0; Index--)
			{
				UE_VLOG_SEGMENT_THICK(TaskComponent, LogGameplayInteractions, Log, Offset + DebugTrajectory[Index].Location, Offset + DebugTrajectory[Index - 1].Location, DebugTrajectory[Index - 1].Color, 4, TEXT_EMPTY);
			}
		}
	}
#endif

}

#if ENABLE_VISUAL_LOG
void UGameplayActuationComponent::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	const FGameplayActuationStateBase* State = ActuationState.GetMutablePtr<FGameplayActuationStateBase>();
	if (State == nullptr)
	{
		return;
	}
	
	FString StateAsText;
	ActuationState.GetScriptStruct()->ExportText(StateAsText, State, /*Default*/ nullptr, /*OwnerObject*/ nullptr, PPF_None, /*ExportRootScope*/nullptr);
	StateAsText.ReplaceInline(TEXT("\","), TEXT("\"\n"));
	StateAsText.ReplaceInline(TEXT("\',"), TEXT("\"\n"));
	
	const FString Desc = FString::Printf(TEXT("%s:\n%s"),*ActuationState.GetScriptStruct()->GetName(), *StateAsText);

	FVisualLogStatusCategory StatusCategory(TEXT("GameplayActuation"));
	StatusCategory.Add(TEXT("State"), Desc);
	Snapshot->Status.Add(StatusCategory);
}
#endif

