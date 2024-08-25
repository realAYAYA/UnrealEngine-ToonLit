// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Expression_InputParam.h"
#include "Engine/Texture.h"
#include "TG_Texture.h"

#include "TG_Expression_Texture.generated.h"

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Texture : public UTG_Expression_InputParam
{
	GENERATED_BODY()
	TG_DECLARE_INPUT_PARAM_EXPRESSION(TG_Category::Input);

public:

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;

	virtual bool Validate(MixUpdateCyclePtr	Cycle) override;
	
	// The output of the node, which is the loaded texture asset
	UPROPERTY(meta = (TGType = "TG_Output", PinDisplayName = "", HideInnerPropertiesInNode))
	FTG_Texture Output;

	// The source asset to be used to generate the Output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Input", TGPinNotConnectable) )
	TObjectPtr<UTexture> Source;

	// The input texture that was loaded from the asset
	UPROPERTY(meta = (TGType = "TG_InputParam"))
	FTG_Texture Texture;

	class ULayerChannel* Channel;
	virtual FTG_Name GetDefaultName() const override { return TEXT("Texture"); }
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Makes an existing texture asset available. It is automatically exposed as a graph input parameter.")); }

	virtual void SetAsset(UObject* Asset) override;
	virtual bool CanHandleAsset(UObject* Asset) override;
};

