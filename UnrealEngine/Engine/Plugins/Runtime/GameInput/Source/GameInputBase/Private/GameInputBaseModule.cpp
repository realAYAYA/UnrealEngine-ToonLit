// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputBaseModule.h"

#include "CoreGlobals.h"
#include "GameInputBaseIncludes.h"
#include "GameInputKeyTypes.h"
#include "GameInputLogging.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"

#if GAME_INPUT_SUPPORT
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <GameInput.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#include "Microsoft/COMPointer.h"
#endif

#define LOCTEXT_NAMESPACE "GameInputBaseModule" 

namespace UE::GameInput
{
#if GAME_INPUT_SUPPORT
	// A singleton pointer to the base GameInput interface. 
	// This provides access to reading the input stream, device callbacks, and more. 
	static TComPtr<IGameInput> GGameInputInterface;

#if PLATFORM_WINDOWS
	// The name of the Game Input DLL file which would be needed when running on windows
	static const FString GameInputDLLPath = TEXT("GameInput.dll");
#endif	// #if PLATFORM_WINDOWS

#endif	// #if GAME_INPUT_SUPPORT	
}

FGameInputBaseModule& FGameInputBaseModule::Get()
{
	return FModuleManager::LoadModuleChecked<FGameInputBaseModule>(TEXT("GameInputBase"));
}

bool FGameInputBaseModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("GameInputBase"));
}

void FGameInputBaseModule::StartupModule()
{
	UE_LOG(LogGameInput, Log, TEXT("GameInputBase module startup..."));
	
	// We don't care for Game Input if we are running a commandlet, like when we are cooking.
	if (IsRunningCommandlet())
	{
		UE_LOG(LogGameInput, Log, TEXT("GameInputBase module is exiting because IsRunningCommandlet is true. GameInput will not be initalized."));
		return;
	}

	// If there is no project name then we don't need game input either. This means we are in the project launcher
	if (!FApp::HasProjectName())
	{
		UE_LOG(LogGameInput, Log, TEXT("GameInputBase module is exiting because there is no project name. GameInput will not be initalized."));
		return;
	}

	// Unattended app can't receive any user input, so there is no need to try and create the GameInput interface.
	if (FApp::IsUnattended())
	{
		UE_LOG(LogGameInput, Log, TEXT("GameInputBase module is exiting because it is unattended (FApp::IsUnattended is true) and thus cannot recieve user input. GameInput will not be initalized."));
		return;
	}

	// Doesn't make sense to have headless apps create game input
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogGameInput, Log, TEXT("GameInputBase module is exiting because it cannot render anything (FApp::CanEverRender is false). GameInput will not be initalized."));
		return;
	}

#if GAME_INPUT_SUPPORT

#if PLATFORM_WINDOWS
	// Check to see if the GameInput.dll exists...
	// Search for the GameInput dll on desktop platforms. If for some reason it doesn't exist, then we shouldn't
	// attempt to call any functions from it. The only known case for this is when running a client on a server OS
	// which doesn't have game input installed by default. 
	GameInputDLLHandle = FPlatformProcess::GetDllHandle(*UE::GameInput::GameInputDLLPath);
	if (GameInputDLLHandle == nullptr)
	{
		UE_LOG(LogGameInput, Warning, TEXT("[%hs] module is exiting because '%s' cannot be found. GameInput will not be initalized. Is it installed correctly?"), __func__, *UE::GameInput::GameInputDLLPath);
		return;
	}

// Disable warning C4191: 'type cast' : unsafe conversion from 'PROC' to 'XXX' getting the GameInputCreate function
#pragma warning(push)
#pragma warning(disable:4191)
	
	// Create the Game Input interface
	typedef HRESULT(WINAPI* FGameInputCreateFn)(IGameInput**);
	FGameInputCreateFn pfnGameInputCreate = (FGameInputCreateFn)GetProcAddress((HMODULE)GameInputDLLHandle, "GameInputCreate");

	UE_CLOG(!pfnGameInputCreate, LogGameInput, Warning, TEXT("[%hs] Failed to GetProcAddress (GameInputCreate). Game Input will fail to be created."), __func__);
	
	const HRESULT HResult = pfnGameInputCreate ? pfnGameInputCreate(&UE::GameInput::GGameInputInterface) : E_FAIL;

#pragma warning(pop)

#else 	// PLATFORM_WINDOWS
	
	// Create the Game Input interface
	const HRESULT HResult = GameInputCreate(&UE::GameInput::GGameInputInterface);
	
#endif	// !PLATFORM_WINDOWS

	UE_CLOG(FAILED(HResult), LogGameInput, Warning, TEXT("Failed to initialize GameInput: 0x%X"), HResult);

	if (SUCCEEDED(HResult))
	{
		UE_LOG(LogGameInput, Log, TEXT("[FGameInputBaseModule::StartupModule] Successfully created the IGameInput interface"));
		InitializeGameInputKeys();
	}

#else
	UE_LOG(LogGameInput, Warning, TEXT("Failed to initalize GameInput! GAME_INPUT_SUPPORT is false!"));
#endif	// #if GAME_INPUT_SUPPORT
}

void FGameInputBaseModule::ShutdownModule()
{
#if GAME_INPUT_SUPPORT
	UE::GameInput::GGameInputInterface.Reset();

#if PLATFORM_WINDOWS
	if (GameInputDLLHandle)
	{
		FPlatformProcess::FreeDllHandle(GameInputDLLHandle);
	}
#endif // #if PLATFORM_WINDOWS

#endif // #if GAME_INPUT_SUPPORT
}

