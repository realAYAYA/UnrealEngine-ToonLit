// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPOpacity.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyOpacity::UDMMaterialPropertyOpacity()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Opacity),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyOpacity::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Translucent:
		case EBlendMode::BLEND_Additive:
		case EBlendMode::BLEND_AlphaComposite:
		case EBlendMode::BLEND_AlphaHoldout:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertyOpacity::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 1.f);
}
