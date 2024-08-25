// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBColorDodge.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendColorDodge"

UDMMaterialStageBlendColorDodge::UDMMaterialStageBlendColorDodge()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendColorDodge", "ColorDodge"),
		"DM_Blend_ColorDodge",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_ColorDodge.MF_DM_Blend_ColorDodge'")
	)
{
}

#undef LOCTEXT_NAMESPACE
