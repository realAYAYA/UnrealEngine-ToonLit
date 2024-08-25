// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDevice.h"
#include "Misc/CoreMiscDefines.h"	// For FInputDeviceId/FPlatformUserId

#if GAME_INPUT_SUPPORT
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <GameInput.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#include "Microsoft/COMPointer.h"
#endif	// GAME_INPUT_SUPPORT

#include "GameInputDeviceContainer.h"

/**
* IInputDevice interface for the Game Input library. This is a base class
* that is intended to be implemented on each platform that supports it (Xbox and PC).
* 
* This stores all state about every unique Human Interface Device that the GameInput library
* detects. 
* 
* Any implementors of this interface should be Created in an IInputDevice module's CreateInputDevice function
* (IInputDeviceModule::CreateInputDevice), and have it's Initialize function called
*/
class GAMEINPUTBASE_API IGameInputDeviceInterface : public IInputDevice
{
public:
	explicit IGameInputDeviceInterface(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, struct IGameInput* InGameInput);
	virtual ~IGameInputDeviceInterface() override;
	
	/** 
	* Init function that should be called post-construction of this object. 
	* This is required for sending controller events and will ensure if not called.
	*/
	void Initialize();

protected:

	//~ Begin IInputDevice interface
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values) override;
	virtual bool IsGamepadAttached() const override;
	//~End IInputDevice interface

#if GAME_INPUT_SUPPORT

	/** Returns a mask of the currently supported Game Input Kind that we want to get the reading of. */
	virtual GameInputKind GetCurrentGameInputKindSupport() const;

	/** Binds a listener for device status changes from game input to this class */
	virtual bool BindDeviceStatusCallbacks();
	
	/**
	 * A callback for when an input device connection change has been detected.
	 */
	virtual void OnDeviceConnectionChanged(GameInputCallbackToken CallbackToken, IGameInputDevice* Device, uint64 Timestamp, GameInputDeviceStatus CurrentStatus, GameInputDeviceStatus PreviousStatus);

	/**
	 * Mark this device as disconnected and invalidate the tracked game input device. A disconnected device will stay in the
	 * GameInputSourceDevices with a null Device pointer.
	 */
	virtual void HandleDeviceDisconnected(IGameInputDevice* Device, uint64 Timestamp) = 0;

	/**
	 * Map this new input device to a platform user and initialize the known FGameInputDeviceInfo.
	 * If this is a new connection, this will request a new FInputDeviceId from the platform input device mapper
	 * and add it to the GameInputSourceDevices map.
	 */
	virtual void HandleDeviceConnected(IGameInputDevice* Device, uint64 Timestamp) = 0;

	/** Process any deferred device connection changes that may have occurred. This should only be called on the game thread. */
	void ProcessDeferredDeviceConnectionChanges();
	
	/** Callbacks for when the app is constrained/unconstrained (i.e. you Alt + Tab out of the window, or open the home menu on a console platform) */
	void OnAppConstrained();
	void OnAppUnconstrained();
	
	/** Called on the first update after the was un-constrained. */
	void DetermineStateAfterFirstUnconstrainedUpdate();

	/** Returns a matching FGameInputDeviceContainer if one exists for the given GameInput device. Nullptr if none exist. */
	FGameInputDeviceContainer* GetDeviceData(IGameInputDevice* InDevice);

	/**
	* Returns a pointer to the Device info associated with the given IGameInputDevice.
	*
	* If one doesn't exist, then create a new one and return that.
	*/
	FGameInputDeviceContainer* GetOrCreateDeviceData(IGameInputDevice* InDevice);

	/**
	* Creates a new FGameInputDeviceContainer* and stores it in DeviceData. It is up to each implementing 
	* device interface to how that device is mapped to platform users and initialized.
	*/
	virtual FGameInputDeviceContainer* CreateDeviceData(IGameInputDevice* InDevice) = 0;

	/**
	* This will update the CurrentlyConnectedDeviceTypes bitmask based on the current set of DeviceData
	*/
	virtual void EnumerateCurrentlyConnectedDeviceTypes();
	
	/** Message handler that we can use to tell the engine about input events */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;
	
	/** Pointer to the Game Input interface object */
	TComPtr<IGameInput> GameInput;

	// CS for OnDeviceConnectionChanged called from background system thread and PollGameDeviceState called from Main thread
	FCriticalSection DeviceInfoCS;

	/** Token for when a device connection change happens */
	GameInputCallbackToken ConnectionChangeCallbackToken;
	
	/** An enumeration of all the currently registered Game input Devices. */
	GameInputKind CurrentlyConnectedDeviceTypes;

	/** Set to true if the IGameInputDeviceInterface::Initialize function was called. This is required for sending controller events. */
	bool bWasinitialized;

	/** True if the app is currently constrained this tick */
	bool bIsAppCurrentlyConstrained;

	/** True if the app was constrained last tick */
	bool bWasAppConstrainedLastTick;

	// Array of Input Device data for every connected Game Input Device.
	TArray<TSharedPtr<FGameInputDeviceContainer>> DeviceData;

	// A mapping of a platform user to the most recently used Input Device for that user
	TMap<FPlatformUserId, TSharedPtr<FGameInputDeviceContainer>> PlatformUserIdToMostRecentDeviceContainer;	

	/** Stores some data about when device connection changes happen to be processed later */
	struct FDeferredDeviceConnectionChanges
	{
		FDeferredDeviceConnectionChanges() = default;

		/** The GameInput device that has changed */
		IGameInputDevice* Device = nullptr;
		
		/** Timestamp of when the connection state change happened */
		uint64 Timestamp = 0;
		
		/** The evaluated status of the game input device */
		EInputDeviceConnectionState Status = EInputDeviceConnectionState::Invalid;
		
		/** The current GameInput status of the device */
		GameInputDeviceStatus CurrentStatus = GameInputDeviceStatus::GameInputDeviceNoStatus;

		/** The previous GameInput status of the device */
		GameInputDeviceStatus PreviousStatus = GameInputDeviceStatus::GameInputDeviceNoStatus;
	};

	/** Array of device connection changed events that will be processed in ProcessDeferredDeviceConnectionChanges. */
	TArray<FDeferredDeviceConnectionChanges> DeferredDeviceConnectionChanges;

#endif	// #if GAME_INPUT_SUPPORT
};