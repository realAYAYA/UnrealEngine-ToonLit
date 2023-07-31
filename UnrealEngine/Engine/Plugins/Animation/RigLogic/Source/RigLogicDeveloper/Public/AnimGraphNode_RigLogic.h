// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode_RigLogic.h"
#include "AnimGraphNode_RigLogic.generated.h"

UCLASS(meta = (Keywords = "Rig Logic Animation Node"))
class RIGLOGICDEVELOPER_API UAnimGraphNode_RigLogic : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_RigLogic Node;

public:
	FText GetTooltipText() const;
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const;

protected:
	void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog);
};
