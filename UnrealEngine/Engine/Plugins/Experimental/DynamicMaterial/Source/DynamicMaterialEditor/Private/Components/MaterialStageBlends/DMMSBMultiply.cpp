// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBMultiply.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendMultiply"

UDMMaterialStageBlendMultiply::UDMMaterialStageBlendMultiply()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendMultiply", "Multiply"),
		"DM_Blend_Multiply",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Multiply.MF_DM_Blend_Multiply'")
	)
{
}

#undef LOCTEXT_NAMESPACE
