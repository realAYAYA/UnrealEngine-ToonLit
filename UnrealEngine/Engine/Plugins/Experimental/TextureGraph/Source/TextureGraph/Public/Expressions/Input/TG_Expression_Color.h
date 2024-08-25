// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Expression_InputParam.h"

#include "TG_Expression_Color.generated.h"

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Color : public UTG_Expression_InputParam
{
	GENERATED_BODY()
	TG_DECLARE_INPUT_PARAM_EXPRESSION(TG_Category::Input);

public:

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;

	// The color value
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_InputParam", PinDisplayName = ""))
    FLinearColor Color = FLinearColor::Black;
    
	// The output of the node, which is the color value
    UPROPERTY(meta = (TGType = "TG_Output", PinDisplayName = ""))
	FLinearColor ValueOut = FLinearColor::Black;


	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;
	virtual FTG_Name GetDefaultName() const override { return TEXT("Color"); }
	
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Makes an RGBA color available. It is automatically exposed as a graph input parameter.")); } 
};

