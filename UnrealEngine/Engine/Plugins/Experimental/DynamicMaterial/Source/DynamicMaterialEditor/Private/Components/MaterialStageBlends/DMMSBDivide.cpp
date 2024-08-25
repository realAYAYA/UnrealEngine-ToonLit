// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBDivide.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendDivide"

UDMMaterialStageBlendDivide::UDMMaterialStageBlendDivide()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendDivide", "Divide"),
		"DM_Blend_Divide",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Divide.MF_DM_Blend_Divide'")
	)
{
}

#undef LOCTEXT_NAMESPACE
