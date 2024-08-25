// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"

#include "DMMSEMathBase.generated.h"

class UDMMaterialStageInput;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionMathBase : public UDMMaterialStageExpression
{
	GENERATED_BODY()

public:

	UDMMaterialStageExpressionMathBase(const FText& InName, TSubclassOf<UMaterialExpression> InClass);

	virtual bool CanInputAcceptType(int32 InInputIndex, EDMValueType InValueType) const override;

	virtual void Update(EDMUpdateType InUpdateType) override;

	virtual void AddDefaultInput(int32 InInputIndex) const override;

	//~ Begin UDMMaterialStageSource
	virtual bool UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, UMaterialExpression*& OutMaterialExpression,
		int32& OutputIndex) override;
	//~ End UDMMaterialStageSource

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bSingleChannelOnly;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 VariadicInputCount;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bAllowSingleFloatMatch;

	UDMMaterialStageExpressionMathBase();

	void SetupInputs(int32 InCount);

	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr) override;

};
