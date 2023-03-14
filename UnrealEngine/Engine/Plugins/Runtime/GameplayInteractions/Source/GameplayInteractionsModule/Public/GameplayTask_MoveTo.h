// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "AITypes.h"
#include "NavCorridor.h"
#include "GameplayTask.h"
#include "GameplayTaskTransition.h"
#include "GameplayActuationState.h"
#include "GameplayActuationStateProvider.h"
#include "Templates/SharedPointer.h"
#include "GameplayInteractionsTypes.h"
#include "GameplayTask_MoveTo.generated.h"

class UCharacterMovementComponent;
class UGameplayActuationComponent;

UENUM()
enum class EGameplayTaskMoveToIntent : uint8
{
	Stop,
	KeepMoving,
};

UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UGameplayTask_MoveTo : public UGameplayTask, public IGameplayActuationStateProvider
{
	GENERATED_BODY()

public:
	UGameplayTask_MoveTo(const FObjectInitializer& ObjectInitializer);

	/** prepare move task for activation */
	void SetUp(const FAIMoveRequest& InMoveRequest);

	EGameplayTaskActuationResult GetResult() const { return Result; }
	bool WasMoveSuccessful() const { return Result == EGameplayTaskActuationResult::Succeeded; }
	bool WasMovePartial() const { return ActuationState.Path.IsValid() && ActuationState.Path->IsPartial(); }

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuousGoalTracking,ProjectGoalOnNavigation", DefaultToSelf = "Actor", BlueprintInternalUseOnly = "TRUE", DisplayName = "GP Move To Location or Actor"))
	static UGameplayTask_MoveTo* MoveTo(AActor* Actor, FVector GoalLocation, AActor* GoalActor = nullptr, const EGameplayTaskMoveToIntent EndOfPathIntent = EGameplayTaskMoveToIntent::Stop,
		float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
		bool bUsePathfinding = true, bool bUseContinuousGoalTracking = false, EAIOptionFlag::Type ProjectGoalOnNavigation = EAIOptionFlag::Default);

	/** Allows custom move request tweaking. Note that all MoveRequest need to 
	 *	be performed before PerformMove is called. */
	FAIMoveRequest& GetMoveRequestRef() { return MoveRequest; }

	/** Switch task into continuous tracking mode: keep restarting move toward goal actor. Only pathfinding failure or external cancel will be able to stop this task. */
	void SetContinuousGoalTracking(bool bEnable);

	/** Sets the movement intent at the end of the path. */
	void SetEndOfPathIntent(const EGameplayTaskMoveToIntent InEndOfPathIntent);

	/** Registers transition completed delegate. */
	void RegisterTransitionCompleted(IGameplayTaskTransition& Transition)
	{
		Transition.GetTransitionCompleted().AddUObject(this, &UGameplayTask_MoveTo::OnTransitionCompleted);
	}
	
protected:
	/** IGameplayActuationStateProvider */
	virtual FConstStructView GetActuationState() const override
	{
		return FConstStructView::Make(ActuationState);
	}

	void OnTransitionCompleted(const EGameplayTransitionResult InResult, UGameplayTask* InTask);

	virtual void TickTask(float DeltaTime) override;
	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;
	virtual void Resume() override;
	virtual void ExternalCancel() override;

	virtual void TriggerEndOfPathTransition(const double DistanceToEndOfPath);
	
	/** finish task */
	void FinishTask(const EGameplayTaskActuationResult Result);

	/** stores path and starts observing its events */
	void SetObservedPath(FNavPathSharedPtr InPath);

	/** remove all delegates */
	virtual void ResetObservedPath();

	/** tries to update invalidated path */
	void ConditionalUpdatePath();

	/** start move request */
	bool FindPath();

	/** event from followed path */
	virtual void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	void InitPathFollowing();
	void UpdatePathFollow(const float DeltaTime);
	void UpdateCorridor(const int32 PathPointIndex);
	
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FGameplayTaskActuationCompleted OnCompleted;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UGameplayActuationComponent> ActuationComponent = nullptr;

	/** parameters of move request */
	UPROPERTY()
	FAIMoveRequest MoveRequest;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	FNavCorridorParams CorridorParams;

	FGameplayActuationState_Moving ActuationState;

	float FollowLookAheadDistance = 150.0f;

	float HeadingAngle = 0.0f;
	FVector DesiredVelocity;
	FVector ClampedVelocity;
	FVector NearestPathLocation;
	FVector LookAheadPathLocation;

	int32 LastCorridorPathPoint = 0;
	int32 CorridorPathPointIndex = 0;
	
	EGameplayTaskActuationResult Result = EGameplayTaskActuationResult::None;
	
	EGameplayTaskMoveToIntent EndOfPathIntent = EGameplayTaskMoveToIntent::Stop;

	uint8 bCompleteCalled : 1;
	uint8 bIsAtLastCorridor : 1;
	uint8 bEndOfPathTransitionTried : 1;
	uint8 bUseContinuousTracking : 1;

	UPROPERTY()
	TObjectPtr<UGameplayTask> StartTransitionTask = nullptr;

	UPROPERTY()
	TObjectPtr<UGameplayTask> EndTransitionTask = nullptr;
};
