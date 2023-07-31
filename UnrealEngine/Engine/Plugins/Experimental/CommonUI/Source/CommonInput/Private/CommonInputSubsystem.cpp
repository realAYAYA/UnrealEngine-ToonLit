// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonInputSubsystem.h"
#include "CommonInputPrivate.h"
#include "Misc/ConfigCacheIni.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateUser.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "HAL/IConsoleManager.h"
#include "CommonInputSettings.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "ICommonInputModule.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInputSubsystem)

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

FPlatformInputSupportOverrideDelegate UCommonInputSubsystem::OnPlatformInputSupportOverride;

/**
 * Helper class that is designed to fire before any UI has a chance to process input so that
 * we can properly set the current input type of the application.
 */
class FCommonInputPreprocessor : public IInputProcessor
{
public:
	FCommonInputPreprocessor(UCommonInputSubsystem& InCommonInputSubsystem)
		: InputSubsystem(InCommonInputSubsystem)
		, bIgnoreNextMove(false)
	{
		for (uint8 InputTypeIndex = 0; InputTypeIndex < (uint8)ECommonInputType::Count; InputTypeIndex++)
		{
			InputMethodPermissions[InputTypeIndex] = false;
		}
	}

	
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		const ECommonInputType InputType = GetInputType(InKeyEvent.GetKey());
		if (IsRelevantInput(SlateApp, InKeyEvent, InputType))
		{
			if (IsInputMethodBlocked(InputType))
			{
				return true;
			}

			RefreshCurrentInputMethod(InputType);
		}
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	{
		const ECommonInputType InputType = GetInputType(InAnalogInputEvent.GetKey());
		return IsRelevantInput(SlateApp, InAnalogInputEvent, InputType) && IsInputMethodBlocked(InputType);
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override
	{
		const ECommonInputType InputType = GetInputType(InPointerEvent);
		if (IsRelevantInput(SlateApp, InPointerEvent, InputType))
		{
			if (bIgnoreNextMove)
			{
				bIgnoreNextMove = false;
			}
			else if (!InPointerEvent.GetCursorDelta().IsNearlyZero())
			{
				if (IsInputMethodBlocked(InputType))
				{
					return true;
				}

				RefreshCurrentInputMethod(InputType);
			}
		}
		
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override
	{
		const ECommonInputType InputType = GetInputType(InPointerEvent);
		if (IsRelevantInput(SlateApp, InPointerEvent, InputType))
		{
			if (IsInputMethodBlocked(InputType))
			{
				return true;
			}
			RefreshCurrentInputMethod(InputType);
		}
		return false;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override
	{
		const ECommonInputType InputType = GetInputType(InPointerEvent);
		if (IsRelevantInput(SlateApp, InPointerEvent, InputType))
		{
			if (IsInputMethodBlocked(InputType))
			{
				return true;
			}
			RefreshCurrentInputMethod(InputType);
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("CommonInput"); }

	void SetInputTypeFilter(ECommonInputType InputType, FName InReason, bool InFilter)
	{
		TMap<FName, bool> Reasons = FilterInputTypeWithReasons[(uint8)InputType];
		Reasons.Add(InReason, InFilter);

		bool ComputedFilter = false;
		for (const auto& Entry : Reasons)
		{
			ComputedFilter |= Entry.Value;
		}

		InputMethodPermissions[(uint8)InputType] = ComputedFilter;
	}

	bool IsInputMethodBlocked(ECommonInputType InputType) const
	{
		return InputMethodPermissions[(uint8)InputType];
	}

	FGamepadChangeDetectedEvent OnGamepadChangeDetected;

private:
	bool IsRelevantInput(FSlateApplication& SlateApp, const FInputEvent& InputEvent, const ECommonInputType DesiredInputType)
	{
		if (SlateApp.IsActive() 
			|| SlateApp.GetHandleDeviceInputWhenApplicationNotActive() 
			|| (ICommonInputModule::GetSettings().GetAllowOutOfFocusDeviceInput() && DesiredInputType == ECommonInputType::Gamepad))
		{
			const ULocalPlayer& LocalPlayer = *InputSubsystem.GetLocalPlayerChecked();
			int32 ControllerId = LocalPlayer.GetControllerId();

#if WITH_EDITOR
			// PIE is a very special flower, especially when running two clients - we have two LocalPlayers with ControllerId 0
			// The editor has existing shenanigans for handling this scenario, so we're using those for now
			// Ultimately this would ideally be something the editor resolves at the SlateApplication level with a custom ISlateInputMapping or something
			GEngine->RemapGamepadControllerIdForPIE(LocalPlayer.ViewportClient, ControllerId);
#endif
			return ControllerId == InputEvent.GetUserIndex();
		}
		return false;
	}

	void RefreshCurrentInputMethod(ECommonInputType InputMethod)
	{
#if WITH_EDITOR
		// Lots of special-case fun for PIE - there are special scenarios wherein we want to ignore the update attempt
		const ULocalPlayer& LocalPlayer = *InputSubsystem.GetLocalPlayerChecked();
		bool bCanApplyInputMethodUpdate = false;
		if (LocalPlayer.ViewportClient)
		{
			TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(LocalPlayer.GetControllerId());
			if (ensure(SlateUser))
			{
				bool bRoutingGamepadToNextPlayer = false;
				if (!GetDefault<ULevelEditorPlaySettings>()->GetRouteGamepadToSecondWindow(bRoutingGamepadToNextPlayer))
				{
					// This looks strange - it's because the getter will set the output param based on the value of the setting, but return
					//	whether the setting actually matters. So we're making sure that even if the setting is true, we revert to false when it's irrelevant.
					bRoutingGamepadToNextPlayer = false;
				}

				if (SlateUser->IsWidgetInFocusPath(LocalPlayer.ViewportClient->GetGameViewportWidget()))
				{
					// Our owner's game viewport is in the focus path, which in a PIE scenario means we shouldn't
					// acknowledge gamepad input if it's being routed to another PIE client
					if (InputMethod != ECommonInputType::Gamepad || !bRoutingGamepadToNextPlayer)
					{
						bCanApplyInputMethodUpdate = true;
					}
				}
				else if (InputMethod == ECommonInputType::Gamepad)
				{
					bCanApplyInputMethodUpdate = bRoutingGamepadToNextPlayer;
				}
			}
		}
		if (!bCanApplyInputMethodUpdate)
		{
			return;
		}
#endif

		InputSubsystem.SetCurrentInputType(InputMethod);

		// Try to auto-detect the type of gamepad
		if ((InputMethod == ECommonInputType::Gamepad) && UCommonInputPlatformSettings::Get()->CanChangeGamepadType())
		{
			if (const FInputDeviceScope* DeviceScope = FInputDeviceScope::GetCurrent())
			{
				if ((DeviceScope->InputDeviceName != LastSeenGamepadInputDeviceName) || (DeviceScope->HardwareDeviceIdentifier != LastSeenGamepadHardwareDeviceIdentifier))
				{
					LastSeenGamepadInputDeviceName = DeviceScope->InputDeviceName;
					LastSeenGamepadHardwareDeviceIdentifier = DeviceScope->HardwareDeviceIdentifier;

					const FName GamepadInputType = InputSubsystem.GetCurrentGamepadName();
					const FName BestGamepadType = UCommonInputPlatformSettings::Get()->GetBestGamepadNameForHardware(GamepadInputType, DeviceScope->InputDeviceName, DeviceScope->HardwareDeviceIdentifier);
					if (BestGamepadType != GamepadInputType)
					{
						UE_LOG(LogCommonInput, Log, TEXT("UCommonInputSubsystem: Autodetect changed GamepadInputType to %s"), *BestGamepadType.ToString());
						InputSubsystem.SetGamepadInputType(BestGamepadType);
						OnGamepadChangeDetected.Broadcast(BestGamepadType);
					}
				}
			}
		}
	}

	ECommonInputType GetInputType(const FKey& Key)
	{
		// If the key is a gamepad key or if the key is a Virtual Click key (which simulates a click), we should be in Gamepad mode
		if (Key.IsGamepadKey() || InputSubsystem.GetIsGamepadSimulatedClick())
		{
			if (UCommonInputSubsystem::IsMobileGamepadKey(Key))
			{
				return ECommonInputType::Touch;
			}

			//@todo DanH: Why would the gamepad simulating a click imply we also want to ignore key input? Should the property instead be IsGamepadSimulatedInput? Or is this wrong?
			return ECommonInputType::Gamepad;
		}

		return ECommonInputType::MouseAndKeyboard;
	}

	ECommonInputType GetInputType(const FPointerEvent& PointerEvent)
	{
		if (PointerEvent.IsTouchEvent())
		{
			return ECommonInputType::Touch;
		}
		else if (InputSubsystem.GetIsGamepadSimulatedClick())
		{
			return ECommonInputType::Gamepad;
		}
		return ECommonInputType::MouseAndKeyboard;
	}
	
private:
	UCommonInputSubsystem& InputSubsystem;
	
	bool bIgnoreNextMove = false;
	bool InputMethodPermissions[(uint8)ECommonInputType::Count];

	// The reasons we might be filtering input right now.
	TMap<FName, bool> FilterInputTypeWithReasons[(uint8)ECommonInputType::Count];

	FName LastSeenGamepadInputDeviceName;
	FString LastSeenGamepadHardwareDeviceIdentifier;

	friend class UCommonInputSubsystem;
};

//////////////////////////////////////////////////////////////////////////
// UCommonInputSubsystem
//////////////////////////////////////////////////////////////////////////

static int32 GCommonInputKeysVisible = 1;
static FAutoConsoleVariableRef CVarInputKeysVisible
(
	TEXT("CommonInput.ShowKeys"),
	GCommonInputKeysVisible,
	TEXT("Should we show the keys for the current input device."),
	ECVF_Default
);

UCommonInputSubsystem* UCommonInputSubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UCommonInputSubsystem>() : nullptr;
}

UCommonInputSubsystem::UCommonInputSubsystem()
{
	//@TODO: Project setting (should be done as another config var any platform can set tho)?
//	// Uncomment this if we want mobile platforms to default to gamepad when gamepads are enabled.
// #if PLATFORM_IOS
//     bool bAllowControllers = false;
//     GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
//     CurrentInputType = LastInputType = bAllowControllers ? ECommonInputType::Gamepad : CurrentInputType;
// #elif PLATFORM_ANDROID
//     bool bAllowControllers = false;
//     GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
//     CurrentInputType = LastInputType = bAllowControllers ? ECommonInputType::Gamepad : CurrentInputType;
// #endif
}

void UCommonInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCommonInputBase::GetInputSettings()->LoadData();

	const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();

	GamepadInputType = Settings->GetDefaultGamepadName();
	CurrentInputType = LastInputType = Settings->GetDefaultInputType();

	CommonInputPreprocessor = MakeShared<FCommonInputPreprocessor>(*this);
	FSlateApplication::Get().RegisterInputPreProcessor(CommonInputPreprocessor, 0);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCommonInputSubsystem::Tick), 0.1f);

