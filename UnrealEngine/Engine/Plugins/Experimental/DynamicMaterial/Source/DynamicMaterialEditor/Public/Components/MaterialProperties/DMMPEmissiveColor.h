// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialProperty.h"
#include "DMMPEmissiveColor.generated.h"

class UDynamicMaterialModelEditorOnlyData;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialPropertyEmissiveColor : public UDMMaterialProperty
{
	GENERATED_BODY()

public:
	UDMMaterialPropertyEmissiveColor();

	//~ Begin UDMMaterialProperty
	virtual bool IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const override;
	virtual UMaterialExpression* GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	//~ End UDMMaterialProperty
};
