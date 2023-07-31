// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_MoveTo.h"
#include "AISystem.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "GameplayActuationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayInteractionsTypes.h"
#include "GameplayTaskTransition.h"
#include "Navigation/PathFollowingComponent.h" // LogPathHelper

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_MoveTo)

UGameplayTask_MoveTo::UGameplayTask_MoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bIsPausable = true;

	MoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	MoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	MoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	MoveRequest.SetUsePathfinding(true);

	AddRequiredResource<UAIResource_Movement>();
	AddClaimedResource<UAIResource_Movement>();

	ResourceOverlapPolicy = ETaskResourceOverlapPolicy::RequestCancelAndStartAtEnd;

	Result = EGameplayTaskActuationResult::None;
	bUseContinuousTracking = false;
	bCompleteCalled = false;
	bIsAtLastCorridor = false;
	bEndOfPathTransitionTried = false;
}

UGameplayTask_MoveTo* UGameplayTask_MoveTo::MoveTo(AActor* Actor, FVector InGoalLocation, AActor* InGoalActor, const EGameplayTaskMoveToIntent InEndOfPathIntent,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath,
	bool bUsePathfinding, bool bUseContinuousGoalTracking, EAIOptionFlag::Type ProjectGoalOnNavigation)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	UGameplayTasksComponent* TaskComponent = Actor->FindComponentByClass<UGameplayTasksComponent>();
	if (TaskComponent == nullptr)
	{
		UE_VLOG(Actor, LogGameplayTasks, Error, TEXT("Expecting Pawn to have Gameplay Tasks Component"));
		return nullptr;
	}

	UGameplayTask_MoveTo* Task = NewTask<UGameplayTask_MoveTo>(*Cast<IGameplayTaskOwnerInterface>(TaskComponent));
	if (Task == nullptr)
	{
		return nullptr;
	}

	FAIMoveRequest MoveReq;
	if (InGoalActor)
	{
		MoveReq.SetGoalActor(InGoalActor);
	}
	else
	{
		MoveReq.SetGoalLocation(InGoalLocation);
	}

	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
	MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(ProjectGoalOnNavigation, MoveReq.IsProjectingGoal()));

	Task->SetUp(MoveReq);
	Task->SetContinuousGoalTracking(bUseContinuousGoalTracking);
	Task->SetEndOfPathIntent(InEndOfPathIntent);

	return Task;
}

void UGameplayTask_MoveTo::SetUp(const FAIMoveRequest& InMoveRequest)
{
	MoveRequest = InMoveRequest;
}

void UGameplayTask_MoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}

void UGameplayTask_MoveTo::SetEndOfPathIntent(const EGameplayTaskMoveToIntent InEndOfPathIntent)
{
	EndOfPathIntent = InEndOfPathIntent;
}

void UGameplayTask_MoveTo::FinishTask(const EGameplayTaskActuationResult InResult)
{
	ResetObservedPath();

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

void UGameplayTask_MoveTo::ExternalCancel()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("MoveTo: ExternalCancel. Start:%s End:%s"), *GetNameSafe(StartTransitionTask), *GetNameSafe(EndTransitionTask));

	if (StartTransitionTask != nullptr)
	{
		StartTransitionTask->ExternalCancel();
	}
	if (EndTransitionTask != nullptr)
	{
		EndTransitionTask->ExternalCancel();
	}
	
	EndTask();
}

void UGameplayTask_MoveTo::OnTransitionCompleted(const EGameplayTransitionResult InResult, UGameplayTask* InTask)
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("MoveTo: OnTransitionCompleted. Start:%s End:%s"), *GetNameSafe(StartTransitionTask), *GetNameSafe(EndTransitionTask));

	if (InTask == EndTransitionTask)
	{
		FinishTask(InResult == EGameplayTransitionResult::Succeeded ? EGameplayTaskActuationResult::Succeeded : EGameplayTaskActuationResult::Failed);
		EndTransitionTask = nullptr;
	}
	if (InTask == StartTransitionTask)
	{
		UGameplayTask* TaskToEnd = StartTransitionTask;
		StartTransitionTask = nullptr;
		TaskToEnd->EndTask();
	}
}

void UGameplayTask_MoveTo::Activate()
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
	ActuationComponent = Pawn->FindComponentByClass<UGameplayActuationComponent>();
	if (ActuationComponent == nullptr)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Failed to find actuation component"));
		return;
	}
	
	UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
		MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	if (FindPath() == false)
	{
		FinishTask(EGameplayTaskActuationResult::RequestFailed);
	}

	InitPathFollowing();
}