	CVarInputKeysVisible->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UCommonInputSubsystem::ShouldShowInputKeysChanged));

	SetActionDomainTable(FCommonInputBase::GetInputSettings()->GetActionDomainTable());
}

void UCommonInputSubsystem::Deinitialize()
{
	Super::Deinitialize();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(CommonInputPreprocessor);
	}
	CommonInputPreprocessor.Reset();

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
}

FGamepadChangeDetectedEvent& UCommonInputSubsystem::GetOnGamepadChangeDetected()
{
	return CommonInputPreprocessor->OnGamepadChangeDetected;
}

void UCommonInputSubsystem::SetInputTypeFilter(ECommonInputType InputType, FName Reason, bool Filter)
{
	CommonInputPreprocessor->SetInputTypeFilter(InputType, Reason, Filter);
}

bool UCommonInputSubsystem::GetInputTypeFilter(ECommonInputType InputType) const
{
	return CommonInputPreprocessor->IsInputMethodBlocked(InputType);
}

void UCommonInputSubsystem::AddOrRemoveInputTypeLock(FName InReason, ECommonInputType InInputType, bool bAddLock)
{
	if (bAddLock)
	{
		ECommonInputType& CurrentValue = CurrentInputLocks.FindOrAdd(InReason);
		CurrentValue = InInputType;

		UE_LOG(LogCommonInput, Log, TEXT("Adding Input Method Lock: %s - %d"), *InReason.ToString(), (int32)InInputType);
	}
	else
	{
		CurrentInputLocks.Remove(InReason);

		UE_LOG(LogCommonInput, Log, TEXT("Removing Input Method Lock: %s - %d"), *InReason.ToString(), (int32)InInputType);
	}

	int32 ComputedInputLock = INDEX_NONE;
	for (auto Entry : CurrentInputLocks)
	{
		// Take the most restrictive lock, e.g. Gamepad lock is more restrictive than a Keyboard/Mouse lock.
		if (((int32)Entry.Value) > ComputedInputLock)
		{
			ComputedInputLock = (int32)Entry.Value;
		}
	}

	if (ComputedInputLock == INDEX_NONE)
	{
		CurrentInputLock.Reset();
	}
	else
	{
		CurrentInputLock = (ECommonInputType)ComputedInputLock;
	}

	// If a lock was put in place, lock the current input type.
	CurrentInputType = LockInput(LastInputType);

	if (CurrentInputType != LastInputType)
	{
		BroadcastInputMethodChanged();
	}
}

