// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITask_UseGameplayInteraction.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "AIController.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "Misc/ScopeExit.h"
#include "Tasks/AITask_MoveTo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AITask_UseGameplayInteraction)

//----------------------------------------------------------------------//
// Static Blueprint callable functions
//----------------------------------------------------------------------//
UAITask_UseGameplayInteraction* UAITask_UseGameplayInteraction::UseSmartObjectWithGameplayInteraction(AAIController* Controller, const FSmartObjectClaimHandle ClaimHandle, const bool bLockAILogic)
{
	if (Controller == nullptr)
	{
		UE_LOG(LogSmartObject, Error, TEXT("AI Controller required to use smart object."));
		return nullptr;
	}

	if (!ClaimHandle.IsValid())
	{
		UE_VLOG_UELOG(Controller, LogSmartObject, Error, TEXT("Valid claimed handle required to use smart object."));
		return nullptr;
	}

	UAITask_UseGameplayInteraction* MyTask = NewAITask<UAITask_UseGameplayInteraction>(*Controller, EAITaskPriority::High);
	if (MyTask == nullptr)
	{
		return nullptr;
	}

	MyTask->SetClaimHandle(ClaimHandle);

	if (bLockAILogic)
	{
		MyTask->RequestAILogicLocking();
	}

	return MyTask;
}

UAITask_UseGameplayInteraction* UAITask_UseGameplayInteraction::MoveToAndUseSmartObjectWithGameplayInteraction(AAIController* Controller, const FSmartObjectClaimHandle ClaimHandle, const bool bLockAILogic)
{
	UAITask_UseGameplayInteraction* NewTask = UseSmartObjectWithGameplayInteraction(Controller, ClaimHandle, bLockAILogic);
	if (NewTask != nullptr)
	{
		NewTask->SetShouldReachSlotLocation(true);
	}

	return NewTask;
}

//----------------------------------------------------------------------//
// UAITask_UseGameplayInteraction
//----------------------------------------------------------------------//
UAITask_UseGameplayInteraction::UAITask_UseGameplayInteraction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

void UAITask_UseGameplayInteraction::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (MoveToTask == nullptr)
	{
		const bool bKeepTicking = GameplayInteractionContext.Tick(DeltaTime);
		if (bKeepTicking == false)
		{
			bInteractionCompleted = true;
			EndTask();
		}
	}
}

void UAITask_UseGameplayInteraction::Activate()
{
	Super::Activate();

	bool bSuccess = false;
	ON_SCOPE_EXIT
	{
		if (!bSuccess)
		{
			EndTask();
		}
	};

	if (!ensureMsgf(ClaimedHandle.IsValid(), TEXT("SmartObject handle must be valid at this point.")))
	{
		return;
	}

	USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(OwnerController->GetWorld());
	if (!ensureMsgf(SmartObjectSubsystem != nullptr, TEXT("SmartObjectSubsystem must be accessible at this point.")))
	{
		return;
	}

	// A valid claimed handle can point to an object that is no longer part of the simulation
	if (!SmartObjectSubsystem->IsClaimedSmartObjectValid(ClaimedHandle))
	{
		UE_VLOG(OwnerController, LogSmartObject, Log, TEXT("Claim handle: %s refers to an object that is no longer available."), *LexToString(ClaimedHandle));
		return;
	}

	// Register a callback to be notified if the claimed slot became unavailable
	SmartObjectSubsystem->RegisterSlotInvalidationCallback(ClaimedHandle, FOnSlotInvalidated::CreateUObject(this, &UAITask_UseGameplayInteraction::OnSlotInvalidated));

	if (bShouldUseMoveTo)
	{
		const TOptional<FVector> GoalLocation = SmartObjectSubsystem->GetSlotLocation(ClaimedHandle);
		if (!ensureMsgf(GoalLocation.IsSet(), TEXT("Unable to extract a valid slot location.")))
		{
			return;
		}

		FAIMoveRequest MoveReq(GoalLocation.GetValue());
		MoveReq.SetUsePathfinding(true);
		MoveReq.SetAllowPartialPath(false);

		MoveToTask = NewAITask<UAITask_MoveTo>(*OwnerController, *this, EAITaskPriority::High, TEXT("SmartObject"));
		MoveToTask->SetUp(OwnerController, MoveReq);
		MoveToTask->ReadyForActivation();
		bSuccess = true;
	}
	else
	{
		bSuccess = StartInteraction();
	}
}

