// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBOverlay.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendOverlay"

UDMMaterialStageBlendOverlay::UDMMaterialStageBlendOverlay()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendOverlay", "Overlay"),
		"DM_Blend_Overlay",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Overlay.MF_DM_Blend_Overlay'")
	)
{
}

#undef LOCTEXT_NAMESPACE
