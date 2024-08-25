// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageExpressions/DMMSETextureSampleBase.h"
#include "DMMSEParticleSubUV.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionParticleSubUV : public UDMMaterialStageExpressionTextureSampleBase
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionParticleSubUV();
};