bool UAITask_UseGameplayInteraction::StartInteraction()
{
	check(OwnerController);
	const UWorld* World = OwnerController->GetWorld();
	USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(World);
	if (!ensure(SmartObjectSubsystem))
	{
		return false;
	}

	const UGameplayInteractionSmartObjectBehaviorDefinition* SmartObjectGameplayBehaviorDefinition = SmartObjectSubsystem->MarkSlotAsOccupied<UGameplayInteractionSmartObjectBehaviorDefinition>(ClaimedHandle);
	if (SmartObjectGameplayBehaviorDefinition == nullptr)
	{
		UE_VLOG_UELOG(OwnerController, LogGameplayInteractions, Error,
			TEXT("SmartObject was claimed for a different type of behavior definition. Expecting: %s."),
			*UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass()->GetName());
		return false;
	}

	const USmartObjectComponent* SmartObjectComponent = SmartObjectSubsystem->GetSmartObjectComponent(ClaimedHandle);
	GameplayInteractionContext.SetContextActor(OwnerController->GetPawn() ? Cast<AActor>(OwnerController->GetPawn()) : OwnerController);
	GameplayInteractionContext.SetSmartObjectActor(SmartObjectComponent ? SmartObjectComponent->GetOwner() : nullptr);
	GameplayInteractionContext.SetClaimedHandle(ClaimedHandle);

	return GameplayInteractionContext.Activate(*SmartObjectGameplayBehaviorDefinition);
}

void UAITask_UseGameplayInteraction::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	if (MoveToTask == &Task)
	{
		if (MoveToTask->IsFinished())
		{
			if (MoveToTask->WasMoveSuccessful())
			{
				MoveToTask = nullptr;
				if (StartInteraction() == false)
				{
					EndTask();	
				}
			}
			else
			{
				OnMoveToFailed.Broadcast();
				EndTask();
			}
		}
	}

	Super::OnGameplayTaskDeactivated(Task);
}

void UAITask_UseGameplayInteraction::OnDestroy(const bool bInOwnerFinished)
{
	GameplayInteractionContext.Deactivate();

	if (ClaimedHandle.IsValid())
	{
		USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(OwnerController->GetWorld());
		check(SmartObjectSubsystem);
		SmartObjectSubsystem->MarkSlotAsFree(ClaimedHandle);
		SmartObjectSubsystem->UnregisterSlotInvalidationCallback(ClaimedHandle);
		ClaimedHandle.Invalidate();
	}

	if (TaskState != EGameplayTaskState::Finished)
	{
		if (AbortContext.Reason == EGameplayInteractionAbortReason::Unset && bInteractionCompleted)
		{
			OnSucceeded.Broadcast();
		}
		else
		{
			OnFailed.Broadcast();
		}
	}

	OnFinished.Broadcast();

	Super::OnDestroy(bInOwnerFinished);
}

void UAITask_UseGameplayInteraction::OnSlotInvalidated(const FSmartObjectClaimHandle& ClaimHandle, ESmartObjectSlotState State)
{
	Abort(EGameplayInteractionAbortReason::InternalAbort);
}

void UAITask_UseGameplayInteraction::Abort(const EGameplayInteractionAbortReason Reason)
{
	AbortContext.Reason = Reason;
	
	if (MoveToTask != nullptr)
	{
		// clear before triggering 'the end' so that OnGameplayTaskDeactivated
		// ignores the incoming info about task end
		UAITask_MoveTo* Task = MoveToTask;
		MoveToTask = nullptr;
		Task->ExternalCancel();
	}
	else if (!bInteractionCompleted)
	{
		GameplayInteractionContext.SetAbortContext(AbortContext);
	}
	
	EndTask();
}

void UAITask_UseGameplayInteraction::RequestAbort()
{
	Abort(EGameplayInteractionAbortReason::ExternalAbort);
}