bool UGameplayTask_MoveTo::FindPath()
{
	APawn* Pawn = Cast<APawn>(GetAvatarActor());
	check(Pawn);

	// Reset
	ResetObservedPath();

	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("MoveTo: %s"), *MoveRequest.ToString());

	// Early out if we cannot access needed systems.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys == nullptr)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s: failed due to no NavigationSystem present. Note that even pathfinding-less movement requires presence of NavigationSystem."), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}
	
	const ANavigationData* NavData = (NavSys == nullptr) ? nullptr :
		MoveRequest.IsUsingPathfinding() ? NavSys->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetNavAgentLocation()) :
		NavSys->GetAbstractNavData();
	if (NavData == nullptr)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s: Unable to find NavigationData instance"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}
	
	if (MoveRequest.IsValid() == false)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s: MoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists. MoveRequest: '%s'"), ANSI_TO_TCHAR(__FUNCTION__), *MoveRequest.ToString());
		return false;
	}

	// Validate target
	if (MoveRequest.IsMoveToActorRequest() == false)
	{
		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s: Destination is not valid! Goal(%s)"), ANSI_TO_TCHAR(__FUNCTION__), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			return false;
		}

		// fail if projection to navigation is required but it failed
		if (MoveRequest.IsProjectingGoal())
		{
			const FNavAgentProperties& AgentProps = Pawn->GetNavAgentPropertiesRef();
			FNavLocation ProjectedLocation;

			if (NavSys->ProjectPointToNavigation(MoveRequest.GetGoalLocation(), ProjectedLocation, INVALID_NAVEXTENT, &AgentProps) == false)
			{
				if (MoveRequest.IsUsingPathfinding())
				{
					UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Error, MoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("%s: failed to project destination location to navmesh"), ANSI_TO_TCHAR(__FUNCTION__));
				}
				else
				{
					UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Error, MoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("%s: failed to project destination location to navmesh, path finding is disabled perhaps disable goal projection ?"), ANSI_TO_TCHAR(__FUNCTION__));
				}
				
				return false;
			}

			MoveRequest.UpdateGoalLocation(ProjectedLocation.Location);
		}
	}
	
	FPathFindingQuery PathQuery;
	
	FVector GoalLocation = MoveRequest.GetGoalLocation();
	if (MoveRequest.IsMoveToActorRequest())
	{
		const INavAgentInterface* NavGoal = Cast<const INavAgentInterface>(MoveRequest.GetGoalActor());
		if (NavGoal)
		{
			const FVector Offset = NavGoal->GetMoveGoalOffset(Pawn);
			GoalLocation = FQuatRotationTranslationMatrix(MoveRequest.GetGoalActor()->GetActorQuat(), NavGoal->GetNavAgentLocation()).TransformPosition(Offset);
		}
		else
		{
			GoalLocation = MoveRequest.GetGoalActor()->GetActorLocation();
		}
	}

	const FSharedConstNavQueryFilter NavFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, Pawn, MoveRequest.GetNavigationFilter());
	PathQuery = FPathFindingQuery(*Pawn, *NavData, Pawn->GetNavAgentLocation(), GoalLocation, NavFilter);
	PathQuery.SetAllowPartialPaths(MoveRequest.IsUsingPartialPaths());

	FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
	
	if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Trying to find path to %s resulted in Error")
			, MoveRequest.IsMoveToActorRequest() ? *GetNameSafe(MoveRequest.GetGoalActor()) : *MoveRequest.GetGoalLocation().ToString());
		UE_VLOG_SEGMENT(GetGameplayTasksComponent(), LogGameplayTasks, Error, Pawn->GetActorLocation()
			, MoveRequest.GetGoalLocation(), FColor::Red, TEXT("Failed move to %s"), *GetNameSafe(MoveRequest.GetGoalActor()));
		return false;
	}

	if (MoveRequest.IsMoveToActorRequest())
	{
		PathResult.Path->SetGoalActorObservation(*MoveRequest.GetGoalActor(), 100.0f);
	}

	PathResult.Path->EnableRecalculationOnInvalidation(true);
	SetObservedPath(PathResult.Path);

	return true;	
}

void UGameplayTask_MoveTo::Resume()
{
	Super::Resume();
	
	// @todo: should repath or fail if InitPathFollowing() fails.
	InitPathFollowing();
}

