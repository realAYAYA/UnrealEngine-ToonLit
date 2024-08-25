// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "NavFilters/NavigationQueryFilter.h"
#endif
#include "Actions/PawnAction.h"
#include "Navigation/PathFollowingComponent.h"
#include "PawnAction_Move.generated.h"

class AAIController;

UENUM()
namespace EPawnActionMoveMode
{
	enum Type : int
	{
		UsePathfinding,
		StraightLine,
	};
}

UCLASS(MinimalAPI)
class UDEPRECATED_PawnAction_Move : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()
protected:
	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> GoalActor;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	FVector GoalLocation;

	UPROPERTY(Category = PawnAction, EditAnywhere, meta = (ClampMin = "0.01"), BlueprintReadWrite)
	float AcceptableRadius;

	/** "None" will result in default filter being used */
	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	uint32 bAllowStrafe : 1;
	
	/** if set to true (default) will make action succeed when the pawn's collision component overlaps with goal's collision component */
	UPROPERTY()
	uint32 bFinishOnOverlap : 1;

	/** if set, movement will use path finding */
	UPROPERTY()
	uint32 bUsePathfinding : 1;

	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY()
	uint32 bAllowPartialPath : 1;

	/** if set, GoalLocation will be projected on navigation before using  */
	UPROPERTY()
	uint32 bProjectGoalToNavigation : 1;

	/** if set, path to GoalActor will be updated with goal's movement */
	UPROPERTY()
	uint32 bUpdatePathToGoal : 1;

	/** if set, other actions with the same priority will be aborted when path is changed */
	UPROPERTY()
	uint32 bAbortSubActionOnPathChange : 1;

public:
	AIMODULE_API virtual void BeginDestroy() override;

	static AIMODULE_API UDEPRECATED_PawnAction_Move* CreateAction(UWorld& World, AActor* GoalActor, EPawnActionMoveMode::Type Mode);
	static AIMODULE_API UDEPRECATED_PawnAction_Move* CreateAction(UWorld& World, const FVector& GoalLocation, EPawnActionMoveMode::Type Mode);

	static AIMODULE_API bool CheckAlreadyAtGoal(AAIController& Controller, const FVector& TestLocation, float Radius);
	static AIMODULE_API bool CheckAlreadyAtGoal(AAIController& Controller, const AActor& TestGoal, float Radius);

	AIMODULE_API virtual void HandleAIMessage(UBrainComponent*, const FAIMessage&) override;

	AIMODULE_API void SetPath(FNavPathSharedRef InPath);
	AIMODULE_API virtual void OnPathUpdated(FNavigationPath* UpdatedPath, ENavPathEvent::Type Event);

	void SetAcceptableRadius(float NewAcceptableRadius) { AcceptableRadius = NewAcceptableRadius; }
	void SetFinishOnOverlap(bool bNewFinishOnOverlap) { bFinishOnOverlap = bNewFinishOnOverlap; }
	void EnableStrafing(bool bNewStrafing) { bAllowStrafe = bNewStrafing; }
	void EnablePathUpdateOnMoveGoalLocationChange(bool bEnable) { bUpdatePathToGoal = bEnable; }
	void EnableGoalLocationProjectionToNavigation(bool bEnable) { bProjectGoalToNavigation = bEnable; }
	void SetAbortSubActionOnPathUpdate(bool bEnable) { bAbortSubActionOnPathChange = bEnable; }
	void SetFilterClass(TSubclassOf<UNavigationQueryFilter> NewFilterClass) { FilterClass = NewFilterClass; }
	void SetAllowPartialPath(bool bEnable) { bAllowPartialPath = bEnable; }

	UE_DEPRECATED(5.1, "Use SetAbortSubActionOnPathUpdate instead.")
	void EnableChildAbortionOnPathUpdate(bool bEnable) { SetAbortSubActionOnPathUpdate(bEnable); }

protected:
	/** currently followed path */
	FNavPathSharedPtr Path;

	FDelegateHandle PathObserverDelegateHandle;
	
	/** Handle for efficient management of DeferredPerformMoveAction timer */
	FTimerHandle TimerHandle_DeferredPerformMoveAction;

	/** Handle for efficient management of TryToRepath timer */
	FTimerHandle TimerHandle_TryToRepath;

	AIMODULE_API void ClearPath();
	AIMODULE_API virtual bool Start() override;
	AIMODULE_API virtual bool Pause(const UDEPRECATED_PawnAction* PausedBy) override;
	AIMODULE_API virtual bool Resume() override;
	AIMODULE_API virtual void OnFinished(EPawnActionResult::Type WithResult) override;
	AIMODULE_API virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;
	AIMODULE_API virtual bool IsPartialPathAllowed() const;

	AIMODULE_API virtual EPathFollowingRequestResult::Type RequestMove(AAIController& Controller);
	
	AIMODULE_API bool PerformMoveAction();
	AIMODULE_API void DeferredPerformMoveAction();

	AIMODULE_API void TryToRepath();
	AIMODULE_API void ClearPendingRepath();
	AIMODULE_API void ClearTimers();
};