bool UCommonInputSubsystem::IsInputMethodActive(ECommonInputType InputMethod) const
{
	return GetCurrentInputType() == InputMethod;
}

ECommonInputType UCommonInputSubsystem::LockInput(ECommonInputType InputToLock) const
{
	return CurrentInputLock.Get(InputToLock);
}

ECommonInputType UCommonInputSubsystem::GetCurrentInputType() const
{
	return CurrentInputType;
}

ECommonInputType UCommonInputSubsystem::GetDefaultInputType() const
{
	return UCommonInputPlatformSettings::Get()->GetDefaultInputType();
}

void UCommonInputSubsystem::BroadcastInputMethodChanged()
{
	if (UWorld* World = GetWorld())
	{
		if (!World->bIsTearingDown)
		{
			OnInputMethodChangedNative.Broadcast(CurrentInputType);
			OnInputMethodChanged.Broadcast(CurrentInputType);
			LastInputMethodChangeTime = FPlatformTime::Seconds();
		}
	}
}

bool UCommonInputSubsystem::CheckForInputMethodThrashing(ECommonInputType NewInputType)
{
	UCommonInputSettings& InputSettings = ICommonInputModule::GetSettings();


	if (InputSettings.GetEnableInputMethodThrashingProtection())
	{
		const double Now = FPlatformTime::Seconds();

		if (LastTimeInputMethodThrashingBegan + InputSettings.GetInputMethodThrashingCooldownInSeconds() > Now)
		{
			return true;
		}
		else if (CurrentInputLocks.Contains(TEXT("InputMethodThrashing")))
		{
			//Remove the thrashing lock.
			AddOrRemoveInputTypeLock(TEXT("InputMethodThrashing"), ECommonInputType::MouseAndKeyboard, false);
		}

		switch (NewInputType)
		{
		case ECommonInputType::Gamepad:
		case ECommonInputType::MouseAndKeyboard:
			break;
		default:
			// Ignore any thrashing that's not exclusively between mouse and gamepad.
			NumberOfInputMethodChangesRecently = 0;
			return false;
		}

		const double ChangeDelta = (Now - LastInputMethodChangeTime);
		if (ChangeDelta < InputSettings.GetInputMethodThrashingWindowInSeconds())
		{
			NumberOfInputMethodChangesRecently++;
			if (NumberOfInputMethodChangesRecently > InputSettings.GetInputMethodThrashingLimit())
			{
				LastTimeInputMethodThrashingBegan = Now;
				//Add the thrashing lock
				AddOrRemoveInputTypeLock(TEXT("InputMethodThrashing"), ECommonInputType::MouseAndKeyboard, true);
				NumberOfInputMethodChangesRecently = 0;
				return true;
			}
		}
		else
		{
			NumberOfInputMethodChangesRecently = 0;
		}
	}
	return false;
}

