// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputPlatformSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputDeveloperSettings)

UEnhancedInputDeveloperSettings::UEnhancedInputDeveloperSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	PlatformSettings.Initialize(UEnhancedInputPlatformSettings::StaticClass());
}
