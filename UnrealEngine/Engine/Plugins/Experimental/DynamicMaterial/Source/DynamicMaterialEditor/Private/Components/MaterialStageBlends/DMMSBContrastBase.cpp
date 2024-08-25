// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBContrastBase.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendContrastBase"

UDMMaterialStageBlendContrastBase::UDMMaterialStageBlendContrastBase()
	: UDMMaterialStageBlendContrastBase(LOCTEXT("BlendContrastBase", "Contrast Base"))
{
}

UDMMaterialStageBlendContrastBase::UDMMaterialStageBlendContrastBase(const FText& InName)
	: UDMMaterialStageBlend(InName)
{
}

#undef LOCTEXT_NAMESPACE
