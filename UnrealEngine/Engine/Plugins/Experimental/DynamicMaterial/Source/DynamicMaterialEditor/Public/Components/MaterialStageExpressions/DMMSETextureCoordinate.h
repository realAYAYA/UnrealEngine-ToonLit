// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "DMMSETextureCoordinate.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionTextureCoordinate : public UDMMaterialStageExpression
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionTextureCoordinate();

	virtual void AddExpressionProperties(const TArray<UMaterialExpression*>& InExpressions) const override;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 CoordinateIndex;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	float UTiling;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	float VTiling;
};
