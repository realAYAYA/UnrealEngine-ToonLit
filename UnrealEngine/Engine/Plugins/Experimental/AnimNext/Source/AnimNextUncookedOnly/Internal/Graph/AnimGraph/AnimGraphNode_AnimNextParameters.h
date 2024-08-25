// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimGraph/AnimNode_AnimNextParameters.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AnimNextParameters.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_AnimNextParameters : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_AnimNextParameters Node;

	// UAnimGraphNode_Base interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetRequiredExtensions(TArray<TSubclassOf<UAnimBlueprintExtension>>& OutExtensions) const override;
};