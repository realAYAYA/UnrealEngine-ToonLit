// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialProperty.h"
#include "DMMPRoughness.generated.h"

class UDynamicMaterialModelEditorOnlyData;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialPropertyRoughness : public UDMMaterialProperty
{
	GENERATED_BODY()

public:
	UDMMaterialPropertyRoughness();

	//~ Begin UDMMaterialProperty
	virtual bool IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const override;
	virtual UMaterialExpression* GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	//~ End UDMMaterialProperty
};