void UCommonInputSubsystem::SetCurrentInputType(ECommonInputType NewInputType)
{
	if ((LastInputType != NewInputType) && PlatformSupportsInputType(NewInputType))
	{
		CheckForInputMethodThrashing(NewInputType);
	
		//If we have any locks we can't change the input mode.
		if (!CurrentInputLocks.Num())
		{
			LastInputType = NewInputType;

			ECommonInputType LockedInput = LockInput(NewInputType);

			if (LockedInput != CurrentInputType)
			{
				CurrentInputType = LockedInput;

				FSlateApplication& SlateApplication = FSlateApplication::Get();
				ULocalPlayer* LocalPlayer = GetLocalPlayerChecked();
				bool bCursorUser = LocalPlayer && LocalPlayer->GetSlateUser() == SlateApplication.GetCursorUser();

				switch (CurrentInputType)
				{
				case ECommonInputType::Gamepad:
					UE_LOG(LogCommonInput, Log, TEXT("UCommonInputSubsystem::SetCurrentInputType(): Using Gamepad"));
					if (bCursorUser)
					{
						SlateApplication.UsePlatformCursorForCursorUser(false);
					}
					break;
				case ECommonInputType::Touch:
					UE_LOG(LogCommonInput, Log, TEXT("UCommonInputSubsystem::SetCurrentInputType(): Using Touch"));
					break;
				case ECommonInputType::MouseAndKeyboard:
				default:				
					UE_LOG(LogCommonInput, Log, TEXT("UCommonInputSubsystem::SetCurrentInputType(): Using Mouse"));
					if (bCursorUser)
					{
						SlateApplication.UsePlatformCursorForCursorUser(true);
					}
					break;
				}

				BroadcastInputMethodChanged();
			}
		}
	}
}

