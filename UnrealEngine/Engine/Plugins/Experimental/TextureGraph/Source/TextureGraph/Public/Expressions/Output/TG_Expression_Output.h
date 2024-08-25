// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"
#include "TG_Texture.h"
#include "TG_OutputSettings.h"

#include "TG_Expression_Output.generated.h"

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Output : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_VARIANT_EXPRESSION(TG_Category::Output);
	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
	
	// The final generated output
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = ""))
	FTG_Variant Source = 0;

	// The final generated output value
	UPROPERTY(meta = (TGType = "TG_OutputParam", PinDisplayName = ""))
	FTG_Variant Output = 0;

	//When we will work on Node UI for FOutputSettings we will set the category as TG_Setting
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Input", CollapsableChildProperties,ShowOnlyInnerProperties, FullyExpand, NoResetToDefault, PinDisplayName = "Settings") )
	FTG_OutputSettings OutputSettings;
	
	virtual FTG_Name GetDefaultName() const override { return TEXT("Output");}
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Uses the node input as a graph output. It can be exported as a texture asset and is automatically exposed as a graph output parameter. ")); } 

	void UpdateBufferDescriptorValues();
	void InitializeOutputSettings();

	//Sets the Export bool in OutputSettings pin
	void SetExport(bool bExport);
};
