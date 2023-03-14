// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_SuddenTurnTransition.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_SuddenTurnTransition)

//-----------------------------------------------------
// FGameplayTransitionDesc_SuddenTurn
//-----------------------------------------------------
UGameplayTask* FGameplayTransitionDesc_SuddenTurn::MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const
{
	check(Context.MovementComponent);
	
	const FGameplayActuationState_Moving* CurrentState = Context.CurrentActuationState.GetPtr<FGameplayActuationState_Moving>();
	const FGameplayActuationState_Moving* NextState = Context.NextActuationState.GetPtr<FGameplayActuationState_Moving>();

	if (CurrentState == nullptr || NextState == nullptr)
	{
		return nullptr;
	}

	const FName MoveName("Move");
	if (CurrentState->ActuationName != MoveName || NextState->ActuationName != MoveName)
	{
		return nullptr;
	}

	const FVector CurrentLocation = Context.MovementComponent->GetActorFeetLocation();
	const FVector CurrentForward = Context.MovementComponent->GetActorTransform().GetRotation().GetForwardVector();
	const FVector CurrentVelocity = Context.MovementComponent->Velocity;
	const double CurrentSpeed = CurrentVelocity.Length();
	const float MaxSpeed = Context.MovementComponent->GetMaxSpeed();

	const double StandToMoveDistance = 150.0;
		
	const FNavCorridorLocation NearestPathLoc = NextState->Corridor->FindNearestLocationOnPath(CurrentLocation);
	const FNavCorridorLocation LookAheadPathLoc = NextState->Corridor->AdvancePathLocation(NearestPathLoc, StandToMoveDistance);

	const FVector DirToLookAhead = (LookAheadPathLoc.Location - CurrentLocation).GetSafeNormal2D();

	// If the new path is this much different, make a transition.
	constexpr double AngleThreshold = 45.0;
	const double AngleThresholdCos = FMath::Cos(FMath::DegreesToRadians(AngleThreshold));
	const double DirToLookAheadCos = FVector::DotProduct(DirToLookAhead, FVector(CurrentState->HeadingDirection));
	if (DirToLookAheadCos > AngleThresholdCos)
	{
		return nullptr;
	}

	// Match, create transitions.
	const FVector PathDir = NextState->Corridor->GetPathDirection(LookAheadPathLoc);
		
	UGameplayTask_SuddenTurnTransition* NewTransitionTask = UGameplayTask::NewTask<UGameplayTask_SuddenTurnTransition>(Context.TasksComponent);

	NewTransitionTask->BreakingDistance = StandToMoveDistance;
	NewTransitionTask->StartHeadingDirection = FVector(CurrentState->HeadingDirection);
	
	NewTransitionTask->ActuationState.ActuationName = FName(TEXT("SuddenTurn"));
	NewTransitionTask->ActuationState.HeadingDirection = CurrentState->HeadingDirection;
	NewTransitionTask->ActuationState.Prediction.Location = LookAheadPathLoc.Location;
	NewTransitionTask->ActuationState.Prediction.Direction = FVector3f(PathDir);
	NewTransitionTask->ActuationState.Prediction.Speed = MaxSpeed;
	NewTransitionTask->ActuationState.Prediction.Time = 0.0f; // @todo
	NewTransitionTask->ActuationState.Path = CurrentState->Path;
	NewTransitionTask->ActuationState.Corridor = CurrentState->Corridor;

	// Transition task setup
	NewTransitionTask->ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	NewTransitionTask->Priority = 128;
	
	return NewTransitionTask;
} 

//-----------------------------------------------------
// UGameplayTask_SuddenTurnTransition
//-----------------------------------------------------
UGameplayTask_SuddenTurnTransition::UGameplayTask_SuddenTurnTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());
}

void UGameplayTask_SuddenTurnTransition::ExternalCancel()
{
	EndTask();
}

void UGameplayTask_SuddenTurnTransition::Activate()
{
	Super::Activate();

	const APawn* Pawn = Cast<APawn>(GetAvatarActor());
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

void UGameplayTask_SuddenTurnTransition::TickTask(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World);
	checkf(MovementComponent, TEXT("Expecting valid Movement Component.")); 

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentVelocity = MovementComponent->Velocity;
	double CurrentSpeed = CurrentVelocity.Length();

	UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation, 30, FColor::White, TEXT("Sudden Turn"));

	const float MaxSpeed = MovementComponent->GetMaxSpeed();
	const float MaxAcceleration = 350.0f;

	const FVector EndLocation = ActuationState.Prediction.Location;
	const FVector EndDirection = FVector(ActuationState.Prediction.Direction);

	const float DistToEnd = FVector::Dist2D(CurrentLocation, EndLocation);

	const float Fade = FMath::Clamp((DistToEnd) / BreakingDistance, 0.0f, 1.0f);
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
		const FVector Offset(0,0,10);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, EndLocation + Offset, FColor::Yellow, 2, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, CurrentLocation + Offset + HeadingDir * 150.0f, FColor::Blue, 4, TEXT_EMPTY);

		// Destination
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, ActuationState.Prediction.Location + Offset, FColor::Orange, 2, TEXT("Fade:%.2f"), Fade);
		UE_VLOG_CIRCLE_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, FVector::UpVector, 30.0f, FColor::Orange, 2, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, ActuationState.Prediction.Location + FVector(ActuationState.Prediction.Direction) * 100.0 + Offset, FColor::Orange, 4, TEXT_EMPTY);

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

