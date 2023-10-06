// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionIf.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionIf : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput A;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstB' if not specified"))
	FExpressionInput B;

	UPROPERTY()
	FExpressionInput AGreaterThanB;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'A > B' if not specified"))
	FExpressionInput AEqualsB;

	UPROPERTY()
	FExpressionInput ALessThanB;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionIf, meta = (ShowAsInputPin = "Advanced"))
	float EqualsThreshold;

	/** only used if B is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionIf, meta = (OverridingInputProperty = "B"))
	float ConstB;

	UPROPERTY()
	float ConstAEqualsB_DEPRECATED;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};



