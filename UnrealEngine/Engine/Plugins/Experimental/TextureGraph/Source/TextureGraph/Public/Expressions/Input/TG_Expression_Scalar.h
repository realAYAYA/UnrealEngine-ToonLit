// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Expression_InputParam.h"

#include "TG_Expression_Scalar.generated.h"

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Scalar : public UTG_Expression_InputParam
{
	GENERATED_BODY()
	TG_DECLARE_INPUT_PARAM_EXPRESSION(TG_Category::Input);

public:

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;

	// The floating point constant
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_InputParam", PinDisplayName = ""))
		float Scalar = 1.0f;

	// The output of the node, which is the floating point constant
	UPROPERTY(meta = (TGType = "TG_Output", PinDisplayName = ""))
		float ValueOut = 1.0f;

	virtual FTG_Name GetDefaultName() const override { return TEXT("Scalar"); }
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Makes a single floating point value available. It is automatically exposed as a graph input parameter.")); } 
};

