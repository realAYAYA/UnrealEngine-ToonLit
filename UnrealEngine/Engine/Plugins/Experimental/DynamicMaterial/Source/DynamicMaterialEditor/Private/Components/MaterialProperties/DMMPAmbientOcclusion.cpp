// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPAmbientOcclusion.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyAmbientOcclusion::UDMMaterialPropertyAmbientOcclusion()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::AmbientOcclusion),
		EDMValueType::VT_Float1)
{
}

bool UDMMaterialPropertyAmbientOcclusion::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	return (InModelEditorOnlyData.GetShadingModel() != EDMMaterialShadingModel::Unlit);
}

UMaterialExpression* UDMMaterialPropertyAmbientOcclusion::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::ZeroVector);
}
