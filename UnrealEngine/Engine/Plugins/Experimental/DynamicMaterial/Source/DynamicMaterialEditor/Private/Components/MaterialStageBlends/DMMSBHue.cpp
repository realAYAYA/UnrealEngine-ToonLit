// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBHue.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendHue"

UDMMaterialStageBlendHue::UDMMaterialStageBlendHue()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendHue", "Hue"), 
		"DM_Blend_Hue", 
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Hue.MF_DM_Blend_Hue'")
	)
{
}

#undef LOCTEXT_NAMESPACE
