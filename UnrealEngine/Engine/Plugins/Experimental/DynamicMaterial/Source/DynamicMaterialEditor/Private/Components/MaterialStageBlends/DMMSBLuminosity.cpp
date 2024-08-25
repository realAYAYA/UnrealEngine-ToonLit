// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBLuminosity.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendLuminosity"

UDMMaterialStageBlendLuminosity::UDMMaterialStageBlendLuminosity()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendLuminosity", "Luminosity"), 
		"DM_Blend_Luminosity", 
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Luminosity.MF_DM_Blend_Luminosity'")
	)
{
}

#undef LOCTEXT_NAMESPACE
