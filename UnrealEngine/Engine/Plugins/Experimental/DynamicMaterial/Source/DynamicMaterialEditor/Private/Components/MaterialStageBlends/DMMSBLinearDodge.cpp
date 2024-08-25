// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBLinearDodge.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendLinearDodge"

UDMMaterialStageBlendLinearDodge::UDMMaterialStageBlendLinearDodge()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendLinearDodge", "LinearDodge"),
		"DM_Blend_LinearDodge",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_LinearDodge.MF_DM_Blend_LinearDodge'")
	)
{
}

#undef LOCTEXT_NAMESPACE
