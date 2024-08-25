// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBExclusion.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendExclusion"

UDMMaterialStageBlendExclusion::UDMMaterialStageBlendExclusion()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendExclusion", "Exclusion"),
		"DM_Blend_Exclusion",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Exclusion.MF_DM_Blend_Exclusion'")
	)
{
}

#undef LOCTEXT_NAMESPACE
