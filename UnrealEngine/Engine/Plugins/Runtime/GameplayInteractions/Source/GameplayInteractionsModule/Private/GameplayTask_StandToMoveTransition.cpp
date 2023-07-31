// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_StandToMoveTransition.h"
#include "AISystem.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "GameplayActuationComponent.h"
#include "NavCorridor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayInteractionsTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_StandToMoveTransition)

//-----------------------------------------------------
// FGameplayTransitionDesc_StandToMove
//-----------------------------------------------------
UGameplayTask* FGameplayTransitionDesc_StandToMove::MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const
{
	check(Context.MovementComponent);
	
	//	const FGameplayActuationStateBase& CurrentState = Context.CurrentMovementState.Get<FGameplayActuationStateBase>();
	const FGameplayActuationState_Moving* NextState = Context.NextActuationState.GetPtr<FGameplayActuationState_Moving>();

	if (NextState == nullptr)
	{
		return nullptr;
	}

	const FVector CurrentLocation = Context.MovementComponent->GetActorFeetLocation();
	const FVector CurrentForward = Context.MovementComponent->GetActorTransform().GetRotation().GetForwardVector();
	const FVector CurrentVelocity = Context.MovementComponent->Velocity;
	const double CurrentSpeed = CurrentVelocity.Length();
	const float MaxSpeed = Context.MovementComponent->GetMaxSpeed();

	constexpr double StandSpeedThreahold = 25.0;
	constexpr double StandToMoveDistance = 250.0;

	// If moving too fast, dont try to to do this transition. 
	if (CurrentSpeed > StandSpeedThreahold)
	{
		return nullptr;
	}

	// Match, create transitions.
	const FNavCorridorLocation NearestPathLoc = NextState->Corridor->FindNearestLocationOnPath(CurrentLocation);
	const FNavCorridorLocation LookAheadPathLoc = NextState->Corridor->AdvancePathLocation(NearestPathLoc, StandToMoveDistance);

	const FVector PathDir = NextState->Corridor->GetPathDirection(LookAheadPathLoc);
		
	UGameplayTask_StandToMoveTransition* NewTransitionTask = UGameplayTask::NewTask<UGameplayTask_StandToMoveTransition>(Context.TasksComponent);

	NewTransitionTask->BreakingDistance = StandToMoveDistance;
	NewTransitionTask->StartHeadingDirection = CurrentForward;
	NewTransitionTask->StartDistanceToEnd = FVector::Dist2D(LookAheadPathLoc.Location, CurrentLocation);

	NewTransitionTask->ActuationState.ActuationName = FName(TEXT("StandToMove"));
	NewTransitionTask->ActuationState.HeadingDirection = FVector3f(CurrentForward);
	NewTransitionTask->ActuationState.Prediction.Location = LookAheadPathLoc.Location;
	NewTransitionTask->ActuationState.Prediction.Direction = FVector3f(PathDir);
	NewTransitionTask->ActuationState.Prediction.Speed = MaxSpeed;
	NewTransitionTask->ActuationState.Prediction.Time = 0.0f; // @todo
	NewTransitionTask->ActuationState.Path = NextState->Path;
	NewTransitionTask->ActuationState.Corridor = NextState->Corridor;

	// Transition task setup
	NewTransitionTask->ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	NewTransitionTask->Priority = 128;
	
	return NewTransitionTask;
} 

//-----------------------------------------------------
// UGameplayTask_StandToMoveTransition
//-----------------------------------------------------
UGameplayTask_StandToMoveTransition::UGameplayTask_StandToMoveTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());
}

void UGameplayTask_StandToMoveTransition::ExternalCancel()
{
	EndTask();
}

void UGameplayTask_StandToMoveTransition::Activate()
{
	Super::Activate();

	APawn* Pawn = Cast<APawn>(GetAvatarActor());
	check(Pawn);

	MovementComponent = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if (MovementComponent == nullptr)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Failed to find movement component"));
		bCompleted = true;
		return;
	}
	
	bCompleted = false;
}

void UGameplayTask_StandToMoveTransition::TickTask(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World);
	checkf(MovementComponent, TEXT("Expecting valid Movement Component.")); 

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentVelocity = MovementComponent->Velocity;
	const double CurrentSpeed = CurrentVelocity.Length();
	const float MaxSpeed = MovementComponent->GetMaxSpeed();

	const float MaxAcceleration = 350.0f;

	const FVector EndLocation = ActuationState.Prediction.Location;
	const FVector EndDirection = FVector(ActuationState.Prediction.Direction);

	const float DistToEnd = FVector::Dist2D(CurrentLocation, EndLocation);

	const float Fade = FMath::Clamp((StartDistanceToEnd - DistToEnd) / BreakingDistance, 0.0f, 1.0f);
	const float FadeMaxSpeed = FMath::Max(40.0f, MaxSpeed * FMath::Sqrt(Fade));
	const float DesiredSpeed = FMath::Max(CurrentSpeed, FadeMaxSpeed);

	const double DistanceToGoal = FVector::DotProduct(EndDirection, CurrentLocation - EndLocation);

	const FVector DirToEnd = DistanceToGoal > -10.0f ? EndDirection : (EndLocation - CurrentLocation).GetSafeNormal();
	const FVector HeadingDir = FMath::Lerp(StartHeadingDirection, EndDirection, FMath::Square(Fade)).GetSafeNormal();

	const FVector ClampedVelocity = DirToEnd * DesiredSpeed;

	MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
	MovementComponent->RequestDirectMove(ClampedVelocity, /*bForceMaxSpeed*/false);

	if (!bCompleted && DistanceToGoal > 0.0f)
	{
		if (OnTransitionCompleted.IsBound())
		{
			OnTransitionCompleted.Broadcast(EGameplayTransitionResult::Succeeded, this);
		}
		bCompleted = true;
	}

#if ENABLE_VISUAL_LOG
	if (FVisualLogger::IsRecording())
	{
		UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation, 30, FColor::White, TEXT("Stand -> Move"));

		const FVector Offset(0,0,10);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, CurrentLocation + Offset + HeadingDir * 150.0f, FColor::Blue, 4, TEXT("Des Speed:%f  DistanceToGoal:%f"), DesiredSpeed, DistanceToGoal);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, ActuationState.Prediction.Location + Offset, FColor::Yellow, 2, TEXT("Fade:%.2f"), Fade);
		UE_VLOG_CIRCLE_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, FVector::UpVector, 30.0f, FColor::Yellow, 2, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, ActuationState.Prediction.Location + FVector(ActuationState.Prediction.Direction) * 100.0 + Offset, FColor::Yellow, 4, TEXT_EMPTY);

		// Debug draw
		if (ActuationState.Path.IsValid())
		{
			UE::GameplayInteraction::Debug::VLogPath(GetGameplayTasksComponent(), *ActuationState.Path.Get());
		}

		if (ActuationState.Corridor.IsValid())
		{
			UE::GameplayInteraction::Debug::VLogCorridor(GetGameplayTasksComponent(), *ActuationState.Corridor.Get());
		}
	}
#endif // ENABLE_VISUAL_LOG
}

