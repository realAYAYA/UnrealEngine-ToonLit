// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPAnisotropy.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyAnisotropy::UDMMaterialPropertyAnisotropy()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Anisotropy),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyAnisotropy::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
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

UMaterialExpression* UDMMaterialPropertyAnisotropy::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 1.f);
}
