// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/InputVCamSubsystem.h"

#include "Input/VCamPlayerInput.h"
#include "LogVCamCore.h"
#include "VCamComponent.h"
#include "VCamInputProcessor.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ConsoleManager.h"

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
	if (ensure(FSlateApplication::IsInitialized()))
	{
		// It's dangerous to consume input in editor (imagine typing something into search boxes but all L keys were consumed by VCam input)
		// whereas probably expected by gameplay code.
		using namespace UE::VCamCore::Private;
		InputPreprocessor = MakeShared<FVCamInputProcessor>(*this, EInputConsumptionRule::DoNotConsume);
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, 0);	
	}

#if WITH_EDITOR
	// Use-case: Person A using gamepad to drive VCam input while Person B clicks stuff in editor > Gamepad may start navigating editor widgets. This CVar prevents that.
	UE::VCamCore::Private::IncrementAndSetEnableGamepadEditorNavigation();
#endif
}

void UInputVCamSubsystem::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogVCamCore, Log, TEXT("De-initializing UInputVCamSubsystem..."));

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
	}

	PlayerInput = nullptr;

#if WITH_EDITOR
	UE::VCamCore::Private::DecrementAndResetEnableGamepadEditorNavigation();
#endif
}

void UInputVCamSubsystem::OnUpdate(float DeltaTime)
{
	if (!ensure(PlayerInput))
	{
		return;
	}

	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	RequestRebuildControlMappings(Options);

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
	if (!ensure(InInputComponent))
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

void UInputVCamSubsystem::SetShouldConsumeGamepadInput(EVCamGamepadInputMode GamepadInputMode)
{
	check(InputPreprocessor && PlayerInput);
	
	FVCamInputDeviceConfig InputSettings = PlayerInput->GetInputSettings();
	InputSettings.GamepadInputMode = GamepadInputMode;
	SetInputSettings(InputSettings);
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

	const bool bShouldConsumeGamepad = Input.GamepadInputMode == EVCamGamepadInputMode::IgnoreAndConsume || Input.GamepadInputMode == EVCamGamepadInputMode::AllowAndConsume;
	InputPreprocessor->SetInputConsumptionRule(
		bShouldConsumeGamepad ? UE::VCamCore::Private::EInputConsumptionRule::ConsumeOnlyGamepadIfUsed : UE::VCamCore::Private::EInputConsumptionRule::DoNotConsume
		);
}

UEnhancedPlayerInput* UInputVCamSubsystem::GetPlayerInput() const
{
	return PlayerInput;
}
