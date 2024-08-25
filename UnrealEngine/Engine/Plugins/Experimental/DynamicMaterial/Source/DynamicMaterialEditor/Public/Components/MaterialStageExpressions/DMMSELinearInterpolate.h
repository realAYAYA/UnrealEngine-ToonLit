// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageExpressions/DMMSEMathBase.h"
#include "DMMSELinearInterpolate.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionLinearInterpolate : public UDMMaterialStageExpressionMathBase
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionLinearInterpolate();

	virtual void AddDefaultInput(int32 InInputIndex) const override;
};
