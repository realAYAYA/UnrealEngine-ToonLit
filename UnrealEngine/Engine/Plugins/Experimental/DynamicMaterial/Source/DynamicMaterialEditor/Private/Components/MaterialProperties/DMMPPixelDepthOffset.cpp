// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPPixelDepthOffset.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyPixelDepthOffset::UDMMaterialPropertyPixelDepthOffset()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::PixelDepthOffset),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyPixelDepthOffset::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Opaque:
		case EBlendMode::BLEND_Masked:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertyPixelDepthOffset::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 0.f);
}
