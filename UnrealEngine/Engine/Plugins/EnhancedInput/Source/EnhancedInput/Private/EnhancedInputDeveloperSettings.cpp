// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputPlatformSettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EnhancedPlayerInput.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputDeveloperSettings)

namespace UE::EnhancedInput::Private
{
	namespace ConsoleVariables
	{
		static bool bShouldLogAllWorldSubsystemInputs = false;
		static FAutoConsoleVariableRef CVarShouldLogAllWorldSubsystemInputs(
			TEXT("EnhancedInput.bShouldLogAllWorldSubsystemInputs"),
			bShouldLogAllWorldSubsystemInputs,
			TEXT("Should each InputKey call to the World subsystem be logged?"),
			ECVF_Cheat);
	}
}

UEnhancedInputDeveloperSettings::UEnhancedInputDeveloperSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, UserSettingsClass(UEnhancedInputUserSettings::StaticClass())
	, DefaultPlayerMappableKeyProfileClass(UEnhancedPlayerMappableKeyProfile::StaticClass())
	, DefaultWorldInputClass(UEnhancedPlayerInput::StaticClass())
	, bSendTriggeredEventsWhenInputIsFlushed(true)
	, bEnableUserSettings(false)
	, bEnableDefaultMappingContexts(true)
	, bShouldOnlyTriggerLastActionInChord(true)
	, bLogOnDeprecatedConfigUsed(true)
	, bEnableWorldSubsystem(false)
	, bShouldLogAllWorldSubsystemInputs(false)
{
	PlatformSettings.Initialize(UEnhancedInputPlatformSettings::StaticClass());
}