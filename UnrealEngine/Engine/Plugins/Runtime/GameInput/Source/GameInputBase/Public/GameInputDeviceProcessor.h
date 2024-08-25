// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Misc/CoreMiscDefines.h"	// For FInputDeviceId/FPlatformUserId
#include "GenericPlatform/GenericApplicationMessageHandler.h"

#if GAME_INPUT_SUPPORT
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <GameInput.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#include "Microsoft/COMPointer.h"
#endif	// GAME_INPUT_SUPPORT

#if GAME_INPUT_SUPPORT

/**
* A Game Input Device Processor is used to determine the state of a device
* each frame and send messages to the given Message Handler when we want to. 
* Each processor represents a single "kind" of Game Input device. 
* 
* They are created by the owning GameInput Device Container (GameInputDeviceContainer.h)
* and polled each frame when possible. Each Human Interface Device can be made up of multiple 
* Game Input Kinds, so a single device container may have multiple different processors associated with it.
*/
class GAMEINPUTBASE_API IGameInputDeviceProcessor
{
public:
	IGameInputDeviceProcessor() = default;

	virtual ~IGameInputDeviceProcessor() = default;

	struct GAMEINPUTBASE_API FGameInputEventParams
	{
		/** The current reading for this input event */
		IGameInputReading* Reading = nullptr;

		/** The previous reading from the last frame. */
		IGameInputReading* PreviousReading = nullptr;

		/** Device associated with this reading */
		IGameInputDevice* Device = nullptr;

		/** The message handler to use to send any input events */
		TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;

		/** The platform user that is associated with the input device that triggered this event */
		FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;

		/** The Input device ID of the device that triggered this event. */
		FInputDeviceId InputDeviceId = INPUTDEVICEID_NONE;

		/** Gets the Input Device Info for this GameInput Device if it is valid. Can be null. */
		const GameInputDeviceInfo* GetDeviceInfo() const;
	};

	/** 
	* Process any input events from the given reading and send events to the message handler
	* using the given Platform User and Input Device Id's.
	* 
	* This can be called multiple times per frame, like if we are running at a lower FPS.
	* This is because GameInput may accumulate multiple readings to use by the time the game thread gets here.
	* 
	* If you have inputs that you only want to process once per frame, then use PostProcessInput instead.
	* 
	* @return True if any input events have been processed, false if nothing happened
	*/
	virtual bool ProcessInput(const FGameInputEventParams& Params) = 0;

	/**
	* Called after all current Game Input readings have been processed
	* this frame and there are no more readings for a device (GetNextReading returns GAMEINPUT_E_READING_NOT_FOUND). 
	* This is called regardless of if an accompanying ProcessInput call has happened.
	* 
	* This function is useful for dealing with multiple readings in a single frame for analog devices. 
	* It is recommended that processors override this function and only process analog input events here at
	* the end of the frame, once, instead of multiple times in ProcessInput. That is because at lower frame rates
	* or hitches you will end up with more then one GameInput reading in the stack that we need to process. If you were to
	* process all of those analog inputs in a single frame, the values would accumulate to above or below +-1.0,
	* which could have unexpected behavior in Slate.
	* 
	* @param Params		Game Input event params sent from the owning FGameInputDeviceContainer.
	*					Params.Reading should always be null here, as there is no current reading.
	*					Params.PreviousReading will contain the most recent Game Input reading from the input stack.
	* 
	* @return			True if any input events have been processed, false if nothing happened
	*/
	virtual bool PostProcessInput(const FGameInputEventParams& Params);

	/** 
	* Clear any input state related to this processor, typically by sending events to the 
	* message handler that each FKey related to this processor now has a value of 0.
	*/
	virtual void ClearState(const FGameInputEventParams& Params) = 0;

	/*
	* Returns the kind of reading that this processor supports. If the current GameInput reading 
	* is not included in this bitmask, then this processor will not not have it's "ProcessInput" 
	* function called.
	* 
	* @see FGameInputDeviceContainer::ProcessInput
	*/
	virtual GameInputKind GetSupportedReadingKind() const = 0;

protected:	

	/** 
	* Returns the string hardware device identifier for the given input event params that can be used
	* to create an FInputDeviceScope
	*/
	const FString& GetHardwareDeviceIdentifierName(const IGameInputDeviceProcessor::FGameInputEventParams& Params) const;

	/** A general use function to call the message handler and tell it about a controller analog key being used */
	void OnControllerAnalog(const FGameInputEventParams& Params, const FName& GamePadKey, float NewAxisValueNormalized, float OldAxisValueNormalized, float DeadZone);

