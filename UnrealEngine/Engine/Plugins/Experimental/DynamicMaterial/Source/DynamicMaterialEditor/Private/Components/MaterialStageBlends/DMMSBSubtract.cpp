// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBSubtract.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendSubtract"

UDMMaterialStageBlendSubtract::UDMMaterialStageBlendSubtract()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendSubtract", "Subtract"),
		"DM_Blend_Subtract",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Subtract.MF_DM_Blend_Subtract'")
	)
{
}

#undef LOCTEXT_NAMESPACE
