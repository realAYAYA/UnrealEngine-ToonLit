// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionComponentMask.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionComponentMask : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask, Meta = (ShowAsInputPin = "Advanced"))
	uint32 R:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask, Meta = (ShowAsInputPin = "Advanced"))
	uint32 G:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask, Meta = (ShowAsInputPin = "Advanced"))
	uint32 B:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask, Meta = (ShowAsInputPin = "Advanced"))
	uint32 A:1;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override { return FText::FromString(TEXT("component mask")); }
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};



