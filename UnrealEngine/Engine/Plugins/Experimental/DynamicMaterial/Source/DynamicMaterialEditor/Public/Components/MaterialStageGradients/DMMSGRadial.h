// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageGradient.h"
#include "DMMSGRadial.generated.h"

struct FDMMaterialBuildState;
class UMaterialExpression;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageGradientRadial : public UDMMaterialStageGradient
{
	GENERATED_BODY()

public:
	UDMMaterialStageGradientRadial();

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
};
