// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTask_MoveToContextualAnim.h"
#include "VisualLogger/VisualLogger.h"
#include "GameplayTasksComponent.h"
#include "GameplayActuationComponent.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimSceneInstance.h"
#include "GameplayTask_PlayContextualAnim.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_MoveToContextualAnim)

UGameplayTask_MoveToContextualAnim* UGameplayTask_MoveToContextualAnim::EnterContextualAnim(
		AActor* Interactor
		, const FName InteractorRole
		, AActor* InteractableObject
		, const FName InteractableObjectRole
		, const FName SectionName
		, const FName ExitSectionName
		, const UContextualAnimSceneAsset* SceneAsset
		)
{
	if (Interactor == nullptr)
	{
		UE_VLOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: Expecting Interactor to be valid"));
		return nullptr;
	}

	UGameplayTasksComponent* TaskComponent = Interactor->FindComponentByClass<UGameplayTasksComponent>();
	if (TaskComponent == nullptr)
	{
		UE_VLOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: Expecting Pawn to have Gameplay Tasks Component"));
		return nullptr;
	}
	UGameplayActuationComponent* InteractorActuationComponent = Interactor->FindComponentByClass<UGameplayActuationComponent>(); 
	if (InteractorActuationComponent == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: missing UGameplayActuationComponent."));
		return nullptr;
	}

	if (InteractableObject == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: InteractableObject missing."));
		return nullptr;
	}
	if (SceneAsset == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: SceneAsset missing."));
		return nullptr;
	}

	// Find location to move to
	FContextualAnimStartSceneParams SceneParams;
	SceneParams.RoleToActorMap.Add(InteractorRole, Interactor);
	SceneParams.RoleToActorMap.Add(InteractableObjectRole, InteractableObject);
	SceneParams.SectionIdx = SceneAsset->GetSectionIndex(SectionName);
	SceneParams.AnimSetIdx = INDEX_NONE;

	if (SceneParams.SectionIdx == INDEX_NONE)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: Could not find section '%s' in asset '%s'."), *SectionName.ToString(), *GetNameSafe(SceneAsset));
		return nullptr;
	}

	// When a starting section is specified we need to precompute the pivots so they are ready
	// to start the scene on all clients. Otherwise they will be computed for transitions (late start or transitions to next sections).
	TArray<FContextualAnimSetPivot> Pivots;
	FContextualAnimSceneBindings Bindings;
	if (UGameplayTask_PlayContextualAnim::CreateBindings(*SceneAsset, SceneParams, Bindings))
	{
		Bindings.CalculateAnimSetPivots(Pivots);
		SceneParams.AnimSetIdx = Bindings.GetAnimSetIdx();
	}
	else
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: unable to find matching bindings or force some."));
		return nullptr;
	}
	
	const FContextualAnimSceneBinding* PrimaryRoleBinding = Bindings.FindBindingByRole(SceneAsset->GetPrimaryRole());
	if (PrimaryRoleBinding == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: unable to find PrimaryRoleBinding."));
		return nullptr;
	}

	// Get pathing entry point from the contextual animation.
	TArray<FContextualAnimPoint> Points;
	SceneAsset->GetAlignmentPointsForSecondaryRole(EContextualAnimPointType::FirstFrame, SceneParams.SectionIdx, PrimaryRoleBinding->GetContext(), Points);
	if (Points.Num() == 0)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: unable to find alignment points."));
		return nullptr;
	}

	
	UGameplayTask_MoveToContextualAnim* Task = NewTask<UGameplayTask_MoveToContextualAnim>(*Cast<IGameplayTaskOwnerInterface>(TaskComponent));
	if (Task == nullptr)
	{
		return nullptr;
	}

	Task->NextState.InteractorRole = InteractorRole;
	Task->NextState.InteractableObject = InteractableObject;
	Task->NextState.InteractableObjectRole = InteractableObjectRole;
	Task->NextState.SectionName = SectionName;
	Task->NextState.ExitSectionName = ExitSectionName;
	Task->NextState.SceneAsset = SceneAsset;

	FAIMoveRequest MoveReq;

	MoveReq.SetGoalLocation(Points[0].Transform.GetLocation());

	MoveReq.SetAcceptanceRadius(10.0);
	MoveReq.SetAllowPartialPath(false);
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetProjectGoalLocation(true);

	Task->SetUp(MoveReq);
	Task->SetContinuousGoalTracking(true);
	Task->SetEndOfPathIntent(EGameplayTaskMoveToIntent::Stop);

	return Task;
}

void UGameplayTask_MoveToContextualAnim::TriggerEndOfPathTransition(const double DistanceToEndOfPath)
{
	// @todo: This is a temp solution to trigger the transition.
	if (DistanceToEndOfPath < 20.0)
	{
		if (EndTransitionTask == nullptr && !bEndOfPathTransitionTried)
		{
			EndTransitionTask = ActuationComponent->TryMakeTransitionTask(FConstStructView::Make(NextState));
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
			TransitionContext.NextActuationState = FConstStructView::Make(NextState);

			if (IGameplayTaskTransition* Transition = Cast<IGameplayTaskTransition>(EndTransitionTask))
			{
				RegisterTransitionCompleted(*Transition);
				EndTransitionTask->ReadyForActivation();
			}
		}
	}
}