void UGameplayTask_MoveTo::SetObservedPath(FNavPathSharedPtr InPath)
{
	if (PathUpdateDelegateHandle.IsValid() && ActuationState.Path.IsValid())
	{
		ActuationState.Path->RemoveObserver(PathUpdateDelegateHandle);
	}

	PathUpdateDelegateHandle.Reset();
	
	ActuationState.Path = InPath;
	if (ActuationState.Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		ActuationState.Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = ActuationState.Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UGameplayTask_MoveTo::OnPathEvent));
	}
}

void UGameplayTask_MoveTo::ResetObservedPath()
{
	if (ActuationState.Path.IsValid())
	{
		ActuationState.Path->DisableGoalActorObservation();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (ActuationState.Path.IsValid())
		{
			ActuationState.Path->RemoveObserver(PathUpdateDelegateHandle);
		}

		PathUpdateDelegateHandle.Reset();
	}
	
	ActuationState.Path = nullptr;
	ActuationState.Corridor = nullptr;
	LastCorridorPathPoint = INDEX_NONE;
	CorridorPathPointIndex = INDEX_NONE;
}

void UGameplayTask_MoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
	
	ResetObservedPath();

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	ActuationState.Path = nullptr;
	ActuationState.Corridor = nullptr;
}

void UGameplayTask_MoveTo::OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event)
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());

	const static UEnum* NavPathEventEnum = StaticEnum<ENavPathEvent::Type>();
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
	case ENavPathEvent::NewPath:
	case ENavPathEvent::UpdatedDueToGoalMoved:
	case ENavPathEvent::UpdatedDueToNavigationChanged:
		if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
			UPathFollowingComponent::LogPathHelper(Pawn, InPath, MoveRequest.GetGoalActor());
			FinishTask(EGameplayTaskActuationResult::Failed);
		}
		else
		{
			InitPathFollowing();
		}
		break;

	case ENavPathEvent::Invalidated:
		ConditionalUpdatePath();
		break;

	case ENavPathEvent::Cleared:
	case ENavPathEvent::RePathFailed:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
		FinishTask(EGameplayTaskActuationResult::Failed);
		break;

	case ENavPathEvent::MetaPathUpdate:
	default:
		break;
	}
}

void UGameplayTask_MoveTo::ConditionalUpdatePath()
{
	ANavigationData* NavData = ActuationState.Path.IsValid() ? ActuationState.Path->GetNavigationDataUsed() : nullptr;
	if (NavData)
	{
		NavData->RequestRePath(ActuationState.Path, ENavPathUpdateType::NavigationChanged);
	}
	else
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
		FinishTask(EGameplayTaskActuationResult::Failed);
	}
}

void UGameplayTask_MoveTo::InitPathFollowing()
{
	// Move character along the path.
	if (!ActuationState.Path.IsValid() || MovementComponent == nullptr)
	{
		return;
	}

	// Init actor from current orientation.
	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentForward = MovementComponent->GetActorTransform().GetRotation().GetForwardVector();
	const FVector CurrentVelocity = MovementComponent->Velocity;
	double CurrentSpeed = CurrentVelocity.Length();

	HeadingAngle = FMath::RadiansToDegrees(FMath::Atan2(CurrentForward.Y, CurrentForward.X));

	ActuationState.ActuationName = FName("Move");
	ActuationState.Corridor = MakeShared<FNavCorridor>();
	ActuationState.NavigationLocation = CurrentLocation;
	ActuationState.HeadingDirection = FVector3f(CurrentForward);

	// Find start section
	LastCorridorPathPoint = INDEX_NONE;
	CorridorPathPointIndex = INDEX_NONE;
	UpdateCorridor(0);

	// Stop existing transitions
	if (StartTransitionTask)
	{
		StartTransitionTask->EndTask();
		StartTransitionTask = nullptr;
	}

	// Create start transition
	StartTransitionTask = ActuationComponent->TryMakeTransitionTask(FConstStructView::Make(ActuationState));
	
	if (IGameplayTaskTransition* Transition = Cast<IGameplayTaskTransition>(StartTransitionTask))
	{
		Transition->GetTransitionCompleted().AddUObject(this, &UGameplayTask_MoveTo::OnTransitionCompleted);
		StartTransitionTask->ReadyForActivation();
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("MoveTo: Start Transition: %s"), *GetNameSafe(StartTransitionTask));
	}
}

