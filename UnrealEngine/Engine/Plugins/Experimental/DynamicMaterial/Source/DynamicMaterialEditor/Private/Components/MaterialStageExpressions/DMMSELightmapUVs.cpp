// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSELightmapUVs.h"
#include "Materials/MaterialExpressionLightmapUVs.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionLightmapUVs"

UDMMaterialStageExpressionLightmapUVs::UDMMaterialStageExpressionLightmapUVs()
	: UDMMaterialStageExpression(
		LOCTEXT("LightmapUVs", "Lightmap UVs"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionLightmapUVs"))
	)
{
	Menus.Add(EDMExpressionMenu::Texture);

	OutputConnectors.Add({0, LOCTEXT("UV", "UV"), EDMValueType::VT_Float2});
}

#undef LOCTEXT_NAMESPACE
