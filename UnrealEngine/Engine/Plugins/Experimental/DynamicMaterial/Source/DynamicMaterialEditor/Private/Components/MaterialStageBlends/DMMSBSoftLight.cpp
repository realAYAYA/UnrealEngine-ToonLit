// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBSoftLight.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendSoftLight"

UDMMaterialStageBlendSoftLight::UDMMaterialStageBlendSoftLight()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendSoftLight", "SoftLight"),
		"DM_Blend_SoftLight",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_SoftLight.MF_DM_Blend_SoftLight'")
	)
{
}

#undef LOCTEXT_NAMESPACE
