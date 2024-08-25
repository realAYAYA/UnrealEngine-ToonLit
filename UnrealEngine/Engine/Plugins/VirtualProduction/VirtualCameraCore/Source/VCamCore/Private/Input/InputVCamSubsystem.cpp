// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/InputVCamSubsystem.h"

#include "EnhancedInputDeveloperSettings.h"
#include "Input/VCamPlayerInput.h"
#include "LogVCamCore.h"
#include "VCamComponent.h"
#include "VCamInputProcessor.h"

#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ConsoleManager.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace UE::VCamCore::Private
{
#if WITH_EDITOR
	static int32 GVCamInputSubsystemCount = 0;
	static bool GEnableGamepadEditorNavigationValueBeforeSetting = true;

	static void IncrementAndSetEnableGamepadEditorNavigation()
	{
		++GVCamInputSubsystemCount;
	
		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.EnableGamepadEditorNavigation")))
		{
			if (GVCamInputSubsystemCount == 1)
			{
				GEnableGamepadEditorNavigationValueBeforeSetting = ConsoleVariable->GetBool();
			}
		
			ConsoleVariable->Set(false);
		}
	}

	static void DecrementAndResetEnableGamepadEditorNavigation()
	{
		--GVCamInputSubsystemCount;
		if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.EnableGamepadEditorNavigation"))
			; GVCamInputSubsystemCount == 0 && ConsoleVariable)
		{
			ConsoleVariable->Set(GEnableGamepadEditorNavigationValueBeforeSetting);
		}
	}
#endif
}

void UInputVCamSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogVCamCore, Log, TEXT("Initializing UInputVCamSubsystem..."));
	
	PlayerInput = NewObject<UVCamPlayerInput>(this);
	
	// Create and register the input preprocessor, this is what will call our "InputKey"
	// function to drive input instead of a player controller
	if (FSlateApplication::IsInitialized())
	{
		// It's dangerous to consume input in editor (imagine typing something into search boxes but all L keys were consumed by VCam input)
		// whereas probably expected by gameplay code.
		using namespace UE::VCamCore::Private;
		InputPreprocessor = MakeShared<FVCamInputProcessor>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, 0);

		// The below things should only be done in Slate applications. Slate is disabled e.g. in commandlets. It makes no sense to have VCam input in such cases.
#if WITH_EDITOR
		// Use-case: Person A using gamepad to drive VCam input while Person B clicks stuff in editor > Gamepad may start navigating editor widgets. This CVar prevents that.
		UE::VCamCore::Private::IncrementAndSetEnableGamepadEditorNavigation();
#endif
		
		if (GetDefault<UEnhancedInputDeveloperSettings>()->bEnableUserSettings)
		{
			InitalizeUserSettings();
		}
	}
}

void UInputVCamSubsystem::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogVCamCore, Log, TEXT("De-initializing UInputVCamSubsystem..."));

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
		InputPreprocessor.Reset(); // UObject will still around until GC'ed. No point in keeping the InputProcessor around.
		
		PlayerInput = nullptr;

#if WITH_EDITOR
		UE::VCamCore::Private::DecrementAndResetEnableGamepadEditorNavigation();
#endif
	}
}

void UInputVCamSubsystem::InitalizeUserSettings()
{
	UserSettings = NewObject<UEnhancedInputUserSettings>(this, TEXT("UserSettings"), RF_Transient);
	// UEnhancedInputUserSettings's API is designed to work with ULocalPlayers. However, we won't be making any calls to functions that internally call GetOwningPlayer().
	ULocalPlayer* LocalPlayerHack = GetMutableDefault<ULocalPlayer>();
	UserSettings->Initialize(LocalPlayerHack);
	BindUserSettingDelegates();
}

void UInputVCamSubsystem::OnUpdate(float DeltaTime)
{
	if (!ensure(PlayerInput))
	{
		return;
	}

	TArray<UInputComponent*> InputStack;
	for (auto It = CurrentInputStack.CreateIterator(); It; ++It)
	{
		if (UInputComponent* InputComponent = It->Get())
		{
			InputStack.Push(InputComponent);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
	
	PlayerInput->Tick(DeltaTime);
	PlayerInput->ProcessInputStack(InputStack, DeltaTime, false);
}

bool UInputVCamSubsystem::InputKey(const FInputKeyParams& Params)
{
	// UVCamComponent::Update causes UInputVCamSubsystem::OnUpdate to be called.
	// If CanUpdate tells us that won't be called, no input should be enqueued.
	// If it was, then the next time an Update occurs, there would be an "explosion" of processed, accumulated, outdated inputs.
	return GetVCamComponent()->CanUpdate()
		&& PlayerInput->InputKey(Params);
}

void UInputVCamSubsystem::PushInputComponent(UInputComponent* InInputComponent)
{
	if (!ensureAlways(InInputComponent))
	{
		return;
	}
	
	bool bPushed = false;
	CurrentInputStack.RemoveSingle(InInputComponent);
	for (int32 Index = CurrentInputStack.Num() - 1; Index >= 0; --Index)
	{
		UInputComponent* IC = CurrentInputStack[Index].Get();
		if (IC == nullptr)
		{
			CurrentInputStack.RemoveAt(Index);
		}
		else if (IC->Priority <= InInputComponent->Priority)
		{
			CurrentInputStack.Insert(InInputComponent, Index + 1);
			bPushed = true;
			break;
		}
	}
	if (!bPushed)
	{
		CurrentInputStack.Insert(InInputComponent, 0);
		RequestRebuildControlMappings();
	}
}

bool UInputVCamSubsystem::PopInputComponent(UInputComponent* InInputComponent)
{
	if (ensure(InInputComponent) && CurrentInputStack.RemoveSingle(InInputComponent) > 0)
	{
		InInputComponent->ClearBindingValues();
		RequestRebuildControlMappings();
		return true;
	}
	return false;
}

const FVCamInputDeviceConfig& UInputVCamSubsystem::GetInputSettings() const
{
	// Undefined behaviour returning from dereferenced nullptr, let's make sure to assert.
	UE_CLOG(PlayerInput == nullptr, LogVCamCore, Fatal, TEXT("PlayerInput is designed to exist for the lifetime of UInputVCamSubsystem. Investigate!"));
	return PlayerInput->GetInputSettings();
}

void UInputVCamSubsystem::SetInputSettings(const FVCamInputDeviceConfig& Input)
{
	check(PlayerInput);
	PlayerInput->SetInputSettings(Input);
}

UEnhancedPlayerInput* UInputVCamSubsystem::GetPlayerInput() const
{
	return PlayerInput;
}
