// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDesaturation.generated.h"

UCLASS(MinimalAPI, collapsecategories, hidecategories=Object)
class UMaterialExpressionDesaturation : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	// Outputs: Lerp(Input,dot(Input,LuminanceFactors)),Fraction)
	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY()
	FExpressionInput Fraction;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionDesaturation, Meta = (ShowAsInputPin = "Advanced"))
	FLinearColor LuminanceFactors;    // Color component factors for converting a color to greyscale.


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override
	{
		OutCaptions.Add(TEXT("Desaturation"));
	}
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};



