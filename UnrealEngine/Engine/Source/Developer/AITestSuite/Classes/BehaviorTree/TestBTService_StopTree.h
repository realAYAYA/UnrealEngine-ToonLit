// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTService.h"
#include "TestBTService_StopTree.generated.h"

UENUM()
namespace EBTTestServiceStopTree
{
	enum Type
	{
		DuringBecomeRelevant,
		DuringTick,
		DuringCeaseRelevant,
	};
}

UCLASS(meta = (HiddenNode))
class UTestBTService_StopTree : public UBTService
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 LogIndex;

	UPROPERTY()
	TEnumAsByte<EBTTestServiceStopTree::Type> StopTimming;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
