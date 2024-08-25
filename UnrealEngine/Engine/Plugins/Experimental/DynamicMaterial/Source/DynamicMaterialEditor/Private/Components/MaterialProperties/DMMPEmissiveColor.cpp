// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPEmissiveColor.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyEmissiveColor::UDMMaterialPropertyEmissiveColor()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::EmissiveColor),
		EDMValueType::VT_Float3_RGB)
{
}

bool UDMMaterialPropertyEmissiveColor::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_AlphaHoldout:
			return false;

		default:
			return true;
	}
}

UMaterialExpression* UDMMaterialPropertyEmissiveColor::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}
