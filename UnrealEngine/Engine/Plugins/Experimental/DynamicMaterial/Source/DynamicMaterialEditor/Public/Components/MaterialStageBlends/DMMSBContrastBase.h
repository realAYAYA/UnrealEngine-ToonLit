// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageBlend.h"
#include "DMMSBContrastBase.generated.h"

class UMaterialExpression;
struct FDMMaterialBuildState;

UCLASS(BlueprintType, Abstract, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageBlendContrastBase : public UDMMaterialStageBlend
{
	GENERATED_BODY()

public:
	UDMMaterialStageBlendContrastBase(const FText& InName);

protected:
	UDMMaterialStageBlendContrastBase();
};
