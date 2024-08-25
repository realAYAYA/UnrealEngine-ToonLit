// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPBaseColor.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyBaseColor::UDMMaterialPropertyBaseColor()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::BaseColor),
		EDMValueType::VT_Float3_RGB)
{
}

bool UDMMaterialPropertyBaseColor::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	return (InModelEditorOnlyData.GetShadingModel() != EDMMaterialShadingModel::Unlit);
}

UMaterialExpression* UDMMaterialPropertyBaseColor::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}
