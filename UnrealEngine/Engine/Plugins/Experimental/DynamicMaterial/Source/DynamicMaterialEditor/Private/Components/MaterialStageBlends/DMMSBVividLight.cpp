// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBVividLight.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendVividLight"

UDMMaterialStageBlendVividLight::UDMMaterialStageBlendVividLight()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendVividLight", "VividLight"),
		"DM_Blend_VividLight",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_VividLight.MF_DM_Blend_VividLight'")
	)
{
}

#undef LOCTEXT_NAMESPACE
