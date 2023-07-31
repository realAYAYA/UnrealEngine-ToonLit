// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GenericPlatform/GenericApplicationMessageHandler.h"

#include <android/input.h>
#include <android/keycodes.h>
#include <android/api-level.h>
#include "GenericPlatform/ICursor.h"
#include "GenericPlatform/IInputInterface.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Color.h"

#if __ANDROID_API__ < 13

// Joystick functions and constants only available API level 13 and above
// Definitions are provided to allow compiling against lower API levels, but
// still using the features when available.

enum
{
	AMOTION_EVENT_AXIS_X = 0,
	AMOTION_EVENT_AXIS_Y = 1,
	AMOTION_EVENT_AXIS_PRESSURE = 2,
	AMOTION_EVENT_AXIS_SIZE = 3,
	AMOTION_EVENT_AXIS_TOUCH_MAJOR = 4,
	AMOTION_EVENT_AXIS_TOUCH_MINOR = 5,
	AMOTION_EVENT_AXIS_TOOL_MAJOR = 6,
	AMOTION_EVENT_AXIS_TOOL_MINOR = 7,
	AMOTION_EVENT_AXIS_ORIENTATION = 8,
	AMOTION_EVENT_AXIS_VSCROLL = 9,
	AMOTION_EVENT_AXIS_HSCROLL = 10,
	AMOTION_EVENT_AXIS_Z = 11,
	AMOTION_EVENT_AXIS_RX = 12,
	AMOTION_EVENT_AXIS_RY = 13,
	AMOTION_EVENT_AXIS_RZ = 14,
	AMOTION_EVENT_AXIS_HAT_X = 15,
	AMOTION_EVENT_AXIS_HAT_Y = 16,
	AMOTION_EVENT_AXIS_LTRIGGER = 17,
	AMOTION_EVENT_AXIS_RTRIGGER = 18,
	AMOTION_EVENT_AXIS_THROTTLE = 19,
	AMOTION_EVENT_AXIS_RUDDER = 20,
	AMOTION_EVENT_AXIS_WHEEL = 21,
	AMOTION_EVENT_AXIS_GAS = 22,
	AMOTION_EVENT_AXIS_BRAKE = 23,
	AMOTION_EVENT_AXIS_DISTANCE = 24,
	AMOTION_EVENT_AXIS_TILT = 25,
	AMOTION_EVENT_AXIS_GENERIC_1 = 32,
	AMOTION_EVENT_AXIS_GENERIC_2 = 33,
	AMOTION_EVENT_AXIS_GENERIC_3 = 34,
	AMOTION_EVENT_AXIS_GENERIC_4 = 35,
	AMOTION_EVENT_AXIS_GENERIC_5 = 36,
	AMOTION_EVENT_AXIS_GENERIC_6 = 37,
	AMOTION_EVENT_AXIS_GENERIC_7 = 38,
	AMOTION_EVENT_AXIS_GENERIC_8 = 39,
	AMOTION_EVENT_AXIS_GENERIC_9 = 40,
	AMOTION_EVENT_AXIS_GENERIC_10 = 41,
	AMOTION_EVENT_AXIS_GENERIC_11 = 42,
	AMOTION_EVENT_AXIS_GENERIC_12 = 43, 
	AMOTION_EVENT_AXIS_GENERIC_13 = 44,
	AMOTION_EVENT_AXIS_GENERIC_14 = 45,
	AMOTION_EVENT_AXIS_GENERIC_15 = 46,
	AMOTION_EVENT_AXIS_GENERIC_16 = 47,
};
enum
{
	AINPUT_SOURCE_CLASS_JOYSTICK = 0x00000010,
};
enum
{
	AINPUT_SOURCE_GAMEPAD = 0x00000400 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_JOYSTICK = 0x01000000 | AINPUT_SOURCE_CLASS_JOYSTICK,
};
#endif // __ANDROID_API__ < 13


enum TouchType
{
	TouchBegan,
	TouchMoved,
	TouchEnded,
};

enum MappingState
{
	Unassigned,
	ToValidate,
	Valid
};

enum ControllerClassType
{
	Generic,
	XBoxWired,
	XBoxWireless,
	PlaystationWireless
};

enum ButtonRemapType
{
	Normal,
	XBox,
	PS4,
	PS5,
	PS5New
};

struct FAndroidInputDeviceInfo {
	int32 DeviceId;
	int32 VendorId;
	int32 ProductId;
	int32 ControllerId;
	FString Name;
	FString Descriptor;
};

struct FAndroidGamepadDeviceMapping
{
	// Information for this mapped device
	FAndroidInputDeviceInfo DeviceInfo;

	// State of mapping
	MappingState DeviceState;

	// Type of controller
	ControllerClassType ControllerClass;

	// Type of button remapping to use
	ButtonRemapType ButtonRemapping;

	// Sets the analog range of the trigger minimum (normally 0).  Final value is mapped as (input - Minimum) / (1 - Minimum) to [0,1] output.
	float LTAnalogRangeMinimum;
	float RTAnalogRangeMinimum;

	// Device supports hat as dpad
	bool bSupportsHat;

	// Device uses threshold to send button pressed events.
	bool bTriggersUseThresholdForClick;

	// Map L1 and R1 to LTRIGGER and RTRIGGER
	bool bMapL1R1ToTriggers;

	// Map Z and RZ to LTAnalog and RTAnalog
	bool bMapZRZToTriggers;

	// Right stick on Z/RZ
	bool bRightStickZRZ;

	// Right stick on RX/RY
	bool bRightStickRXRY;

	// Map RX and RY to LTAnalog and RTAnalog
	bool bMapRXRYToTriggers;
};

