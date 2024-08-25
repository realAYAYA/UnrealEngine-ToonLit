// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPTangent.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyTangent::UDMMaterialPropertyTangent()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Tangent),
		EDMValueType::VT_Float3_XYZ)
{
}

bool UDMMaterialPropertyTangent::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
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

UMaterialExpression* UDMMaterialPropertyTangent::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}
