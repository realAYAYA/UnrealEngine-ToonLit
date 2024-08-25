// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPSpecular.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertySpecular::UDMMaterialPropertySpecular()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Specular),
		EDMValueType::VT_Float3_RGB)
{
}

bool UDMMaterialPropertySpecular::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	if (InModelEditorOnlyData.GetShadingModel() == EDMMaterialShadingModel::Unlit)
	{
		return false;
	}

	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Opaque:
		case EBlendMode::BLEND_Masked:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertySpecular::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}
