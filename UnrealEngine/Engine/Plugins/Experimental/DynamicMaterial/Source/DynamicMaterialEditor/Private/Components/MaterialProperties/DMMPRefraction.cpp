// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPRefraction.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyRefraction::UDMMaterialPropertyRefraction()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Refraction),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyRefraction::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Translucent:
		case EBlendMode::BLEND_Additive:
		case EBlendMode::BLEND_AlphaComposite:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertyRefraction::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 0.f);
}
