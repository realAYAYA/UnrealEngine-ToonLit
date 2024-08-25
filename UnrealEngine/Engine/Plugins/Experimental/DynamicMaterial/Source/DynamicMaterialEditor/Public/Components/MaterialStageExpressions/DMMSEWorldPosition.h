// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "DMMSEWorldPosition.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionWorldPosition : public UDMMaterialStageExpression
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionWorldPosition();

	virtual void AddExpressionProperties(const TArray<UMaterialExpression*>& InExpressions) const override;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TEnumAsByte<EWorldPositionIncludedOffsets> WorldPositionShaderOffset;
};
