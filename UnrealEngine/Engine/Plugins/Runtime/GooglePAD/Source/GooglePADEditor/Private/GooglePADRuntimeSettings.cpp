// Copyright Epic Games, Inc. All Rights Reserved.

#include "GooglePADRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UGooglePADRuntimeSettings

UGooglePADRuntimeSettings::UGooglePADRuntimeSettings(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
        , bEnablePlugin(false)
        , bOnlyDistribution(true)
		, bOnlyShipping(false)
{
}