// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionDistanceFieldsRenderingSwitch.generated.h"

/** Material output expression to switch logic according to whether distance fields renderering is supported on this project and feature level. */
UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionDistanceFieldsRenderingSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Used if distance fields renderering is not supported. */
	UPROPERTY()
	FExpressionInput No;

	/** Used if distance fields renderering is supported. */
	UPROPERTY()
	FExpressionInput Yes;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Unknown; }
	virtual uint32 GetOutputType(int32 OutputIndex) override { return MCT_Unknown; }
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
