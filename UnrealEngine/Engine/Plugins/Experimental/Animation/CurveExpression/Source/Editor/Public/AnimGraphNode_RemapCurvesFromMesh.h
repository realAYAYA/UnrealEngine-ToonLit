// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"

#include "AnimNode_RemapCurvesFromMesh.h"

#include "AnimGraphNode_RemapCurvesFromMesh.generated.h"


UCLASS(MinimalAPI)
class UAnimGraphNode_RemapCurvesFromMesh :
	public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_RemapCurvesFromMesh Node;

	bool CanVerifyExpressions() const;
	void VerifyExpressions();

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	FText GetMenuCategory() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual bool UsingCopyPoseFromMesh() const override { return true; }
	virtual void GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const override;
	// End of UAnimGraphNode_Base interface
	
private:
	FAnimNode_RemapCurvesFromMesh* GetDebuggedNode() const;
	
};
