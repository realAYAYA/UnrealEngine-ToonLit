// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "RHIDefinitions.h"
#include "MaterialExpressionShaderStageSwitch.generated.h"

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionShaderStageSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	static inline bool ShouldUsePixelShaderInput(EShaderFrequency InShaderFrequency)
	{
		// Compute is considered pixel shader here, as the majority of material behavior that depends on pixel shader also allows compute shader
		return (InShaderFrequency == SF_Pixel || InShaderFrequency == SF_Compute);
	}

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Input will be used in pixel (or compute) shader stages"))
	FExpressionInput PixelShader;

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Input will be in vertex (or tessellation) shader stages"))
	FExpressionInput VertexShader;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual FText GetKeywords() const override { return FText::FromString(TEXT("vertex pixel shader")); }

	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
