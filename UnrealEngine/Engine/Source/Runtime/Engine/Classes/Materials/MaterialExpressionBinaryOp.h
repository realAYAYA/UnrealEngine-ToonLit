// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "MaterialExpressionBinaryOp.generated.h"

UCLASS(abstract, MinimalAPI)
class UMaterialExpressionBinaryOp : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstA' if not specified"))
	FExpressionInput A;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstB' if not specified"))
	FExpressionInput B;

	/** only used if A is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionAdd, meta = (OverridingInputProperty = "A"))
	float ConstA;

	/** only used if B is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionAdd, meta = (OverridingInputProperty = "B"))
	float ConstB;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual UE::HLSLTree::EOperation GetBinaryOp() const PURE_VIRTUAL(UMaterialExpressionBinaryOp::GetBinaryOp, return UE::HLSLTree::EOperation::None;);
	virtual FText GetKeywords() const override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

};

UCLASS(MinimalAPI, meta = (MaterialNewHLSLGenerator))
class UMaterialExpressionLess : public UMaterialExpressionBinaryOp
{
	GENERATED_UCLASS_BODY()
#if WITH_EDITOR
	virtual UE::HLSLTree::EOperation GetBinaryOp() const override { return UE::HLSLTree::EOperation::Less; }
#endif // WITH_EDITOR
};
