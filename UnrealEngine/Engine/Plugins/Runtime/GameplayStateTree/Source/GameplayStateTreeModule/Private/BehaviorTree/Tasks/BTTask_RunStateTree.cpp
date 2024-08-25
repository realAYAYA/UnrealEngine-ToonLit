// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_RunStateTree.h"

#include "BehaviorTree/GameplayStateTreeBTUtils.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "StateTreeExecutionContext.h"

UBTTask_RunStateTree::UBTTask_RunStateTree(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	INIT_TASK_NODE_NOTIFY_FLAGS();
	NodeName = TEXT("Run State Tree");
	bCreateNodeInstance = true;
	bTickIntervals = true;
}

EBTNodeResult::Type UBTTask_RunStateTree::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if (SetContextRequirements(OwnerComp, Context))
		{
			const EStateTreeRunStatus StartStatus = Context.Start(&StateTreeRef.GetParameters());
			return GameplayStateTreeBTUtils::StateTreeRunStatusToBTNodeResult(StartStatus);
		}
	}

	return EBTNodeResult::Failed;
}

void UBTTask_RunStateTree::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if (SetContextRequirements(OwnerComp, Context))
		{
			const EStateTreeRunStatus TickStatus = Context.Tick(DeltaSeconds);
			if (TickStatus != EStateTreeRunStatus::Running)
			{
				FinishLatentTask(OwnerComp, GameplayStateTreeBTUtils::StateTreeRunStatusToBTNodeResult(TickStatus));
			}
			else
			{
				SetNextTickTime(NodeMemory, FMath::Max(0.f, Interval + FMath::FRandRange(-RandomDeviation, RandomDeviation)));
			}
			return;
		}
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}

void UBTTask_RunStateTree::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if(SetContextRequirements(OwnerComp, Context))
		{
			Context.Stop();
		}
	}
}

bool UBTTask_RunStateTree::SetContextRequirements(UBehaviorTreeComponent& OwnerComp, FStateTreeExecutionContext& Context)
{
	Context.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateUObject(this, &UBTTask_RunStateTree::CollectExternalData));
	return UStateTreeAIComponentSchema::SetContextRequirements(OwnerComp, Context);
}

bool UBTTask_RunStateTree::CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews)
{
	return UStateTreeAIComponentSchema::CollectExternalData(Context, StateTree, ExternalDataDescs, OutDataViews);
}

TSubclassOf<UStateTreeSchema> UBTTask_RunStateTree::GetSchema() const
{
	return UStateTreeAIComponentSchema::StaticClass();
}
