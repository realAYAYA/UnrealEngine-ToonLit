// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBColor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendColor"

UDMMaterialStageBlendColor::UDMMaterialStageBlendColor()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendColor", "Color"), 
		"DM_Blend_Color", 
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Color.MF_DM_Blend_Color'")
	)
{
}

#undef LOCTEXT_NAMESPACE
