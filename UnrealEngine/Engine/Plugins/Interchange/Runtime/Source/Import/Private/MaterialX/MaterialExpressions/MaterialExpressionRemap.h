// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"

#include "MaterialExpressionRemap.generated.h"

/**
 * A material expression that Remap a value from one range to another.
 */
UCLASS(collapsecategories, hidecategories = Object, MinimalAPI, meta = (Private))
class UMaterialExpressionMaterialXRemap : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MinDefault1' if not specified"))
	FExpressionInput InputLow;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MaxDefault1' if not specified"))
	FExpressionInput InputHigh;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MinDefault2' if not specified"))
	FExpressionInput TargetLow;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'MaxDefault1' if not specified"))
	FExpressionInput TargetHigh;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionClamp, meta = (OverridingInputProperty = "InputLow"))
	float InputLowDefault;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionClamp, meta = (OverridingInputProperty = "InputHigh"))
	float InputHighDefault;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionClamp, meta = (OverridingInputProperty = "TargetLow"))
	float TargetLowDefault;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionClamp, meta = (OverridingInputProperty = "TargetHigh"))
	float TargetHighDefault;

	//~ Begin UMaterialExpressionMaterialX Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(class FMaterialHLSLGenerator& Generator, class UE::HLSLTree::FScope& Scope, int32 OutputIndex, class UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpressionMaterialX Interface
};

