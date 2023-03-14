// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionBumpOffset.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionBumpOffset : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	// Outputs: Coordinate + Eye.xy * (Height - ReferencePlane) * HeightRatio
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinate;

	UPROPERTY()
	FExpressionInput Height;

	UPROPERTY(meta=(RequiredInput = "false"))
	FExpressionInput HeightRatioInput;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionBumpOffset, meta=(OverridingInputProperty = "HeightRatioInput"))
	float HeightRatio;    // Perceived height as a fraction of width.

	UPROPERTY(EditAnywhere, Category=MaterialExpressionBumpOffset)
	float ReferencePlane;    // Height at which no offset is applied.

	/** only used if Coordinate is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionBumpOffset, meta = (OverridingInputProperty = "Coordinate"))
	uint32 ConstCoordinate;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};



