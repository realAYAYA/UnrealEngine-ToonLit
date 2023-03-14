// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputModule.h"

#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Tickable.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "EnhancedInput"

DEFINE_LOG_CATEGORY(LogEnhancedInput);

class FEnhancedInputModule : public IEnhancedInputModule, public FTickableGameObject
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;

	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Always;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FEnhancedInputModule, STATGROUP_Tickables);
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}
	// End FTickableGameObject interface

	virtual UEnhancedInputLibrary* GetLibrary() override
	{
		ensureAlwaysMsgf(Library, TEXT("Trying to access the enhanced input library after the plugin has been unloaded!"));
		return Library;
	}

	FInputActionValue ParseValue(const TArray<FString>& Args, FInputActionValue Default)
	{
		FInputActionValue Value(Default);
		if (Args.Num() > 1)
		{
			// Build value string from the remaining args
			FString ValueStr;
			for (int32 i = 1; i < Args.Num(); ++i)
			{
				ValueStr += Args[i];
				// There must be a space beteen the values of axis types for InitFromString to work correctly
				// There is no need add a space to the end
				if(i < Args.Num() - 1)
				{
					ValueStr += " ";
				}
			}

			FVector2D Value2D;
			FVector Value3D;

			if (Value3D.InitFromString(ValueStr))
			{
				Value = Value3D;					// Axis 3D type
			}
			else if (Value2D.InitFromString(ValueStr))
			{
				Value = Value2D;					// Axis 2D type
			}
			else if (ValueStr.IsNumeric())
			{
				Value = FCString::Atof(*ValueStr);	// Axis 1D type
			}
			else
			{
				Value = ValueStr.ToBool();	// Bool type
			}
		}
		return Value;
	}

	void EnableForcedAction(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();	// TODO: UGameplayStatics::GetPlayerControllerFromId(OptionalPlayerIdArg)
		if (!PC)
		{
			return;
		}

		if (!Args.Num())
		{
			PC->ClientMessage(TEXT("Expected arguments:\\n   <action> - Name of action to force each tick\\n   <value> - Value to apply. Valid examples are: true, 0.4, \"X=1.0, Y=-2.0\", \"X=0.1, Y=0.2, Z=0.3\""));
			return;
		}

		if (!Cast<UEnhancedPlayerInput>(PC->PlayerInput))
		{
			PC->ClientMessage(TEXT("Player controller is not using the Enhanced Input system."));
			return;
		}

		UInputAction* Action = FindFirstObject<UInputAction>(*Args[0], EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("EnableForcedAction"));
		if (!Action)
		{
			PC->ClientMessage(TEXT("Failed to find action '%s'."), *Args[0]);
			return;
		}

		if(UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ApplyForcedInput(Action, ParseValue(Args, FInputActionValue(true)));	// Default to triggered boolean
		}
	}

	void DisableForcedAction(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			return;
		}

		if (!Args.Num())
		{
			PC->ClientMessage(TEXT("Expected arguments:\\n   <action> - Name of action to stop forcing"));
			return;
		}

		UInputAction* Action = FindFirstObject<UInputAction>(*Args[0], EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("DisableForcedAction"));
		if (!Action)
		{
			PC->ClientMessage(TEXT("Failed to find action '%s'."), *Args[0]);
			return;
		}

		if(UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveForcedInput(Action);
		}
	}

	void EnableForcedKey(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			return;
		}

		if (!Args.Num())
		{
			PC->ClientMessage(TEXT("Expected arguments:\\n   <key> - Name of key to force each tick\\n   <value> - Value to apply. Valid examples are: true, 0.4"));
			return;
		}

		FKey FoundKey(FName(*Args[0], FNAME_Find));
		if (!FoundKey.IsValid())
		{
			PC->ClientMessage(FString::Printf(TEXT("Failed to locate a key named '%s'"), *Args[0]));
			return;
		}
		
		if(UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ApplyForcedInput(FoundKey, ParseValue(Args, FInputActionValue(1.f))); // Default to "pressed" standard key (Axis1D)
		}
	}

	void DisableForcedKey(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			return;
		}

		if (!Args.Num())
		{
			PC->ClientMessage(TEXT("Expected arguments:\\n   <key> - Name of key to stop forcing"));
			return;
		}

		FKey FoundKey(FName(*Args[0], FNAME_Find));
		if (!FoundKey.IsValid())
		{
			PC->ClientMessage(FString::Printf(TEXT("Failed to locate a key named '%s'"), *Args[0]));
			return;
		}


		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveForcedInput(FoundKey);
		}
	}

	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

