// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBAdd.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendAdd"

UDMMaterialStageBlendAdd::UDMMaterialStageBlendAdd()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendAdd", "Add"), 
		"DM_Blend_Add", 
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Add.MF_DM_Blend_Add'")
	)
{
}

#undef LOCTEXT_NAMESPACE
