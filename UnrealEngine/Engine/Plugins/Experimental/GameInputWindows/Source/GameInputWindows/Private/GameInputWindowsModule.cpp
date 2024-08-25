// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputWindowsModule.h"

#include "CoreGlobals.h"
#include "GameInputWindowsDevice.h"
#include "GameInputBaseModule.h"
#include "GameInputLogging.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"

#if GAME_INPUT_SUPPORT
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <GameInput.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif	// GAME_INPUT_SUPPORT

namespace UE::GameInput
{
	/** The name of this modular feature plugin. */
	static const FName GameInputWindowsFeatureName = TEXT("GameInputWindows");
}

FGameInputWindowsModule& FGameInputWindowsModule::Get()
{
	return FModuleManager::LoadModuleChecked<FGameInputWindowsModule>(UE::GameInput::GameInputWindowsFeatureName);
}

bool FGameInputWindowsModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(UE::GameInput::GameInputWindowsFeatureName);
}

TSharedPtr<IInputDevice> FGameInputWindowsModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
#if GAME_INPUT_SUPPORT
	if (IGameInput* GameInputPtr = FGameInputBaseModule::GetGameInput())
	{
		TSharedPtr<FGameInputWindowsInputDevice> DevicePtr = MakeShared<FGameInputWindowsInputDevice>(InMessageHandler, GameInputPtr);
		DevicePtr->Initialize();
		return DevicePtr;
	}
	else
	{
		if (IsRunningCommandlet())
		{
			// We don't care for Game Input if we are running a commandlet, like when we are cooking or running C++ automation tests
			UE_LOG(LogGameInput, Log, TEXT("[%s] GameInput interface will not be created because IsRunningCommandlet is true."), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else if (!FApp::HasProjectName())
		{
			// If there is no project name then we don't need game input either. This means we are in the project launcher
			UE_LOG(LogGameInput, Log, TEXT("[%s] GameInput interface will not be created because there is no project name."), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else if (FApp::IsUnattended())
		{
			UE_LOG(LogGameInput, Log, TEXT("[%s] GameInput interface will not be created because FApp::IsUnattended is true!"), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else if (!FApp::CanEverRender())
		{
			UE_LOG(LogGameInput, Log, TEXT("[%s] GameInput interface will not be created because FApp::CanEverRender is false!"), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else
		{
			// Otherwise, we want Game Input to be properly initialized
			UE_LOG(LogGameInput, Log, TEXT("[%s] Failed to get the IGameInput object from the base module! You will not be able to process input."), ANSI_TO_TCHAR(__FUNCTION__));	
		}
	}
#else
	UE_LOG(LogGameInput, Error, TEXT("[FGameInputWindowsModule] Failed to create a GameInput device! GAME_INPUT_SUPPORT is false!"));
#endif	// GAME_INPUT_SUPPORT

	return nullptr;
}

IMPLEMENT_MODULE(FGameInputWindowsModule, GameInputWindows)