struct TouchInput
{
	int32 DeviceId;
	int32 Handle;
	TouchType Type;
	FVector2D LastPosition;
	FVector2D Position;
};

#define MAX_NUM_CONTROLLERS					8  // reasonable limit for now
#define MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS	18
#define MAX_NUM_VIRTUAL_CONTROLLER_BUTTONS	8
#define MAX_NUM_CONTROLLER_BUTTONS			MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + MAX_NUM_VIRTUAL_CONTROLLER_BUTTONS
#define MAX_DEFERRED_MESSAGE_QUEUE_SIZE		128

struct FAndroidControllerData
{
	// ID of the controller
	int32 DeviceId;

	// Current button states and the next time a repeat event should be generated for each button
	bool ButtonStates[MAX_NUM_CONTROLLER_BUTTONS];
	double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];

	// Raw analog values for various axes (sticks and triggers)
	float LXAnalog;
	float LYAnalog;
	float RXAnalog;
	float RYAnalog;
	float LTAnalog;
	float RTAnalog;
};

enum FAndroidMessageType
{
	MessageType_KeyDown,
	MessageType_KeyUp,
};

struct FDeferredAndroidMessage
{
	FDeferredAndroidMessage() {}

	FAndroidMessageType messageType;
	union
	{
		struct
		{
			int32 keyId;
			int32 unichar;
			uint32 modifier;
			bool  isRepeat;
		}
		KeyEventData;

	};
};

/**
 * Interface class for Android input devices                 
 */
class FAndroidInputInterface : public IInputInterface
{
public:

	static TSharedRef< FAndroidInputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor);


public:

	~FAndroidInputInterface();

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime );

	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();

	static void QueueTouchInput(const TArray<TouchInput>& InTouchEvents);

	static void ResetGamepadAssignments();
	static void ResetGamepadAssignmentToController(int32 ControllerId);
	static bool IsControllerAssignedToGamepad(int32 ControllerId);
	static FString GetGamepadControllerName(int32 ControllerId);

	static void JoystickAxisEvent(int32 deviceId, int32 axisId, float axisValue);
	static void JoystickButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown);

	static int32 GetAlternateKeyEventForMouse(int32 deviceID, int32 buttonID);
	static void MouseMoveEvent(int32 deviceId, float absoluteX, float absoluteY, float deltaX, float deltaY);
	static void MouseWheelEvent(int32 deviceId, float wheelDelta);
	static void MouseButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown);

	static void DeferMessage(const FDeferredAndroidMessage& DeferredMessage);

	static void QueueMotionData(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration);

	/**
	* Force Feedback implementation
	*/
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;
	virtual void SetLightColor(int32 ControllerId, FColor Color) override;
	virtual void ResetLightColor(int32 ControllerId) override;

	void SetGamepadsAllowed(bool bAllowed) { bAllowControllers = bAllowed; }
	void SetGamepadsBlockDeviceFeedback(bool bBlock) { bControllersBlockDeviceFeedback = bBlock; }

	virtual bool IsGamepadAttached() const;


	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

	const TSharedPtr< ICursor > GetCursor() const { return Cursor; }
	
private:

	FAndroidInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor);

public:

	FAndroidGamepadDeviceMapping* GetDeviceMapping(int32 ControllerId);

private:

	/** Get controller index corresponding to deviceId (assigns and queries type if new) */
	static int32 GetControllerIndex(int32 deviceId);

	/** Find controller index corresponding to validated deviceId (returns -1 if not found) */
	static int32 FindExistingDevice(int32 deviceId);

	/** Push Vibration changes to the controllers */
	void UpdateVibeMotors();

	struct MotionData
	{
		FVector Tilt;
		FVector RotationRate;
		FVector Gravity;
		FVector Acceleration;
	};

	enum MouseEventType
	{
		MouseMove,
		MouseWheel,
		MouseButtonDown,
		MouseButtonUp
	};

	struct MouseData
	{
		MouseEventType EventType;
		EMouseButtons::Type Button;
		int32 AbsoluteX;
		int32 AbsoluteY;
		int32 DeltaX;
		int32 DeltaY;
		float WheelDelta;
	};

	// protects the input stack
	static FCriticalSection TouchInputCriticalSection;

	static TArray<TouchInput> TouchInputStack;

	/** Vibration settings */
	static bool VibeIsOn;
	// Maximum time vibration will be triggered without an update
	static int32 MaxVibeTime;
	static double LastVibeUpdateTime;
	static FForceFeedbackValues VibeValues;

	// should we allow controllers to send input
	static bool bAllowControllers;

	// bluetooth connected controllers will block force feedback.
	static bool bControllersBlockDeviceFeedback;

	// should we allow controllers to send Android_Back and Android_Menu events
	static bool bBlockAndroidKeysOnControllers;

	static FAndroidGamepadDeviceMapping DeviceMapping[MAX_NUM_CONTROLLERS];

	static FAndroidControllerData OldControllerData[MAX_NUM_CONTROLLERS];
	static FAndroidControllerData NewControllerData[MAX_NUM_CONTROLLERS];

	static FGamepadKeyNames::Type ButtonMapping[MAX_NUM_CONTROLLER_BUTTONS];

	static float InitialButtonRepeatDelay;
	static float ButtonRepeatDelay;

	static FDeferredAndroidMessage DeferredMessages[MAX_DEFERRED_MESSAGE_QUEUE_SIZE];
	static int32 DeferredMessageQueueLastEntryIndex;
	static int32 DeferredMessageQueueDroppedCount;

	static TArray<MotionData> MotionDataStack;
	static TArray<MouseData> MouseDataStack;

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;

	/** List of input devices implemented in external modules. */
	TArray<TSharedPtr<class IInputDevice>> ExternalInputDevices;
};
