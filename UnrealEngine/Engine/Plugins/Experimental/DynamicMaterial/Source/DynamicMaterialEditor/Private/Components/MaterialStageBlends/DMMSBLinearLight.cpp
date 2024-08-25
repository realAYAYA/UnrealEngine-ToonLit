// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBLinearLight.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendLinearLight"

UDMMaterialStageBlendLinearLight::UDMMaterialStageBlendLinearLight()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendLinearLight", "LinearLight"),
		"DM_Blend_LinearLight",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_LinearLight.MF_DM_Blend_LinearLight'")
	)
{
}

#undef LOCTEXT_NAMESPACE