	/**
	 * Helper function for processing the button states of Game input.
	 *
	 * @param CurrentButtonHeldMask			The current state of the button mask to evaluate
	 * @param PreviousButtonMask		The previous state of the button mask. This value will be modified to store the current button mask after evaluation
	 * @param RepeatTime				A map of the button index to a time that it was last active, used to determine if the key meets the qualifications for a repeat event
	 * @param UnrealButtonNameMap		A map of Unreal Engine Gamepad key names to their uint32 mapping from Game Input.
	 * @param SupportedButtonCount		The maximum number of buttons we can process
	 */
	void EvaluateButtonStates(
		const FGameInputEventParams& Params,
		const uint32 CurrentButtonHeldMask,
		uint32& PreviousButtonMask,
		double* RepeatTime,
		const TMap<uint32, FGamepadKeyNames::Type>& UnrealButtonNameMap,
		const uint32 SupportedButtonCount);

	/**
	* Helper function to processing the state of a Switch position (aka a DPad, like left/right/up/down)
	* 
	* @param Params		The GameInput event params
	* @param CurrentPosition	The current position of the switch
	* @param PreviousPosition	The previous position of the switch. This will be set to the value of the current switch at the end of this function
	* @param RepeatTimes		Array of doubles that represent the time at which a switch was last pressed that we can use to check for when to send repeat events
	*/
	void EvaluateSwitchState(
		const FGameInputEventParams& Params,
		GameInputSwitchPosition CurrentPosition,
		GameInputSwitchPosition& PreviousPosition,
		TArray<double>& RepeatTimes);
};

/**
* Processor for Gamepad Inputs
*/
class GAMEINPUTBASE_API FGameInputGamepadDeviceProcessor : public IGameInputDeviceProcessor
{
protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual bool PostProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	bool ProcessGamepadAnalogState(const FGameInputEventParams& Params, GameInputGamepadState& GamepadState);
	virtual bool ProcessGamepadButtonState(const FGameInputEventParams& Params, GameInputGamepadState& GamepadState);

	GameInputGamepadState PreviousState = {};

	uint32 LastButtonHeldMask = 0;

	enum { MaxSupportedButtons = 32 };

	double RepeatTime[MaxSupportedButtons];

	/** 
	* Keeps track of how many times this gamepad has been processed this frame. 
	* Every successful processing of button input in ProcessInput will increment this value.
	* This will get reset at the end of the input frame in PostProcessInput.
	*/
	int32 NumReadingsProcessedThisFrame = 0;
};

struct FGameInputDeviceConfiguration;

/**
* Processor for Controller Input Types
* 
* Controllers are very similar to Gamepad types, and often can just be 
* a third party controller type that is not natively supported by the "GamepadInputKind".
* This can include things like instruments, other platform controller types, or other 
* third party input devices.
*/
class GAMEINPUTBASE_API FGameInputControllerDeviceProcessor : public IGameInputDeviceProcessor
{
public:
	FGameInputControllerDeviceProcessor();

protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual bool PostProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	/** Processes Controller buttons (often face buttons, like ABXY or Circle/Triangle/Square/X) */
	bool ProcessControllerButtonState(const FGameInputEventParams& Params, const FGameInputDeviceConfiguration* Config, IGameInputReading* InputReading);

	/**
	* Process any axis readings on this controller.
	* This will only process axises that are configured in the given Config.
	*/
	bool ProcessControllerAxisState(const FGameInputEventParams& Params, const FGameInputDeviceConfiguration* Config, IGameInputReading* InputReading);

	/**
	* Process the controller switch (also known as the DPad) state
	*/
	bool ProcessControllerSwitchState(const FGameInputEventParams& Params, const FGameInputDeviceConfiguration* Config, IGameInputReading* InputReading);

	/** A map of GameInputLabel types to Unreal Engine FKey names. */
	static const TMap<GameInputLabel, FGamepadKeyNames::Type>& GetGameInputButtonLabelToUnrealName();

	/** Bitmask of any held buttons from the last frame */
	uint32 LastButtonHeldMask = 0;

	/** Max number of buttons this processor supports */
	enum { MaxSupportedButtons = 32 };

	/** Timings of when a key press should be considered a "repeat" key */
	double RepeatTime[MaxSupportedButtons];

	/** Repeat times for when switches are pressed (aka the DPad) */
	TArray<double> SwitchRepeatTimes;

	/** Previous frame's axis values */
	TArray<float> PreviousControllerAxisValues;

