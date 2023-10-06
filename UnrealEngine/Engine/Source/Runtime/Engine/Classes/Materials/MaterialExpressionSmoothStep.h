// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectMacros.h"

#include "MaterialExpressionSmoothStep.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionSmoothStep : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstMin' if not specified"))
	FExpressionInput Min;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstMax' if not specified"))
	FExpressionInput Max;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstValue' if not specified"))
	FExpressionInput Value;

	/** only used if Min is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSmoothStep, meta = (OverridingInputProperty = "Min"))
	float ConstMin;

	/** only used if Max is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSmoothStep, meta = (OverridingInputProperty = "Max"))
	float ConstMax;

	/** only used if Value is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSmoothStep, meta = (OverridingInputProperty = "Value"))
	float ConstValue;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override { return FText::FromString(TEXT("SmoothStep")); }
	virtual FText GetCreationName() const override { return FText::FromString(TEXT("SmoothStep")); }

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};
