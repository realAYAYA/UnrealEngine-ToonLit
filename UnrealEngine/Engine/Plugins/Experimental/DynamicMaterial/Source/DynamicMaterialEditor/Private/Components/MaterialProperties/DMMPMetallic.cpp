// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPMetallic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyMetallic::UDMMaterialPropertyMetallic()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Metallic),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyMetallic::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
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

UMaterialExpression* UDMMaterialPropertyMetallic::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 0.f);
}