	/** Array of switch positions */
	TArray<GameInputSwitchPosition> PreviousSwitchPositions;

	/**
	* Keeps track of how many times this gamepad has been processed this frame.
	* Every successful processing of button input in ProcessInput will increment this value.
	* This will get reset at the end of the input frame in PostProcessInput.
	*/
	int32 NumReadingsProcessedThisFrame = 0;
};

/**
* Processor for Keyboard inputs
*/
class GAMEINPUTBASE_API FGameInputKeyboardDeviceProcessor : public IGameInputDeviceProcessor
{
protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	void UpdateUnifiedKeyboardState(const FGameInputEventParams& Params, TSet<uint8>& CurrentPressedKeys);
	
	inline bool IsKeyPressed(uint8 VirtualKeyCode) const { return LastPressedKeys.Contains(VirtualKeyCode); }

	virtual void SetSimulatedCapsLock(bool bVal);

	TSet<uint8> LastPressedKeys;	

	TMap<uint8, double> KeyRepeatTime;	

	// cannot read actual caps lock state, so necessary to simulate it as a bool toggle
	bool bSimulatedCapsLock;
};


/**
* Processor for mouse inputs
*/
class GAMEINPUTBASE_API FGameInputMouseDeviceProcessor : public IGameInputDeviceProcessor
{
public:
	explicit FGameInputMouseDeviceProcessor(const TSharedPtr<class ICursor>& InCursor);

protected:	
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	float CalculateMouseAcceleration(int32 Delta);

	virtual bool CanProcessVirtualMouse() const;

	TSharedPtr<class ICursor> Cursor;

	GameInputMouseState PreviousMouseState;

	FVector2D LastMouseOffset;

	enum { MaxSupportedButtons = 32 };
	double RepeatTime[MaxSupportedButtons];

	uint32 LastButtonHeldMask;
};


/**
* Processor for touch inputs
*/
class GAMEINPUTBASE_API FGameInputTouchDeviceProcessor : public IGameInputDeviceProcessor
{
protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	struct FTouchData
	{
		FTouchData()
		{
			TouchId = INDEX_NONE;
			Position = FVector2D::ZeroVector;
			Pressure = 0.0f;
			bHasMoved = false;
			bIsActive = false;
		}

		int64 TouchId;
		FVector2D Position;
		float Pressure;
		bool bHasMoved;
		bool bIsActive;
		};
	TArray<FTouchData> PreviousTouchData;
	int32 ActiveTouchPoints;

	// The max touch X and Y of the touch region to let us scale input based on the size of the touch area
	int32 MaxTouchX = 1920;
	int32 MaxTouchY = 1080;
};

struct FGameInputDeviceConfiguration;
struct FGameInputRawDeviceReportData;

/**
* Processor for raw input types (GameInputKindRawDeviceReport)
* 
* Raw input is very customizable and is very different per-input device, so it requires a lot of configuration.
* Most of the time it is special analog inputs on custom third party devices. A good example is the Whammy bar on
* a guitar controller, or maybe some special shifter on a racing wheel that the Game Input API does not pick up by default.
*/
class GAMEINPUTBASE_API FGameInputRawDeviceProcessor : public IGameInputDeviceProcessor
{
protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual bool PostProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	/**
	* Reads the current state of the raw input buffer and populates CurrentRawData and PreviousRawData
	*/
	const GameInputRawDeviceReportInfo* ReadCurrentRawInputState(const FGameInputEventParams& Params, IGameInputReading* ReadingToUse);


	/**
	* Translates the given uint8 RawValue to a float between 0.0 and +1.0, like most gamepad analog "triggers"
	* 
	* @param RawValue
	* 
	* @return			A float between 0.0 and +1.0
	*/
	constexpr float RawValueToFloatTrigger(const uint8 RawValue) const;

	/**
	* Translates the given uint8 RawValue to a float between -1.0 and +1.0.
	* 
	* @param RawValue
	* @param Deadzone
	* 
	* @return A float between -1.0 and +1.0. Will be 0.0 if the value is within the deadzone.
	*/
	const float RawValueToFloatAnalog(uint8 RawValue, const uint8 DeadZone = 2) const;

	/**
	* Processes all the current raw input data. This should only be called after we have read our current raw input report
	* or we have manually set the values (i.e. when we clear the input)
	*/
	bool ProcessAllRawValues(const FGameInputEventParams& Params, const FGameInputDeviceConfiguration* DeviceConfig, const bool bShouldProcessButtons, const bool bShouldProcessAnalog);

