// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_Base.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.generated.h"


UCLASS(MinimalAPI, Experimental)
class UAnimGraphNode_PoseSearchHistoryCollector : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_PoseSearchHistoryCollector Node;

	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
};
