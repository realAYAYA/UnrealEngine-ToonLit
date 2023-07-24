// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionQualitySwitch.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionQualitySwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Default connection, used when a specific quality level input is missing. */
	UPROPERTY()
	FExpressionInput Default;

	UPROPERTY()
	FExpressionInput Inputs[EMaterialQualityLevel::Num];

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 InputIndex) override { return MCT_Unknown; }

	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

#if WITH_EDITOR
protected:
	FExpressionInput* GetEffectiveInput(class FMaterialCompiler* Compiler);
#endif
};
