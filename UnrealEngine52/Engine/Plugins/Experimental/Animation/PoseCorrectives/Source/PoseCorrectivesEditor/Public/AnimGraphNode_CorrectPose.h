// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "AnimNode_CorrectPose.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_CorrectPose.generated.h"

UCLASS(Experimental, BlueprintType)
class POSECORRECTIVESEDITOR_API UAnimGraphNode_CorrectPose : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_CorrectPose Node;

public:
	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	// End of UEdGraphNode interface
};
