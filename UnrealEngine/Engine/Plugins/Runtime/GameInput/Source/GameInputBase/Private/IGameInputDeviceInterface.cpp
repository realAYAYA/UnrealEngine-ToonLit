// Copyright Epic Games, Inc. All Rights Reserved.

#include "IGameInputDeviceInterface.h"
#include "GameInputDeveloperSettings.h"
#include "GameInputLogging.h"
#include "GameInputUtils.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/IInputInterface.h"
#include "HAL/IConsoleManager.h"

#if GAME_INPUT_SUPPORT
namespace UE::GameInput
{
	static TAutoConsoleVariable<bool> CVarGameInputEnumerateDeviceTypeOnConnection
	(
		TEXT("gameinput.bEnumerateDeviceTypeOnConnection"),
		true,
		TEXT("If true, EnumerateCurrentlyConnectedDeviceTypes will be called upon device connect and disconnect"),
		ECVF_Default
	);
};
#endif	// GAME_INPUT_SUPPORT

IGameInputDeviceInterface::IGameInputDeviceInterface(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, IGameInput* InGameInput)
#if GAME_INPUT_SUPPORT
	: MessageHandler(InMessageHandler)
	, GameInput(InGameInput)
	, CurrentlyConnectedDeviceTypes(GameInputKindUnknown)
	, bWasinitialized(false)
	, bIsAppCurrentlyConstrained(false)
	, bWasAppConstrainedLastTick(false)
#endif	// GAME_INPUT_SUPPORT
{
}

IGameInputDeviceInterface::~IGameInputDeviceInterface()
{
#if GAME_INPUT_SUPPORT
	if (ConnectionChangeCallbackToken)
	{
		if (GameInput)
		{
			GameInput->UnregisterCallback(ConnectionChangeCallbackToken, UINT64_MAX);
			GameInput.Reset();
		}

		ConnectionChangeCallbackToken = GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE;
	}

	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
#endif	// GAME_INPUT_SUPPORT
}

void IGameInputDeviceInterface::Initialize()
{
#if GAME_INPUT_SUPPORT
	BindDeviceStatusCallbacks();
	bWasinitialized = true;
#endif	// GAME_INPUT_SUPPORT
}

void IGameInputDeviceInterface::Tick(float DeltaTime)
{
	// Required by IInputDevice
}

void IGameInputDeviceInterface::SendControllerEvents()
{
#if GAME_INPUT_SUPPORT

	ensureAlwaysMsgf(bWasinitialized, TEXT("A Game Input Device was not initalized. You must call IGameInputDeviceInterface::Initialize after creating a new device!"));

	FScopeLock Lock(&DeviceInfoCS);

	// At this point we should always have a valid Game Input object
	check(GameInput);

	// On the first update coming back from being constrained, we want to reset
	// the state of inputs
	if (bIsAppCurrentlyConstrained && !bWasAppConstrainedLastTick)
	{
		DetermineStateAfterFirstUnconstrainedUpdate();
	}

	// Process any input devices so long as we are not constrained
	if (!bIsAppCurrentlyConstrained)
	{
		// Handle any device connection/disconnection state changes first before attempting to process any devices
		ProcessDeferredDeviceConnectionChanges();
		
		// A map of Platform users to a bitmask of any reading kinds that were processed this frame.
		// This is used by the devices to keep track of which users had which readings, and use that
		// state to determine if we can process a given reading.
		static TMap<FPlatformUserId, GameInputKind> PlatformUsersWhoHaveHadInputThisFrame;
		PlatformUsersWhoHaveHadInputThisFrame.Reset();

		// The allowed input kinds that we can read from the IGameInput interface.
		const GameInputKind AllowedInputKindsThisFrame = GetCurrentGameInputKindSupport();

		for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
		{
			const FPlatformUserId UserId = KnownDevice->GetPlatformUserId();

			// Figure out what GameInputKind's this platform user has already handled this frame.
			GameInputKind& AlreadyProcessedInputKinds = PlatformUsersWhoHaveHadInputThisFrame.FindOrAdd(UserId, /* default init value */ GameInputKindUnknown);

			// Actually process our input here, and get a bitmask back of what GameInputKind's have sent events.
			const GameInputKind InputKindsWithEvents = KnownDevice->ProcessInput(GameInput, AllowedInputKindsThisFrame, AlreadyProcessedInputKinds);

			// Keep track what kinds of input that this platform user has processed this frame for use on the next device
			AlreadyProcessedInputKinds |= InputKindsWithEvents;
			
			// Keep track of the most recent device that is being used by each given platform user
			// We need to do this whenever we receive input, which is true as long as there was an input kind processed this frame.
			if (InputKindsWithEvents != GameInputKindUnknown)
			{
				// If we know input came from a device associated to this platform user already, then check our timestamp to see if it is newer then it
				if (const TSharedPtr<FGameInputDeviceContainer>* MostRecentKnownDevice = PlatformUserIdToMostRecentDeviceContainer.Find(UserId))
				{
					// If the current device that was just processed has a more recent timestamp then the known one, then
					// use it as our most recent device instead
					if (KnownDevice->GetLastReadingTimestamp() > (*MostRecentKnownDevice)->GetLastReadingTimestamp())
					{
						PlatformUserIdToMostRecentDeviceContainer[UserId] = KnownDevice;
					}
				}
				else
				{
					PlatformUserIdToMostRecentDeviceContainer.Add(UserId, KnownDevice);
				}
			}
		}
	}

	bWasAppConstrainedLastTick = bIsAppCurrentlyConstrained;

#endif// GAME_INPUT_SUPPORT
}

