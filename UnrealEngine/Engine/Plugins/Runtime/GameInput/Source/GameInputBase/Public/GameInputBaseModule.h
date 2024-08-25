// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#if GAME_INPUT_SUPPORT
struct IGameInput;
#endif

class GAMEINPUTBASE_API FGameInputBaseModule : public IModuleInterface
{
public:

	static FGameInputBaseModule& Get();

	/** Returns true if this module is loaded (aka available) by the FModuleManager */
	static bool IsAvailable();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

#if GAME_INPUT_SUPPORT
	/** 
	* Pointer to the static IGameInput that is created upon module startup.
	*/
	static IGameInput* GetGameInput();
#endif

protected:

	void InitializeGameInputKeys();

#if PLATFORM_WINDOWS && GAME_INPUT_SUPPORT
	// Handle to the game input dll which is set on StartupModule.
	// If we can't find the DLL then we will early exit and not attempt to initalize GameInput.
	void* GameInputDLLHandle = nullptr;
#endif // endif PLATFORM_WINDOWS && GAME_INPUT_SUPPORT
};