// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_MoveToStandTransition.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_MoveToStandTransition)

//-----------------------------------------------------
// FGameplayTransitionDesc_MoveToStand
//-----------------------------------------------------
UGameplayTask* FGameplayTransitionDesc_MoveToStand::MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const
{
	check(Context.MovementComponent);
	
	const FGameplayActuationState_Moving* CurrentState = Context.CurrentActuationState.GetPtr<FGameplayActuationState_Moving>();
	const FGameplayActuationState_Standing* NextState = Context.NextActuationState.GetPtr<FGameplayActuationState_Standing>();

	if (CurrentState == nullptr || NextState == nullptr)
	{
		return nullptr;
	}

	constexpr double BreakingDistance = 250.0;

	// Match, create transitions.
	UGameplayTask_MoveToStandTransition* NewTransitionTask = UGameplayTask::NewTask<UGameplayTask_MoveToStandTransition>(Context.TasksComponent);

	const int32 LastIndex = CurrentState->Corridor->Portals.Num() - 2;
	const FVector PathDir = LastIndex >= 0 ? (CurrentState->Corridor->Portals[LastIndex + 1].Location - CurrentState->Corridor->Portals[LastIndex].Location).GetSafeNormal2D() : FVector::ForwardVector;
	
	NewTransitionTask->BreakingDistance = BreakingDistance;
	NewTransitionTask->StartHeadingDirection = FVector(CurrentState->HeadingDirection);
	
	NewTransitionTask->ActuationState.ActuationName = FName(TEXT("MoveToStand"));
	NewTransitionTask->ActuationState.HeadingDirection = CurrentState->HeadingDirection;
	NewTransitionTask->ActuationState.Prediction.Location = CurrentState->Path->GetEndLocation();
	NewTransitionTask->ActuationState.Prediction.Direction = FVector3f(PathDir);
	NewTransitionTask->ActuationState.Prediction.Speed = 0.0f;
	NewTransitionTask->ActuationState.Prediction.Time = 0.0f; // @todo
	NewTransitionTask->ActuationState.Path = CurrentState->Path;
	NewTransitionTask->ActuationState.Corridor = CurrentState->Corridor;
	
	// Transition task setup
	NewTransitionTask->ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	NewTransitionTask->Priority = 128;
	
	return NewTransitionTask;
}

//-----------------------------------------------------
// UGameplayTask_MoveToStandTransition
//-----------------------------------------------------
UGameplayTask_MoveToStandTransition::UGameplayTask_MoveToStandTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());
}

bool UGameplayTask_MoveToStandTransition::ShouldActivate(const FMakeGameplayTransitionTaskContext& Context) const
{
	const FGameplayActuationState_Moving* CurrentState = Context.CurrentActuationState.GetPtr<FGameplayActuationState_Moving>();
	const FGameplayActuationState_Standing* NextState = Context.NextActuationState.GetPtr<FGameplayActuationState_Standing>();

	if (CurrentState == nullptr || NextState == nullptr)
	{
		return false;
	}

	const FVector CurrentLocation = Context.MovementComponent->GetActorFeetLocation();
	const double DistanceToStand = FVector::Dist2D(CurrentLocation, NextState->Prediction.Location);

	return DistanceToStand < BreakingDistance;
}


void UGameplayTask_MoveToStandTransition::ExternalCancel()
{
	EndTask();
}

void UGameplayTask_MoveToStandTransition::Activate()
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

void UGameplayTask_MoveToStandTransition::TickTask(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World);
	checkf(MovementComponent, TEXT("Expecting valid Movement Component.")); 

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentVelocity = MovementComponent->Velocity;
	double CurrentSpeed = CurrentVelocity.Length();

	const float MaxSpeed = MovementComponent->GetMaxSpeed();
	constexpr float MaxAcceleration = 350.0f;

	const FVector EndLocation = ActuationState.Prediction.Location;
	const FVector EndDirection = FVector(ActuationState.Prediction.Direction);
	
	const float DistanceToEnd = FVector::DistSquared2D(CurrentLocation, EndLocation);

	const float Speed = MaxSpeed;
	const float DeltaSpeed = FMath::Clamp(Speed - CurrentSpeed, -MaxAcceleration * DeltaTime, MaxAcceleration * DeltaTime);
	CurrentSpeed += DeltaSpeed;

	const float Fade = FMath::Clamp(FMath::Sqrt(DistanceToEnd) / BreakingDistance, 0.0f, 1.0f);
	const float FadeMaxSpeed = MaxSpeed * FMath::Sqrt(Fade);
	CurrentSpeed = FMath::Clamp(CurrentSpeed, 1.0, FadeMaxSpeed);

	const double DistanceToGoal = FVector::DotProduct(EndDirection, CurrentLocation - EndLocation);

	const FVector DirToEnd = DistanceToGoal > -10.0f ? EndDirection : (EndLocation - CurrentLocation).GetSafeNormal();
	const FVector HeadingDir = FMath::Lerp(StartHeadingDirection, EndDirection, FMath::Square(Fade)).GetSafeNormal();

	ActuationState.HeadingDirection = FVector3f(HeadingDir);
	
	const FVector ClampedVelocity = DirToEnd * CurrentSpeed;
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
		UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation, 30, FColor::White, TEXT("Move -> Stand"));

		const FVector Offset(0,0,10);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation + Offset, ActuationState.Prediction.Location + Offset, FColor::Red, 2, TEXT("Fade:%.2f"), Fade);
		UE_VLOG_CIRCLE_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, FVector::UpVector, 30.0f, FColor::Red, 2, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, ActuationState.Prediction.Location + Offset, ActuationState.Prediction.Location + FVector(ActuationState.Prediction.Direction) * 100.0 + Offset, FColor::Red, 4, TEXT_EMPTY);

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

