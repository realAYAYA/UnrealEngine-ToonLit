// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTreeGraphNode.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "BehaviorTreeGraphNode_Root.generated.h"

class UObject;

/** Root node of this behavior tree, holds Blackboard data */
UCLASS()
class BEHAVIORTREEEDITOR_API UBehaviorTreeGraphNode_Root : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category="AI|BehaviorTree")
	TObjectPtr<class UBlackboardData> BlackboardAsset;

	virtual void PostPlacedNewNode() override;
	virtual void AllocateDefaultPins() override;
	virtual bool CanDuplicateNode() const override { return false; }
	virtual bool CanUserDeleteNode() const override{ return false; }
	virtual bool HasErrors() const override { return false; }
	virtual bool RefreshNodeClass() override { return false; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	/** gets icon resource name for title bar */
	virtual FName GetNameIcon() const override;
	virtual FText GetTooltipText() const override;

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual FText GetDescription() const override;

	virtual FLinearColor GetBackgroundColor(bool bIsActiveForDebugger) const override;

	/** notify behavior tree about blackboard change */
	void UpdateBlackboard();
};
