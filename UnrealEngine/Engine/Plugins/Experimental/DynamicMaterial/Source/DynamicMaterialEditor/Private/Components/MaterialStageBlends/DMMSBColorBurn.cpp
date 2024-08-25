// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBColorBurn.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendColorBurn"

UDMMaterialStageBlendColorBurn::UDMMaterialStageBlendColorBurn()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendColorBurn", "ColorBurn"),
		"DM_Blend_ColorBurn",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_ColorBurn.MF_DM_Blend_ColorBurn'")
	)
{
}

#undef LOCTEXT_NAMESPACE
