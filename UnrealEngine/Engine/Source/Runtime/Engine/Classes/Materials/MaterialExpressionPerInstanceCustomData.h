// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPerInstanceCustomData.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionPerInstanceCustomData : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstDefaultValue' if not specified; Default value is used when no instances provided."))
	FExpressionInput DefaultValue;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionPerInstanceCustomData, meta = (OverridingInputProperty = "DefaultValue"))
	float ConstDefaultValue;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionPerInstanceCustomData, meta = (ShowAsInputPin = "Advanced"))
	uint32 DataIndex;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionPerInstanceCustomData3Vector : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstDefaultValue' if not specified; Default value is used when no instances provided."))
	FExpressionInput DefaultValue;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionPerInstanceCustomData, meta = (OverridingInputProperty = "DefaultValue"))
	FLinearColor ConstDefaultValue;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionPerInstanceCustomData, meta = (ShowAsInputPin = "Advanced"))
	uint32 DataIndex;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};