void UGameplayTask_MoveTo::UpdateCorridor(const int32 PathPointIndex)
{
	if (!ActuationState.Path.IsValid() || !ActuationState.Corridor.IsValid())
	{
		return;
	}
	
	const FNavigationPath* PathInstance = ActuationState.Path.Get();
	if (PathInstance != nullptr && PathInstance->GetPathPoints().IsValidIndex(PathPointIndex))
	{
		// Calculate corridor for up to 4 points at a time
		TArray<FNavPathPoint> PathPoints;
		const int32 NumPathPoints = PathInstance->GetPathPoints().Num();
		int32 LastPointIndex = PathPointIndex;
		const int32 MaxPointIndex = FMath::Min(PathPointIndex + 4, NumPathPoints);
		for (int32 PointIndex = PathPointIndex; PointIndex < MaxPointIndex; PointIndex++)
		{
			const FNavPathPoint& PathPt = PathInstance->GetPathPoints()[PointIndex];
			if (PathPt.CustomLinkId)
			{
				break;
			}
			PathPoints.Add(PathPt);
			LastPointIndex = PointIndex;
		}

		if (LastPointIndex > LastCorridorPathPoint)
		{
			FSharedConstNavQueryFilter NavQueryFilter = PathInstance->GetQueryData().QueryFilter;

			const bool bOffsetFirst = PathPointIndex > 0;
			const bool bOffsetLast = (PathPointIndex + PathPoints.Num()) < NumPathPoints;

			ActuationState.Corridor->BuildFromPathPoints(*PathInstance, PathPoints, PathPointIndex, NavQueryFilter, CorridorParams);
			constexpr double LookAheadToOffsetRatio = 1.0 / 3.0; // Empirically found ratio that works well for most paths.
			ActuationState.Corridor->OffsetPathLocationsFromWalls(FollowLookAheadDistance * LookAheadToOffsetRatio, bOffsetFirst, bOffsetLast);

			LastCorridorPathPoint = LastPointIndex;
		}

		CorridorPathPointIndex = PathPointIndex;

		bIsAtLastCorridor = MaxPointIndex == NumPathPoints;
	}
}

void UGameplayTask_MoveTo::TriggerEndOfPathTransition(const double DistanceToEndOfPath)
{
	FGameplayActuationState_Standing StandingState;
	StandingState.Prediction.Location = ActuationState.Path->GetEndLocation();
	StandingState.Prediction.Direction = FVector3f::ForwardVector; // @todo: provide correct direction
	StandingState.Prediction.Speed = 0.0f;
	StandingState.Prediction.Time = 0.0f; // @todo: provide correct time

	if (EndTransitionTask == nullptr && !bEndOfPathTransitionTried)
	{
		EndTransitionTask = ActuationComponent->TryMakeTransitionTask(FConstStructView::Make(StandingState));
		bEndOfPathTransitionTried = true;
	}

	if (EndTransitionTask
		&& EndTransitionTask->GetState() == EGameplayTaskState::AwaitingActivation)
	{
		FMakeGameplayTransitionTaskContext TransitionContext;
		TransitionContext.Actor = GetOwnerActor();
		TransitionContext.MovementComponent = MovementComponent;
		TransitionContext.ActuationComponent = ActuationComponent;
		TransitionContext.TasksComponent = GetGameplayTasksComponent();
		TransitionContext.CurrentActuationState = FConstStructView::Make(ActuationState);
		TransitionContext.NextActuationState = FConstStructView::Make(StandingState);

		if (IGameplayTaskTransition* Transition = Cast<IGameplayTaskTransition>(EndTransitionTask))
		{
			if (Transition->ShouldActivate(TransitionContext))
			{
				Transition->GetTransitionCompleted().AddUObject(this, &UGameplayTask_MoveTo::OnTransitionCompleted);
				EndTransitionTask->ReadyForActivation();
				
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("MoveTo: End Transition: %s"), *GetNameSafe(EndTransitionTask));
			}
		}
	}
}