void IGameInputDeviceInterface::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
#if GAME_INPUT_SUPPORT
	MessageHandler = InMessageHandler;

	for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		KnownDevice->SetMessageHandler(InMessageHandler);
	}
#endif	// GAME_INPUT_SUPPORT
}

bool IGameInputDeviceInterface::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// required by IInputDevice interface
	return false;
}

void IGameInputDeviceInterface::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
#if GAME_INPUT_SUPPORT
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId UserId = PLATFORMUSERID_NONE;
	FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, UserId, DeviceId);

	if (!UserId.IsValid())
	{
		return;
	}

	GameInputRumbleParams RumbleParams = {};

	switch (ChannelType)
	{
	case FForceFeedbackChannelType::LEFT_LARGE:
		RumbleParams.lowFrequency = FMath::Clamp(Value, 0.0f, 1.0f);
		break;
	case FForceFeedbackChannelType::LEFT_SMALL:
		RumbleParams.leftTrigger = FMath::Clamp(Value, 0.0f, 1.0f);
		break;
	case FForceFeedbackChannelType::RIGHT_LARGE:
		RumbleParams.highFrequency = FMath::Clamp(Value, 0.0f, 1.0f);
		break;
	case FForceFeedbackChannelType::RIGHT_SMALL:
		RumbleParams.rightTrigger = FMath::Clamp(Value, 0.0f, 1.0f);
		break;
	}

	// Send a rumble event for every input device that is mapped to this platform user
	for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		if (DeviceMapper.GetUserForInputDevice(KnownDevice->GetDeviceId()) == UserId)
		{
			if (IGameInputDevice* Device = KnownDevice->GetGameInputDevice())
			{
				Device->SetRumbleState(&RumbleParams);
			}
		}
	}

#endif	// #if GAME_INPUT_SUPPORT
}

void IGameInputDeviceInterface::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
#if GAME_INPUT_SUPPORT
	// TODO: Allow native input device id's to FInputDeviceId's in the platform input device mapper
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId UserId = PLATFORMUSERID_NONE;
	FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, UserId, DeviceId);

	if (!UserId.IsValid())
	{
		return;
	}

	GameInputRumbleParams RumbleParams = {};

	RumbleParams.lowFrequency = FMath::Clamp(Values.LeftLarge, 0.0f, 1.0f);
	RumbleParams.leftTrigger = FMath::Clamp(Values.LeftSmall, 0.0f, 1.0f);
	RumbleParams.highFrequency = FMath::Clamp(Values.RightLarge, 0.0f, 1.0f);
	RumbleParams.rightTrigger = FMath::Clamp(Values.RightSmall, 0.0f, 1.0f);

	// Send a rumble event for every input device that is mapped to this platform user
	for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		if (DeviceMapper.GetUserForInputDevice(KnownDevice->GetDeviceId()) == UserId)
		{
			if (IGameInputDevice* Device = KnownDevice->GetGameInputDevice())
			{
				Device->SetRumbleState(&RumbleParams);
			}
		}
	}
#endif	// #if GAME_INPUT_SUPPORT
}

bool IGameInputDeviceInterface::IsGamepadAttached() const
{
#if GAME_INPUT_SUPPORT
	// We will treat both Gamepads and Controller's as "Gamepads" as far as UE is concerned. 
	return CurrentlyConnectedDeviceTypes & GameInputKindGamepad || CurrentlyConnectedDeviceTypes & GameInputKindController;
#else
	return false;
#endif	// GAME_INPUT_SUPPORT
}

