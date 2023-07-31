// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputEditorSettings.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "EnhancedInputEditorSubsystem.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputEditorSettings)

namespace UE::EnhancedInput::Private
{
	namespace ConsoleVariables
	{
		static bool bShouldLogAllInputs = false;
		static FAutoConsoleVariableRef CVarShouldLogAllInputs(
			TEXT("EnhancedEditorInput.bShouldLogAllInputs"),
			bShouldLogAllInputs,
			TEXT("Should each InputKey call be logged?"),
			ECVF_Default);

		static bool bAutomaticallyStartConsumingInput = false;
		static FAutoConsoleVariableRef CVarAutomaticallyStartConsumingInput(
			TEXT("EnhancedEditorInput.bAutomaticallyStartConsumingInput"),
			bAutomaticallyStartConsumingInput,
			TEXT("Should the UEnhancedInputEditorSubsystem be started as soon as it is inialized?"),
			ECVF_Default);	
	}
}

////////////////////////////////////////////////////////////////////////
// UEnhancedInputEditorProjectSettings
UEnhancedInputEditorProjectSettings::UEnhancedInputEditorProjectSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, DefaultEditorInputClass(UEnhancedPlayerInput::StaticClass())
{
}

////////////////////////////////////////////////////////////////////////
// UEnhancedInputEditorSettings
UEnhancedInputEditorSettings::UEnhancedInputEditorSettings()
	: bLogAllInput(false)
	, bAutomaticallyStartConsumingInput(false)
{
}

