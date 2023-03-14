// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/TestBTService_StopTree.h"
#include "MockAI_BT.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TestBTService_StopTree)

UTestBTService_StopTree::UTestBTService_StopTree(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "StopTreeService";

	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	LogIndex = INDEX_NONE;
	StopTimming = EBTTestServiceStopTree::DuringTick;

	// Force the service to tick every frame
	Interval = 0.0f;
	RandomDeviation = 0.0f;
}

void UTestBTService_StopTree::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (StopTimming == EBTTestServiceStopTree::DuringBecomeRelevant)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
	}
}

void UTestBTService_StopTree::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	if (StopTimming == EBTTestServiceStopTree::DuringCeaseRelevant)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
	}
}

void UTestBTService_StopTree::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (StopTimming == EBTTestServiceStopTree::DuringTick)
	{
		UMockAI_BT::ExecutionLog.Add(LogIndex);
		OwnerComp.StopTree();
	}
}
