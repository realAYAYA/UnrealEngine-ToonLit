// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSetMaterialAttributes.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionSetMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FExpressionInput> Inputs;

	UPROPERTY(EditAnywhere, Category=MaterialAttributes)
	TArray<FGuid> AttributeSetTypes;
#if WITH_EDITOR
	TArray<FGuid> PreEditAttributeSetTypes;
#endif

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual TArrayView<FExpressionInput*> GetInputsView()override;
	virtual FExpressionInput* GetInput(int32 InputIndex)override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override {return true;}
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return true;}
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;

	uint64 GetConnectedInputs() const;
#endif
	//~ End UMaterialExpression Interface
};
