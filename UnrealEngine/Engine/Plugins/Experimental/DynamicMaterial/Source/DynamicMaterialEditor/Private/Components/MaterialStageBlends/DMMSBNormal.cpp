// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBNormal.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendNormal"

UDMMaterialStageBlendNormal::UDMMaterialStageBlendNormal()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendNormal", "Normal"),
		"DM_Blend_Normal",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Normal.MF_DM_Blend_Normal'")
	)
{
}

#undef LOCTEXT_NAMESPACE
