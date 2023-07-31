// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"

#include "MaterialExpressionRampTopBottom.generated.h"

/**
 * A material expression that computes a top-to-bottom bilinear value ramp.
 */
UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionRampTopBottom : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

		UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
		FExpressionInput Coordinates;

	UPROPERTY()
		FExpressionInput A;

	UPROPERTY()
		FExpressionInput B;

	/** only used if Coordinates is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSample, meta = (OverridingInputProperty = "Coordinates"))
		uint8 ConstCoordinate;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};

