// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionBlendMaterialAttributes.generated.h"

UENUM()
namespace EMaterialAttributeBlend
{
	enum Type : int
	{
		Blend,
		UseA,
		UseB
	};
}

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionBlendMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
 	FMaterialAttributesInput A;

	UPROPERTY()
 	FMaterialAttributesInput B;

	UPROPERTY()
	FExpressionInput Alpha;

	// Optionally skip blending attributes of this type.
	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TEnumAsByte<EMaterialAttributeBlend::Type> PixelAttributeBlendType;

	// Optionally skip blending attributes of this type.
	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TEnumAsByte<EMaterialAttributeBlend::Type> VertexAttributeBlendType;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override {return true;}
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return true;}
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override {return InputIndex == 2 ? MCT_Float1 : MCT_MaterialAttributes;}
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