#if GAME_INPUT_SUPPORT

GameInputKind IGameInputDeviceInterface::GetCurrentGameInputKindSupport() const
{
	GameInputKind RegisterInputKindMask = GameInputKindUnknown;
	
	const UGameInputPlatformSettings* PlatformSettings = UGameInputPlatformSettings::Get();
	
	if (PlatformSettings->bProcessController)
	{
		RegisterInputKindMask |= (GameInputKindController | GameInputKindControllerAxis | GameInputKindControllerButton | GameInputKindControllerSwitch);
	}

	if (PlatformSettings->bProcessRawInput)
	{
		RegisterInputKindMask |= GameInputKindRawDeviceReport;
	}

	if (PlatformSettings->bProcessGamepad)
	{
		RegisterInputKindMask |= GameInputKindGamepad;
	}

	if (PlatformSettings->bProcessKeyboard)
	{
		RegisterInputKindMask |= GameInputKindKeyboard;
	}

	if (PlatformSettings->bProcessMouse)
	{
		RegisterInputKindMask |= GameInputKindMouse;
	}

	if (PlatformSettings->bProcessRacingWheel)
	{
		RegisterInputKindMask |= GameInputKindRacingWheel;
	}

	// TODO: Future expansion of GameInput devices!
	/*if (Settings->bProcessArcadeStick)
	{

	}

	if (Settings->bProcessRacingWheel)
	{

	}	
	*/

	return RegisterInputKindMask;
}

bool IGameInputDeviceInterface::BindDeviceStatusCallbacks()
{
	if (!GameInput)
	{
		return false;
	}
	
	// First, check if we are already bound to something. If so, unbind it
	if (ConnectionChangeCallbackToken)
	{
		GameInput->UnregisterCallback(ConnectionChangeCallbackToken, UINT64_MAX);
		ConnectionChangeCallbackToken = GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE;
	}

	// Hook up device callbacks 
	auto DeviceCallbackFn = [](GameInputCallbackToken CallbackToken, void* Context, IGameInputDevice* Device, uint64 Timestamp, GameInputDeviceStatus CurrentStatus, GameInputDeviceStatus PreviousStatus)
	{
		static_cast<IGameInputDeviceInterface*>(Context)->OnDeviceConnectionChanged(CallbackToken, Device, Timestamp, CurrentStatus, PreviousStatus);
	};

	// TODO: should this be a member variable that subclasses can override? That might be desirable on PC
	static constexpr GameInputDeviceStatus DeviceStatusMask = (GameInputDeviceNoStatus | GameInputDeviceConnected);
	
	const GameInputKind RegisterInputKindMask = GetCurrentGameInputKindSupport();

	UE_LOG(LogGameInput, Log, TEXT("Registering Device Callback for GameInputKind: '%s'. Listening for Device Status: '%s'."), *UE::GameInput::LexToString(RegisterInputKindMask), *UE::GameInput::LexToString(DeviceStatusMask));
	
	GameInput->RegisterDeviceCallback(nullptr, RegisterInputKindMask, DeviceStatusMask, GameInputBlockingEnumeration, this, DeviceCallbackFn, &ConnectionChangeCallbackToken);

	// get notified when the app is constrained & unconstrained
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &IGameInputDeviceInterface::OnAppConstrained);
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &IGameInputDeviceInterface::OnAppUnconstrained);

	// We are successful if the callback token is valid
	return ConnectionChangeCallbackToken != GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE;
}

void IGameInputDeviceInterface::OnDeviceConnectionChanged(GameInputCallbackToken CallbackToken, IGameInputDevice* Device, uint64 Timestamp, GameInputDeviceStatus CurrentStatus, GameInputDeviceStatus PreviousStatus)
{
	FScopeLock Lock(&DeviceInfoCS);

	UE_LOG(LogGameInput, Log, TEXT("Device Connection Changed from '%s' to '%s'"), *UE::GameInput::LexToString(PreviousStatus), *UE::GameInput::LexToString(CurrentStatus));
	
	// This event may come in async from GameInput from outside the game thread, so defer it until Tick so we can guarantee game thread access
	IGameInputDeviceInterface::FDeferredDeviceConnectionChanges Event = {};
	Event.Device = Device;
	Event.Timestamp = Timestamp;
	Event.Status = UE::GameInput::DeviceStateToConnectionState(CurrentStatus, PreviousStatus);
	Event.CurrentStatus = CurrentStatus;
	Event.PreviousStatus = PreviousStatus;

	DeferredDeviceConnectionChanges.Emplace(Event);
}