void UGameplayTask_MoveTo::UpdatePathFollow(const float DeltaTime)
{
	// Move character along the path.
	if (!ActuationState.Path.IsValid() || MovementComponent == nullptr || !ActuationState.Corridor.IsValid())
	{
		return;
	}

	const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
	const FVector CurrentVelocity = MovementComponent->Velocity;
	double CurrentSpeed = CurrentVelocity.Length();

	// Find current section
	const FNavCorridorLocation NearestPathLoc = ActuationState.Corridor->FindNearestLocationOnPath(CurrentLocation);
	const FNavCorridorLocation LookAheadPathLoc = ActuationState.Corridor->AdvancePathLocation(NearestPathLoc, FollowLookAheadDistance);

	if (!NearestPathLoc.IsValid() || !LookAheadPathLoc.IsValid())
	{
		return;
	}
	
	NearestPathLocation = NearestPathLoc.Location;
	LookAheadPathLocation = LookAheadPathLoc.Location;

	const float MaxSpeed = MovementComponent->GetMaxSpeed();
	// @todo: move to movement styles
	const float MaxTurnRate = 180.0f;
	const float EdgeSeparationDist = 100.0f;
	const float EdgeSeparationForce = 350.0f;
	const float TurnForce = 1.0f / 0.15f;
	const float MaxAcceleration = 350.0f;
	const float MaxDeceleration = 850.0f;
	
	const FVector SteerPos = (NearestPathLoc.Location + LookAheadPathLoc.Location) * 0.5;
	const FVector Target = ActuationState.Corridor->ConstrainVisibility(NearestPathLoc, CurrentLocation, SteerPos, FVector::Distance(CurrentLocation, LookAheadPathLoc.Location));

	const FVector TargetDir = (Target - CurrentLocation).GetSafeNormal2D();
	FVector HeadingDir(FMath::Cos(FMath::DegreesToRadians(HeadingAngle)), FMath::Sin(FMath::DegreesToRadians(HeadingAngle)), 0.0);

	const FVector Left0 = ActuationState.Corridor->Portals[NearestPathLoc.PortalIndex].Left;
	const FVector Left1 = ActuationState.Corridor->Portals[NearestPathLoc.PortalIndex + 1].Left;
	const FVector LeftNorm = -FVector::CrossProduct(Left1 - Left0, FVector::UpVector).GetSafeNormal();

	const FVector Right0 = ActuationState.Corridor->Portals[NearestPathLoc.PortalIndex].Right;
	const FVector Right1 = ActuationState.Corridor->Portals[NearestPathLoc.PortalIndex + 1].Right;
	const FVector RightNorm = FVector::CrossProduct(Right1 - Right0, FVector::UpVector).GetSafeNormal();


	// Speed control based on predicted collision.
	const double HitDist = FMath::Square(MaxSpeed) / (MaxDeceleration * 2.0);
	double HitT = 1.0;
	ActuationState.Corridor->HitTest(CurrentLocation, CurrentLocation + HeadingDir * HitDist, HitT);

	const double DistToCollision = HitDist * HitT;
	const double DistToStop = FMath::Square(CurrentSpeed) / (MaxDeceleration * 2.0);
	const double MaxSpeedToStop = FMath::Sqrt(DistToCollision * MaxDeceleration * 2.0);

	// @todo: rearrange the code so that this can be done where other debug draw is done.
#if ENABLE_VISUAL_LOG
	if (FVisualLogger::IsRecording())
	{
		const FVector Offset(0,0,50);
		const FVector HitTarget = CurrentLocation + HeadingDir * HitDist * HitT;
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, Offset+CurrentLocation, Offset+HitTarget, FColor::Orange, 2, TEXT("CollDist:%.1f/%.1f  Speed:%.1f/%.1f  CollSpeed:%1f"), DistToCollision, DistToStop, CurrentSpeed,MaxSpeed, MaxSpeedToStop);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, Offset+HitTarget, HitTarget, FColor::Orange, 1, TEXT(""));
	}
