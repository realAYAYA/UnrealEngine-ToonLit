// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PawnActionBase.generated.h"

class UDEPRECATED_PawnAction;

enum class EPawnActionTaskResult : uint8
{
	Unknown,
	TaskFinished,
	TaskAborted,
	ActionLost,
};

/**
 * Base class for managing pawn actions
 *
 * Task will set itself as action observer before pushing it to AI Controller,
 * override OnActionEvent if you need any special event handling.
 *
 * Please use result returned by PushAction for ExecuteTask function.
 */

UCLASS(Abstract, MinimalAPI)
class UBTTask_PawnActionBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	AIMODULE_API virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:

	/** starts executing pawn action */
	AIMODULE_API EBTNodeResult::Type PushAction(UBehaviorTreeComponent& OwnerComp, UDEPRECATED_PawnAction& Action);

	/** action observer, updates state of task */
	AIMODULE_API virtual void OnActionEvent(UDEPRECATED_PawnAction& Action, EPawnActionEventType::Type Event);

	/** called when action is removed from stack (FinishedAborting) by some external event
	 *  default behavior: finish task as failed */
	AIMODULE_API virtual void OnActionLost(UDEPRECATED_PawnAction& Action);

public:

	/** helper functions, should be used when behavior tree task deals with pawn actions, but can't derive from this class */
	static AIMODULE_API EPawnActionTaskResult ActionEventHandler(UBTTaskNode* TaskNode, UDEPRECATED_PawnAction& Action, EPawnActionEventType::Type Event);
};
