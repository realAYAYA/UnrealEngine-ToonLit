// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBScreen.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendScreen"

UDMMaterialStageBlendScreen::UDMMaterialStageBlendScreen()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendScreen", "Screen"),
		"DM_Blend_Screen",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Screen.MF_DM_Blend_Screen'")
	)
{
}

#undef LOCTEXT_NAMESPACE
