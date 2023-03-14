// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTTask_StopTree.h"
#include "MockAI_BT.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TestBTTask_StopTree)

UTestBTTask_StopTree::UTestBTTask_StopTree(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	StopTimming = EBTTestTaskStopTree::DuringExecute;
	LogResult = EBTNodeResult::Succeeded;
	LogIndex = INDEX_NONE;
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UTestBTTask_StopTree::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (StopTimming == EBTTestTaskStopTree::DuringExecute)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
		return LogResult;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UTestBTTask_StopTree::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (StopTimming == EBTTestTaskStopTree::DuringAbort)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
	}
	return EBTNodeResult::Aborted;
}

void UTestBTTask_StopTree::TickTask(class UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (StopTimming == EBTTestTaskStopTree::DuringTick)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
		FinishLatentTask(OwnerComp, LogResult);
	}
}

void UTestBTTask_StopTree::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (StopTimming == EBTTestTaskStopTree::DuringFinish)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
	}
}

