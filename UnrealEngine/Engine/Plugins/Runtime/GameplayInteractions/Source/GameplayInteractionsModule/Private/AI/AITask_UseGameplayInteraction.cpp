// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITask_UseGameplayInteraction.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayInteractionsTypes.h"
#include "AIController.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "Misc/ScopeExit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AITask_UseGameplayInteraction)

UAITask_UseGameplayInteraction::UAITask_UseGameplayInteraction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAITask_UseGameplayInteraction* UAITask_UseGameplayInteraction::UseClaimedGameplayInteractionSmartObject(AAIController* Controller, const FSmartObjectClaimHandle ClaimHandle, const bool bLockAILogic)
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

void UAITask_UseGameplayInteraction::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	
	const bool bKeepTicking = GameplayInteractionContext.Tick(DeltaTime);
	if (!bKeepTicking)
	{
		EndTask();
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
	
	const UGameplayInteractionSmartObjectBehaviorDefinition* SmartObjectGameplayBehaviorDefinition = SmartObjectSubsystem->Use<UGameplayInteractionSmartObjectBehaviorDefinition>(ClaimedHandle);
	if (SmartObjectGameplayBehaviorDefinition == nullptr)		
	{
		UE_VLOG_UELOG(OwnerController, LogGameplayInteractions, Error,
			TEXT("SmartObject was claimed for a different type of behavior definition. Expecting: %s."),
			*UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass()->GetName());
		return;
	}

	const USmartObjectComponent* SmartObjectComponent = SmartObjectSubsystem->GetSmartObjectComponent(ClaimedHandle);
	GameplayInteractionContext.SetContextActor(OwnerController->GetPawn() ? Cast<AActor>(OwnerController->GetPawn()) : OwnerController);
	GameplayInteractionContext.SetSmartObjectActor(SmartObjectComponent ? SmartObjectComponent->GetOwner() : nullptr);
	GameplayInteractionContext.SetClaimedHandle(ClaimedHandle);

	if (!GameplayInteractionContext.Activate(*SmartObjectGameplayBehaviorDefinition))
	{
		return;
	}

	// Register a callback to be notified if the claimed slot became unavailable
	SmartObjectSubsystem->RegisterSlotInvalidationCallback(ClaimedHandle, FOnSlotInvalidated::CreateUObject(this, &UAITask_UseGameplayInteraction::OnSlotInvalidated));

	bSuccess = true;
}

void UAITask_UseGameplayInteraction::OnDestroy(const bool bInOwnerFinished)
{
	GameplayInteractionContext.SetAbortContext(AbortContext);
	GameplayInteractionContext.Deactivate();

	if (ClaimedHandle.IsValid())
	{
		USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(OwnerController->GetWorld());
		check(SmartObjectSubsystem);
		SmartObjectSubsystem->Release(ClaimedHandle);
		ClaimedHandle.Invalidate();
	}

	OnFinished.Broadcast();

	Super::OnDestroy(bInOwnerFinished);
}

void UAITask_UseGameplayInteraction::RequestAbort()
{
	AbortContext.Reason = EGameplayInteractionAbortReason::ExternalAbort;
	EndTask();
}

void UAITask_UseGameplayInteraction::OnSlotInvalidated(const FSmartObjectClaimHandle& ClaimHandle, ESmartObjectSlotState State)
{
	AbortContext.Reason = EGameplayInteractionAbortReason::InternalAbort;
	EndTask();
}

