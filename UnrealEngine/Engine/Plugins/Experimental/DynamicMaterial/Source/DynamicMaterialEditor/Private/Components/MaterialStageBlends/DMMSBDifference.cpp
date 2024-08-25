// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBDifference.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendDifference"

UDMMaterialStageBlendDifference::UDMMaterialStageBlendDifference()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendDifference", "Difference"),
		"DM_Blend_Difference",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Difference.MF_DM_Blend_Difference'")
	)
{
}

#undef LOCTEXT_NAMESPACE