const FName UCommonInputSubsystem::GetCurrentGamepadName() const
{
	return GamepadInputType;
}
 
void UCommonInputSubsystem::SetGamepadInputType(const FName InGamepadInputType) 
{
	if (ensure(UCommonInputPlatformSettings::Get()->CanChangeGamepadType()))
	{
		GamepadInputType = InGamepadInputType;

		// Send out notifications so we update our buttons
		//BroadcastLastInputDeviceChanged();
		BroadcastInputMethodChanged();
	}
}

bool UCommonInputSubsystem::IsUsingPointerInput() const
{
	bool bUsingPointerInput = false;

	switch (LastInputType)
	{
		case ECommonInputType::MouseAndKeyboard:
		case ECommonInputType::Touch:
		{
			bUsingPointerInput = true;
		}
		break;

		case ECommonInputType::Gamepad:
		default:
		{
			bUsingPointerInput = false;
		}
		break;
	}

	return bUsingPointerInput;
}

bool UCommonInputSubsystem::ShouldShowInputKeys() const
{
	return GCommonInputKeysVisible != 0;
}

bool UCommonInputSubsystem::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UCommonInputSubsystem_Tick);

	//@todo DanH: This is wrong now that two of these might exist (and has always been wrong for multi-client PIE scenarios)
	//		Preprocessors need to be kept associated with their registration priority so we can safely know these won't get all thrown out of whack as others come and go

	//Keep the CommonInputPreprocessor on top. Input swap and input filtering (e.g. "Ignore Gamepad Input")
	//both start to break down if narrow game preprocessors temporarily get in front of it.
	//This is a workaround to avoid a bigger intervention in the SlateApplication API for managing preprocessors.
	if (CommonInputPreprocessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		if (SlateApplication.FindInputPreProcessor(CommonInputPreprocessor) != 0)
		{
			SlateApplication.UnregisterInputPreProcessor(CommonInputPreprocessor);
			SlateApplication.RegisterInputPreProcessor(CommonInputPreprocessor, 0);
		}
	}
	
	return true; //repeat ticking
}

void UCommonInputSubsystem::ShouldShowInputKeysChanged(IConsoleVariable* Var)
{
	//BroadcastLastInputDeviceChanged();
	BroadcastInputMethodChanged();
}

bool UCommonInputSubsystem::PlatformSupportsHardwareCursor() const
{
#if PLATFORM_DESKTOP
	return true;
#else
	return false;
#endif
}

void UCommonInputSubsystem::SetCursorPosition(FVector2D NewPosition, bool bForce)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayerChecked();
	if (TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr)
	{
		UpdateCursorPosition(SlateUser.ToSharedRef(), NewPosition, bForce);
	}

	if (CommonInputPreprocessor)
	{
		CommonInputPreprocessor->bIgnoreNextMove = true;
	}
}

void UCommonInputSubsystem::UpdateCursorPosition(TSharedRef<FSlateUser> SlateUser, const FVector2D& NewPosition, bool bForce)
{
	const FVector2D ClampedNewPosition = ClampPositionToViewport(NewPosition);

	//grab the old position
	const FVector2D OldPosition = SlateUser->GetCursorPosition();

	//make sure we are actually moving
	int32 NewIntPosX = ClampedNewPosition.X;
	int32 NewIntPosY = ClampedNewPosition.Y;
	int32 OldIntPosX = OldPosition.X;
	int32 OldIntPosY = OldPosition.Y;
	if (bForce || OldIntPosX != NewIntPosX || OldIntPosY != NewIntPosY)
	{
		//put the cursor in the correct spot
		SlateUser->SetCursorPosition(NewIntPosX, NewIntPosY);

		// Since the cursor may have been locked and its location clamped, get the actual new position
		const FVector2D UpdatedPosition = SlateUser->GetCursorPosition();

		FSlateApplication& SlateApp = FSlateApplication::Get();

		//create a new mouse event
		FPointerEvent MouseEvent(
			FSlateApplicationBase::CursorPointerIndex,
			UpdatedPosition,
			OldPosition,
			SlateApp.GetPressedMouseButtons(),
			EKeys::Invalid,
			0,
			SlateApp.GetPlatformApplication()->GetModifierKeys()
		);

		//process the event
		SlateApp.ProcessMouseMoveEvent(MouseEvent);
	}
}

