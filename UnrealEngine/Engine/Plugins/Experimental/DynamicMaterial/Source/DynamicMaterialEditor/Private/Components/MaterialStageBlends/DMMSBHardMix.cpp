// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBHardMix.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendHardMix"

UDMMaterialStageBlendHardMix::UDMMaterialStageBlendHardMix()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendHardMix", "HardMix"),
		"DM_Blend_HardMix",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_HardMix.MF_DM_Blend_HardMix'")
	)
{
}

#undef LOCTEXT_NAMESPACE
