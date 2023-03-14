// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureEncodingSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureEncodingSettings)

UTextureEncodingProjectSettings::UTextureEncodingProjectSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	bFinalUsesRDO(false),
	FinalRDOLambda(30), /* OodleTex_RDOLagrangeLambda_Default */
	FinalEffortLevel(ETextureEncodeEffort::Normal),
	FinalUniversalTiling(ETextureUniversalTiling::Disabled),
	bFastUsesRDO(false),
	FastRDOLambda(30), /* OodleTex_RDOLagrangeLambda_Default */
	FastEffortLevel(ETextureEncodeEffort::Normal),
	FastUniversalTiling(ETextureUniversalTiling::Disabled),
	CookUsesSpeed(ETextureEncodeSpeed::Final),
	EditorUsesSpeed(ETextureEncodeSpeed::FinalIfAvailable)
{
}

UTextureEncodingUserSettings::UTextureEncodingUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	ForceEncodeSpeed(ETextureEncodeSpeedOverride::Disabled)
{
}

