// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageExpressions/DMMSETextureSampleBase.h"
#include "DMMSETextureSample.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionTextureSample : public UDMMaterialStageExpressionTextureSampleBase
{
	GENERATED_BODY()

	friend class SDMComponentEdit;

public:
	UDMMaterialStageExpressionTextureSample();
};