#endif // ENABLE_VISUAL_LOG
	
	DesiredVelocity = TargetDir * FMath::Min(MaxSpeed, MaxSpeedToStop);

	const FVector RabbitDir = (LookAheadPathLoc.Location - CurrentLocation).GetSafeNormal();

	const double LeftDist = FVector::DotProduct(LeftNorm, CurrentLocation - Left0);
	const double LeftMag = FMath::Square(1.0 - FMath::Clamp(LeftDist / EdgeSeparationDist, 0.0f, 1.0));

	const double RightDist = FVector::DotProduct(RightNorm, CurrentLocation - Right0);
	const double RightMag = FMath::Square(1.0 - FMath::Clamp(RightDist / EdgeSeparationDist, 0.0f, 1.0));

	FVector Force = FVector::ZeroVector;
	Force += LeftNorm * LeftMag * EdgeSeparationForce;
	Force += RightNorm * RightMag * EdgeSeparationForce;

	FVector NewVelocity = DesiredVelocity + Force * DeltaTime;
	
	const double HeadingYaw = FMath::RadiansToDegrees(FMath::Atan2(NewVelocity.Y, NewVelocity.X));
	const double DeltaYaw = FMath::Clamp(FRotator3f::NormalizeAxis(HeadingYaw - HeadingAngle) * TurnForce, -MaxTurnRate, MaxTurnRate);

	double Speed = FMath::Max(0.0f, FVector::DotProduct(HeadingDir, DesiredVelocity));

	Speed = FMath::Min(Speed, MaxSpeedToStop);

	const float DeltaSpeed = FMath::Clamp(Speed - CurrentSpeed, -MaxDeceleration * DeltaTime, MaxAcceleration * DeltaTime);
	CurrentSpeed += DeltaSpeed;

	HeadingAngle = FRotator3f::NormalizeAxis(HeadingAngle + DeltaYaw * DeltaTime);

	const double DistanceToEndOfPath = bIsAtLastCorridor ? ActuationState.Corridor->GetDistanceToEndOfPath(NearestPathLoc) : MAX_dbl;

	// Create end transition, the task is activated later.
	constexpr double TryActivateDistance = 500.0;

	if (EndOfPathIntent == EGameplayTaskMoveToIntent::Stop
		&& DistanceToEndOfPath < TryActivateDistance)
	{
		TriggerEndOfPathTransition(DistanceToEndOfPath);
	}

	const int32 CurrentPathPointIndex = ActuationState.Corridor->Portals[NearestPathLoc.PortalIndex].PathPointIndex;
	if (CurrentPathPointIndex != CorridorPathPointIndex)
	{
		UpdateCorridor(CurrentPathPointIndex);
	}
	
	ClampedVelocity = HeadingDir * CurrentSpeed;

	MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
	MovementComponent->RequestDirectMove(ClampedVelocity, /*bForceMaxSpeed*/false);

	if (StartTransitionTask == nullptr && EndTransitionTask == nullptr)
	{
		const float AcceptanceRadius = MoveRequest.GetAcceptanceRadius() > 0.0f ? MoveRequest.GetAcceptanceRadius() : 10.0f; 
		if (DistanceToEndOfPath < AcceptanceRadius)
		{
			FinishTask(EGameplayTaskActuationResult::Succeeded);
		}
	}

	// Update movement state
	ActuationState.NavigationLocation = CurrentLocation;
	ActuationState.HeadingDirection = FVector3f(FMath::Cos(FMath::DegreesToRadians(HeadingAngle)), FMath::Sin(FMath::DegreesToRadians(HeadingAngle)), 0.0);
	
	ActuationState.Prediction.Location = LookAheadPathLocation;
	ActuationState.Prediction.Direction = FVector3f(RabbitDir);
	ActuationState.Prediction.Speed = CurrentSpeed;
	ActuationState.Prediction.Time = 0.0f; // @todo:
}

void UGameplayTask_MoveTo::TickTask(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World);
	checkf(MovementComponent, TEXT("Expecting valid Movement Component.")); 

	if (Result != EGameplayTaskActuationResult::None)
	{
		return;
	}
	
	UpdatePathFollow(DeltaTime);

#if ENABLE_VISUAL_LOG
	if (FVisualLogger::IsRecording())
	{
		const FVector CurrentLocation = MovementComponent->GetActorFeetLocation();
		UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Log, CurrentLocation, 30, FColor::White, TEXT("Move To"));

		// Debug draw
		if (ActuationState.Path.IsValid())
		{
			UE::GameplayInteraction::Debug::VLogPath(GetGameplayTasksComponent(), *ActuationState.Path.Get());
		}

		if (ActuationState.Corridor.IsValid())
		{
			UE::GameplayInteraction::Debug::VLogCorridor(GetGameplayTasksComponent(), *ActuationState.Corridor.Get());
		}

		const FVector Offset(0,0,10);
		const FVector Offset2(0,0,15);

		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, Offset + CurrentLocation, Offset + CurrentLocation + DesiredVelocity, FColor::Silver, 2, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, Offset2 + CurrentLocation, Offset2 + CurrentLocation + ClampedVelocity, FColor::Yellow, 4, TEXT_EMPTY);
		UE_VLOG_SEGMENT_THICK(GetGameplayTasksComponent(), LogGameplayTasks, Log, Offset2 + NearestPathLocation, Offset2 + LookAheadPathLocation, FColor::Blue, 4, TEXT_EMPTY);
	}
#endif // ENABLE_VISUAL_LOG
}

