// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPRoughness.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyRoughness::UDMMaterialPropertyRoughness()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Roughness),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyRoughness::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
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

UMaterialExpression* UDMMaterialPropertyRoughness::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 0.f);
}
