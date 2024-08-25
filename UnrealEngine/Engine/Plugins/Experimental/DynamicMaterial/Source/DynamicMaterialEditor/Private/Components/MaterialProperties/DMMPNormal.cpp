// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPNormal.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyNormal::UDMMaterialPropertyNormal()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Normal),
		EDMValueType::VT_Float3_XYZ)
{
}

bool UDMMaterialPropertyNormal::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
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

UMaterialExpression* UDMMaterialPropertyNormal::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}
