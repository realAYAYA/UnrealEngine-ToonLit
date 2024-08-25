// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPOpacityMask.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyOpacityMask::UDMMaterialPropertyOpacityMask()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::OpacityMask),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyOpacityMask::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	return (InModelEditorOnlyData.GetBlendMode() == EBlendMode::BLEND_Masked);
}

UMaterialExpression* UDMMaterialPropertyOpacityMask::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, 1.f);
}
