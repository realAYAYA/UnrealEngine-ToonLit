// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "MaterialExpressionStaticSwitchParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionStaticSwitchParameter : public UMaterialExpressionStaticBoolParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput A;

	UPROPERTY()
	FExpressionInput B;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 OutputIndex) override {return MCT_Unknown;}
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;

	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

#if WITH_EDITOR
protected:
	FExpressionInput* GetEffectiveInput(class FMaterialCompiler* Compiler);
#endif
};