void IGameInputDeviceInterface::ProcessDeferredDeviceConnectionChanges()
{
	// We only want to actually handle device connection events in the game thread
	// because a lot of listeners will be in game or ui code that reference the FSlateApplication::Get function.
	check(IsInGameThread());
	if (!DeferredDeviceConnectionChanges.IsEmpty())
	{
		for (const FDeferredDeviceConnectionChanges& Event : DeferredDeviceConnectionChanges)
		{
			// No status means that the device has been disconnected
			if (Event.Status == EInputDeviceConnectionState::Disconnected)
			{
				HandleDeviceDisconnected(Event.Device, Event.Timestamp);
			}
			else if (Event.Status == EInputDeviceConnectionState::Connected)
			{
				HandleDeviceConnected(Event.Device, Event.Timestamp);
			}
			else
			{
				UE_LOG(LogGameInput, Error, TEXT("Unexpected state change for device %s (%s -> %s"), *UE::GameInput::LexToString(Event.Device), *UE::GameInput::LexToString(Event.PreviousStatus), *UE::GameInput::LexToString(Event.CurrentStatus));
			}
		}
		DeferredDeviceConnectionChanges.Reset();
	}
}

void IGameInputDeviceInterface::OnAppConstrained()
{
	bIsAppCurrentlyConstrained = true;
}

void IGameInputDeviceInterface::OnAppUnconstrained()
{
	bIsAppCurrentlyConstrained = false;
}

void IGameInputDeviceInterface::DetermineStateAfterFirstUnconstrainedUpdate()
{
	// This should only be called on the first update after coming back from being constrained
	ensure(bIsAppCurrentlyConstrained && !bWasAppConstrainedLastTick);

	for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		KnownDevice->ClearInputState(GameInput);
	}	
}

FGameInputDeviceContainer* IGameInputDeviceInterface::GetDeviceData(IGameInputDevice* InDevice)
{
	// Check if we already know about the given device
	if (!InDevice)
	{
		return nullptr;
	}

	// Check if we have seen this device's APP_LOCAL_DEVICE_ID before. The IGameInputDevice* could have been invalidated if the device 
	// is being re-connected, but it will have the APP_LOCAL_DEVICE_ID would be the same.
	const GameInputDeviceInfo* Info = InDevice->GetDeviceInfo();

	FGameInputDeviceContainer* RetVal = nullptr;

	for (const TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		const APP_LOCAL_DEVICE_ID KnownDeviceId = KnownDevice->GetGameInputDeviceId();

		if (KnownDevice->GetGameInputDevice() == InDevice ||
			(Info && FMemory::Memcmp(&Info->deviceId, &KnownDeviceId, sizeof(KnownDeviceId)) == 0))
		{
			RetVal = KnownDevice.Get();
			break;
		}
	}

	return RetVal;
}

FGameInputDeviceContainer* IGameInputDeviceInterface::GetOrCreateDeviceData(IGameInputDevice* InDevice)
{
	// Check if we already know about the given device
	if (!InDevice)
	{
		return nullptr;
	}

	// Check if we already have some device data about this...
	if (FGameInputDeviceContainer* ExistingDevice = GetDeviceData(InDevice))
	{
		// For existing devices, we want to ensure that their IGameInputDevice pointer matches up with what was given.
		// This may be the case if you disconnect and then reconnect a device, because we can still find it's associated
		// FGameInputDeviceContainer based on the  APP_LOCAL_DEVICE_ID, but the IGameInputDevice pointer would be null.
		ExistingDevice->SetGameInputDevice(InDevice);

		return ExistingDevice;
	}

	// ... if not, then create a new one.
	return CreateDeviceData(InDevice);
}

void IGameInputDeviceInterface::EnumerateCurrentlyConnectedDeviceTypes()
{
	if (!UE::GameInput::CVarGameInputEnumerateDeviceTypeOnConnection.GetValueOnAnyThread())
	{
		return;
	}

	CurrentlyConnectedDeviceTypes = GameInputKindUnknown;

	// Check all of our devices and their supported input flags
	for (TSharedPtr<FGameInputDeviceContainer>& KnownDevice : DeviceData)
	{
		if (KnownDevice)
		{
			if (IGameInputDevice* Device = KnownDevice->GetGameInputDevice())
			{
				if (const GameInputDeviceInfo* Info = Device->GetDeviceInfo())
				{
					CurrentlyConnectedDeviceTypes |= Info->supportedInput;
				}
			}
		}
	}
}

#endif	// GAME_INPUT_SUPPORT