	/**
	* Processes the raw value at the given index as a button bitmask. 
	* For every bit in the raw value and treat it as 0 for pressed and 1 for on. 
	*/
	bool ProcessRawInputValueAsBitmask(const FGameInputEventParams& Params, const int32 RawValueIndex, const FGameInputRawDeviceReportData* AxisData);

	/**
	* Processes the raw value at the given index as a button value. If the value is non-zero, then treat it as pressed. 
	* If the value is zero, treat it as unpressed.
	*/
	bool ProcessRawInputValueAsButton(const FGameInputEventParams& Params, const int32 RawValueIndex, const FGameInputRawDeviceReportData* AxisData);

	/**
	* Processes the raw value at the given index as an analog value. This will translate the uint8 to a float value 
	* and call OnControllerAnalog for it.
	*/
	bool ProcessRawInputValueAsAanalog(const FGameInputEventParams& Params, const int32 RawValueIndex, const FGameInputRawDeviceReportData* AxisData);

	/**
	* Processes a pair of raw input indexes. This allows you to combine the values of multiple uint8 axis readings and combine them
	*/
	bool ProcessRawInputValueAsAanalogPaired(const FGameInputEventParams& Params, const FGameInputRawDeviceReportData* AxisData);

	/**
	* The current raw input data that is populated by IGameInputRawDeviceReport::GetRawData
	*/
	TArray<uint8> CurrentRawData;

	/**
	* The previous frame's raw input data that is set at the end of ProcessInput.
	*/
	TArray<uint8> PreviousRawData;

	/** A struct that contains info about each raw input index */
	struct FPerRawInputIndexData
	{
		// The max number of buttons possibly associated with this raw input data. The raw input data is
		// a uint8, so we can have at maximum a button per-bit, so 8.
		enum { MaxSupportedButtons = 8 };

		// Stores the times that each button index has been pressed so we can detect key repeats
		double RepeatTime[MaxSupportedButtons];

		/** 
		* A map of key names to int values that is generated dynamically based on the button mapping.
		*/
		TMap<uint32, FName> KeyNameMap;
	};

	/**
	* A map of an index to some raw data associated with it. 
	* This is only used for button evaluation, not analog.
	*/
	TMap<int32, FPerRawInputIndexData> RawInputIndexDataMap;

	/**
	* Keeps track of how many times this gamepad has been processed this frame.
	* Every successful processing of button input in ProcessInput will increment this value.
	* This will get reset at the end of the input frame in PostProcessInput.
	*/
	int32 NumReadingsProcessedThisFrame = 0;
};

/**
* Processor for racing wheel types (GameInputKindRacingWheel)
* 
* Note: Racing wheels are often paired with other device processors like Gamepad/Controller
* to handle any "normal" buttons that may be on them.
*/
class GAMEINPUTBASE_API FGameInputRacingWheelProcessor : public IGameInputDeviceProcessor
{
public:
	FGameInputRacingWheelProcessor();
protected:
	virtual bool ProcessInput(const FGameInputEventParams& Params) override;
	virtual bool PostProcessInput(const FGameInputEventParams& Params) override;
	virtual void ClearState(const FGameInputEventParams& Params) override;
	virtual GameInputKind GetSupportedReadingKind() const override;

	/**
	* Returns the deadzone that we would like to use for racing wheel analog inputs.
	*/
	static const float GetRacingWheelDeadzone();

	/**
	* Processes the analog inputs of the wheel state (brake/clutch pedals, wheel movement, etc).
	*/
	bool ProcessWheelAnalogState(const FGameInputEventParams& Params, GameInputRacingWheelState& CurrentWheelState);

	/**
	* Processes any buttons on the racing wheel (next/previous gears, menu nav, etc);
	*/
	bool ProcessWheelButtonState(const FGameInputEventParams& Params, GameInputRacingWheelState& CurrentWheelState);

	/**
	* Keeps track of how many times this gamepad has been processed this frame.
	* Every successful processing of button input in ProcessInput will increment this value.
	* This will get reset at the end of the input frame in PostProcessInput.
	*/
	int32 NumReadingsProcessedThisFrame = 0;

	/** The previous game input state that was last processed. */
	GameInputRacingWheelState PreviousState;

	/** 
	* Array of repeat times to calculate if a button has been held long enough
	* to receive an IE_REPEAT event.
	*/
	static const uint32 MaxSupportedButtons = 16;
	double RepeatTime[MaxSupportedButtons];
};

#endif	// GAME_INPUT_SUPPORT