// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBLightenColor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendLightenColor"

UDMMaterialStageBlendLightenColor::UDMMaterialStageBlendLightenColor()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendLightenColor", "LightenColor"),
		"DM_Blend_LightenColor",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_LightenColor.MF_DM_Blend_LightenColor'")
	)
{
}

#undef LOCTEXT_NAMESPACE