private:
	UEnhancedInputLibrary* Library = nullptr;

	TArray<IConsoleCommand*> ConsoleCommands;

	/** The last frame number we were ticked.  We don't want to tick multiple times per frame */
	uint64 LastFrameNumberWeTicked = (uint64)-1;
};

IMPLEMENT_MODULE(FEnhancedInputModule, EnhancedInput)

void FEnhancedInputModule::StartupModule()
{
	Library = NewObject<UEnhancedInputLibrary>(GetTransientPackage(), UEnhancedInputLibrary::StaticClass(), NAME_None);
	Library->AddToRoot();

	if (!IsRunningDedicatedServer())
	{
#if ENABLE_DRAW_DEBUG
		AHUD::OnShowDebugInfo.AddStatic(&FEnhancedInputModule::OnShowDebugInfo);
#endif
	    // Register console commands
	    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		    TEXT("Input.+action"),
		    TEXT("Provide the named action with a constant input value each frame"),
		    FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FEnhancedInputModule::EnableForcedAction),
		    ECVF_Cheat
	    ));

		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Input.-action"),
			TEXT("Stop forcing the named action value each frame"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FEnhancedInputModule::DisableForcedAction),
			ECVF_Cheat
		));

		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Input.+key"),
			TEXT("Provide the named key with a constant input value each frame"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FEnhancedInputModule::EnableForcedKey),
			ECVF_Cheat
		));

		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Input.-key"),
			TEXT("Stop forcing the named key each frame"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FEnhancedInputModule::DisableForcedKey),
			ECVF_Cheat
		));
	}
}

void FEnhancedInputModule::ShutdownModule()
{
	// Unregister console commands
	for (IConsoleCommand* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}

	if (!GExitPurge) // If GExitPurge Object is already gone
	{
		Library->RemoveFromRoot();
	}
	Library = nullptr;
}

void FEnhancedInputModule::Tick(float DeltaTime)
{
	// This may tick multiple times per frame? See MIDIDevice module.
	if (LastFrameNumberWeTicked == GFrameCounter)
	{
		return;
	}
	LastFrameNumberWeTicked = GFrameCounter;

	UEnhancedInputLibrary::ForEachSubsystem([DeltaTime](IEnhancedInputSubsystemInterface* Subsystem)
		{
			Subsystem->RebuildControlMappings();
			Subsystem->TickForcedInput(DeltaTime);
			Subsystem->HandleControlMappingRebuildDelegate();
		});
}


void FEnhancedInputModule::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_EnhancedInput("EnhancedInput");
	static const FName NAME_PlatformDevices("Devices");
	if (Canvas)
	{
		if (HUD->ShouldDisplayDebug(NAME_EnhancedInput))
		{
			FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
			DisplayDebugManager.SetFont(GEngine->GetSmallFont());
			DisplayDebugManager.SetDrawColor(FColor::Yellow);
			DisplayDebugManager.DrawString(TEXT("ENHANCED INPUT"));
			
			// TODO: Support paging through subsystems one at a time (via console? key press?)

			// Show first player only for now
			TObjectIterator<UEnhancedInputLocalPlayerSubsystem> FirstPlayer;
			if (FirstPlayer)
			{
				FirstPlayer->ShowDebugInfo(Canvas);
			}
		}
		
		if (HUD->ShouldDisplayDebug(NAME_PlatformDevices))
		{
			UEnhancedInputLibrary::ForEachSubsystem([Canvas](IEnhancedInputSubsystemInterface* Subsystem)
			{
				Subsystem->ShowPlatformInputDebugInfo(Canvas);
			});
		}
	}
	else
	{
		// Free up visualizer textures
		IEnhancedInputSubsystemInterface::PurgeDebugVisualizations();
	}
}



#undef LOCTEXT_NAMESPACE