bool UCommonInputSubsystem::GetIsGamepadSimulatedClick() const
{
	return bIsGamepadSimulatedClick;
}

void UCommonInputSubsystem::SetIsGamepadSimulatedClick(bool bNewIsGamepadSimulatedClick)
{
	if (bIsGamepadSimulatedClick != bNewIsGamepadSimulatedClick)
	{
		bIsGamepadSimulatedClick = bNewIsGamepadSimulatedClick;
		UE_CLOG(bIsGamepadSimulatedClick, LogCommonInput, VeryVerbose, TEXT("UCommonInputSubsystem::SetIsGamepadSimulatedClick(): Click is being simulated"));
	}
}

FVector2D UCommonInputSubsystem::ClampPositionToViewport(const FVector2D& InPosition) const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return InPosition;
	}

	UGameViewportClient* GameViewport = World->GetGameViewport();
	if (!GameViewport || !GameViewport->Viewport || !GameViewport->GetWindow().IsValid())
	{
		return InPosition;
	}

	TSharedPtr<SViewport> GameViewportWidget = GameViewport->GetGameViewportWidget();
	if (GameViewportWidget.IsValid())
	{
		const FGeometry& ViewportGeometry = GameViewportWidget->GetCachedGeometry();
		FVector2D LocalPosition = ViewportGeometry.AbsoluteToLocal(InPosition);
		LocalPosition.X = FMath::Clamp(LocalPosition.X, 1.0f, ViewportGeometry.GetLocalSize().X - 1.0f);
		LocalPosition.Y = FMath::Clamp(LocalPosition.Y, 1.0f, ViewportGeometry.GetLocalSize().Y - 1.0f);

		return ViewportGeometry.LocalToAbsolute(LocalPosition);
	}

	return InPosition;
}

bool UCommonInputSubsystem::PlatformSupportsInputType(ECommonInputType InInputType) const
{
	bool bPlatformSupportsInput = UCommonInputPlatformSettings::Get()->SupportsInputType(InInputType);
	switch (InInputType)
	{
		case ECommonInputType::MouseAndKeyboard:
		{
#if UE_COMMONINPUT_PLATFORM_KBM_REQUIRES_ATTACHED_MOUSE
			bPlatformSupportsInput &= FSlateApplication::Get().IsMouseAttached();
#endif
		}
		break;
		case ECommonInputType::Touch:
		{
			bPlatformSupportsInput &= UE_COMMONINPUT_PLATFORM_SUPPORTS_TOUCH != 0;
#if PLATFORM_DESKTOP && !UE_BUILD_SHIPPING
			bPlatformSupportsInput = true; // Enable touch in debug editor
#endif
		}
		break;
		case ECommonInputType::Gamepad:
#if PLATFORM_IOS
		{
			bool bAllowControllers = false;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
			bPlatformSupportsInput &= bAllowControllers;
		}
#elif PLATFORM_ANDROID
		{
			bool bAllowControllers = false;
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
			bPlatformSupportsInput &= bAllowControllers;
		}
#endif
		break;
	}

	GetOnPlatformInputSupportOverride().Broadcast(GetLocalPlayer(), InInputType, bPlatformSupportsInput);
	
	return bPlatformSupportsInput;
}

bool UCommonInputSubsystem::IsMobileGamepadKey(const FKey& InKey)
{
	// Mobile keys that can be physically present on the device
	static TArray<FKey> PhysicalMobileKeys = { 
		EKeys::Android_Back, 
		EKeys::Android_Menu, 
		EKeys::Android_Volume_Down, 
		EKeys::Android_Volume_Up 
	};

	return PhysicalMobileKeys.Contains(InKey);
}

