// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_StandAt.h"
#include "TimerManager.h"
#include "AISystem.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Tasks/AITask.h"
#include "GameplayActuationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_StandAt)

UGameplayTask_StandAt::UGameplayTask_StandAt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bIsPausable = true;

	AddRequiredResource<UAIResource_Movement>();
	AddClaimedResource<UAIResource_Movement>();

	ResourceOverlapPolicy = ETaskResourceOverlapPolicy::RequestCancelAndStartAtEnd;

	Result = EGameplayTaskActuationResult::None;
}

UGameplayTask_StandAt* UGameplayTask_StandAt::StandAt(APawn* Pawn, float Duration)
{
	if (Pawn == nullptr)
	{
		return nullptr;
	}

	UGameplayTasksComponent* TaskComponent = Pawn->FindComponentByClass<UGameplayTasksComponent>();
	if (TaskComponent == nullptr)
	{
		UE_VLOG(Pawn, LogGameplayTasks, Error, TEXT("Expecting Pawn to have Gameplay Tasks Component"));
		return nullptr;
	}

	UGameplayTask_StandAt* Task = NewTask<UGameplayTask_StandAt>(*Cast<IGameplayTaskOwnerInterface>(TaskComponent));
	if (Task == nullptr)
	{
		return Task;
	}

	Task->Duration = Duration;

	return Task;
}

void UGameplayTask_StandAt::FinishTask(const EGameplayTaskActuationResult InResult)
{
	Result = InResult;

	if (!bCompleteCalled)
	{
		bCompleteCalled = true;
		
		if (Result == EGameplayTaskActuationResult::RequestFailed)
		{
			OnRequestFailed.Broadcast();
		}
		else
		{
			APawn* Pawn = Cast<APawn>(GetAvatarActor());
			OnCompleted.Broadcast(Result, Pawn);
		}
	}

	// Intentionally not calling EndTask() here, as we expect ExternalCancel() to get called when next task is added.
}

void UGameplayTask_StandAt::ExternalCancel()
{
	EndTask();
}

void UGameplayTask_StandAt::Activate()
{
	Super::Activate();

	APawn* Pawn = Cast<APawn>(GetAvatarActor());
	check(Pawn);

	MovementComponent = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if (MovementComponent == nullptr)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Failed to find movement component"));
		return;
	}

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentForward = MovementComponent->GetActorTransform().GetRotation().GetForwardVector();

	ActuationState.ActuationName = FName(TEXT("Stand"));
	ActuationState.HeadingDirection = FVector3f(CurrentForward);
	ActuationState.Prediction.Location = CurrentLocation;
	ActuationState.Prediction.Direction = FVector3f(CurrentForward);
	ActuationState.Prediction.Speed = 0.0f;
	ActuationState.Prediction.Time = 0.0f; // @todo

	TimeElapsed = 0.0f;
}

void UGameplayTask_StandAt::TickTask(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World);
	checkf(MovementComponent, TEXT("Expecting valid Movement Component.")); 

	if (Result != EGameplayTaskActuationResult::None)
	{
		return;
	}

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();

	if (Duration > 0.0f)
	{
		TimeElapsed += DeltaTime;
		if (TimeElapsed >= Duration)
		{
			FinishTask(EGameplayTaskActuationResult::Succeeded);
		}
	}

#if ENABLE_VISUAL_LOG
	if (FVisualLogger::IsRecording())
	{
		UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation, 30, FColor::White, TEXT("Stand At"));

		const FVector Offset(0,0,10);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, ActuationState.Prediction.Location + Offset, FColor::Red, 2, TEXT_EMPTY);
		UE_VLOG_CIRCLE_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, FVector::UpVector, 30.0f, FColor::Red, 2, TEXT("T:%.1f/.1f"), TimeElapsed, Duration);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, ActuationState.Prediction.Location + FVector(ActuationState.Prediction.Direction) * 100.0 + Offset, FColor::Red, 4, TEXT_EMPTY);
	}
#endif // ENABLE_VISUAL_LOG
}