#if GAME_INPUT_SUPPORT
IGameInput* FGameInputBaseModule::GetGameInput()
{
	return UE::GameInput::GGameInputInterface;
}
#endif	// GAME_INPUT_SUPPORT


void FGameInputBaseModule::InitializeGameInputKeys()
{
#if GAME_INPUT_SUPPORT

	static const FName MenuCategory = TEXT("GameInput");
	EKeys::AddMenuCategoryDisplayInfo(MenuCategory, LOCTEXT("GameInput", "Game Input"), TEXT("GraphEditor.PadEvent_16x"));

	//  
	// Racing Wheel
	//

	// Analog types
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Brake, LOCTEXT("GameInput_RacingWheel_Brake", "Game Input Racing Wheel Brake"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Clutch, LOCTEXT("GameInput_RacingWheel_Clutch", "Game Input Racing Wheel Clutch"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Handbrake, LOCTEXT("GameInput_RacingWheel_Handbrake", "Game Input Racing Wheel Handbrake"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Throttle, LOCTEXT("GameInput_RacingWheel_Throttle", "Game Input Racing Wheel Throttle"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Wheel, LOCTEXT("GameInput_RacingWheel_Wheel", "Game Input Racing Wheel"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_PatternShifterGear, LOCTEXT("GameInput_RacingWheel_PatternShifterGear", "Game Input Racing Wheel Pattern Shifter Gear"), FKeyDetails::Axis1D, MenuCategory));

	// Button types
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_None, LOCTEXT("GameInput_RacingWheel_None", "Game Input Racing Wheel None"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_Menu, LOCTEXT("GameInput_RacingWheel_Menu", "Game Input Racing Wheel Menu"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_View, LOCTEXT("GameInput_RacingWheel_View", "Game Input Racing Wheel View"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_PreviousGear, LOCTEXT("GameInput_RacingWheel_PreviousGear", "Game Input Racing Wheel Previous Gear"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::RacingWheel_NextGear, LOCTEXT("GameInput_RacingWheel_NextGear", "Game Input Racing Wheel Next Gear"), FKeyDetails::GamepadKey, MenuCategory));

	//
	// Flight Stick
	//

	// Analog types
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_Roll, LOCTEXT("GameInput_FlightStick_Roll", "Game Input Flight Stick Roll"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_Pitch, LOCTEXT("GameInput_FlightStick_Pitch", "Game Input Flight Stick Pitch"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_Yaw, LOCTEXT("GameInput_FlightStick_Yaw", "Game Input Flight Stick Yaw"), FKeyDetails::Axis1D, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_Throttle, LOCTEXT("GameInput_FlightStick_Throttle", "Game Input Flight Stick Throttle"), FKeyDetails::Axis1D, MenuCategory));

	// Button types
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_None, LOCTEXT("GameInput_FlightStick_None", "Game Input Flight Stick None"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_Menu, LOCTEXT("GameInput_FlightStick_Menu", "Game Input Flight Stick Menu"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_View, LOCTEXT("GameInput_FlightStick_View", "Game Input Flight Stick View"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_FirePrimary, LOCTEXT("GameInput_FlightStick_FirePrimary", "Game Input Flight Stick Fire Primary"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::FlightStick_FireSecondary, LOCTEXT("GameInput_FlightStick_FireSecondary", "Game Input Flight Stick Fire Secondary"), FKeyDetails::GamepadKey, MenuCategory));

	//
	// Arcade Stick    
	//

	// Button Types
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_None, LOCTEXT("GameInput_ArcadeStick_None", "Game Input Arcade Stick None"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Menu, LOCTEXT("GameInput_ArcadeStick_Menu", "Game Input Arcade Stick Menu"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_View, LOCTEXT("GameInput_ArcadeStick_View", "Game Input Arcade Stick View"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Up, LOCTEXT("GameInput_ArcadeStick_Up", "Game Input Arcade Stick Up"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Down, LOCTEXT("GameInput_ArcadeStick_Down", "Game Input Arcade Stick Down"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Left, LOCTEXT("GameInput_ArcadeStick_Left", "Game Input Arcade Stick Left"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Right, LOCTEXT("GameInput_ArcadeStick_Right", "Game Input Arcade Stick Right"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action1, LOCTEXT("GameInput_ArcadeStick_Action1", "Game Input Arcade Stick Action 1"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action2, LOCTEXT("GameInput_ArcadeStick_Action2", "Game Input Arcade Stick Action 2"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action3, LOCTEXT("GameInput_ArcadeStick_Action3", "Game Input Arcade Stick Action 3"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action4, LOCTEXT("GameInput_ArcadeStick_Action4", "Game Input Arcade Stick Action 4"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action5, LOCTEXT("GameInput_ArcadeStick_Action5", "Game Input Arcade Stick Action 5"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Action6, LOCTEXT("GameInput_ArcadeStick_Action6", "Game Input Arcade Stick Action 6"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Special1, LOCTEXT("GameInput_ArcadeStick_Special1", "Game Input Arcade Stick Special 1"), FKeyDetails::GamepadKey, MenuCategory));
	EKeys::AddKey(FKeyDetails(FGameInputKeys::ArcadeStick_Special2, LOCTEXT("GameInput_ArcadeStick_Special2", "Game Input Arcade Stick Special 2"), FKeyDetails::GamepadKey, MenuCategory));

#endif	// GAME_INPUT_SUPPORT
}

IMPLEMENT_MODULE(FGameInputBaseModule, GameInputBase)

#undef LOCTEXT_NAMESPACE