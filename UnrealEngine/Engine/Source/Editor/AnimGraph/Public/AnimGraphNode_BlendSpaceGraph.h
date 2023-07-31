// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_BlendSpaceGraphBase.h"
#include "AnimNodes/AnimNode_BlendSpaceGraph.h"
#include "AnimGraphNode_BlendSpaceGraph.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FCompilerResultsLog;

UCLASS()
class ANIMGRAPH_API UAnimGraphNode_BlendSpaceGraph : public UAnimGraphNode_BlendSpaceGraphBase
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_BlendSpaceGraph Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	
	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	// UAnimGraphNode_Base interface
	virtual void BakeDataDuringCompilation(FCompilerResultsLog& MessageLog) override;
};
