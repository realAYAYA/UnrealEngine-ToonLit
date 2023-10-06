// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidInputInterface.h"
#if USE_ANDROID_INPUT
#include "Android/AndroidEventManager.h"
//#include "AndroidInputDeviceMappings.h"
#include "Misc/ConfigCacheIni.h"
#include "IInputDevice.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "HAL/ThreadingBase.h"
#include "Misc/CallbackDevice.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "IHapticDevice.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

#ifndef ANDROID_GAMEPAD_TRIGGER_THRESHOLD
	#define ANDROID_GAMEPAD_TRIGGER_THRESHOLD	0.30f
#endif

TArray<TouchInput> FAndroidInputInterface::TouchInputStack = TArray<TouchInput>();
FCriticalSection FAndroidInputInterface::TouchInputCriticalSection;

FAndroidGamepadDeviceMapping FAndroidInputInterface::DeviceMapping[MAX_NUM_CONTROLLERS];

int32 FAndroidInputInterface::CurrentVibeIntensity;
int32 FAndroidInputInterface::MaxVibeTime = 1000;
double FAndroidInputInterface::LastVibeUpdateTime = 0.0;
FForceFeedbackValues FAndroidInputInterface::VibeValues;

bool FAndroidInputInterface::bAllowControllers = true;
bool FAndroidInputInterface::bBlockAndroidKeysOnControllers = false;
bool FAndroidInputInterface::bControllersBlockDeviceFeedback = false;

FAndroidControllerData FAndroidInputInterface::OldControllerData[MAX_NUM_CONTROLLERS];
FAndroidControllerData FAndroidInputInterface::NewControllerData[MAX_NUM_CONTROLLERS];
FAndroidControllerVibeState FAndroidInputInterface::ControllerVibeState[MAX_NUM_CONTROLLERS];

FGamepadKeyNames::Type FAndroidInputInterface::ButtonMapping[MAX_NUM_CONTROLLER_BUTTONS];
float FAndroidInputInterface::InitialButtonRepeatDelay;
float FAndroidInputInterface::ButtonRepeatDelay;

FDeferredAndroidMessage FAndroidInputInterface::DeferredMessages[MAX_DEFERRED_MESSAGE_QUEUE_SIZE];
int32 FAndroidInputInterface::DeferredMessageQueueLastEntryIndex = 0;
int32 FAndroidInputInterface::DeferredMessageQueueDroppedCount   = 0;

TArray<FAndroidInputInterface::MotionData> FAndroidInputInterface::MotionDataStack
	= TArray<FAndroidInputInterface::MotionData>();

TArray<FAndroidInputInterface::MouseData> FAndroidInputInterface::MouseDataStack
	= TArray<FAndroidInputInterface::MouseData>();

float GAndroidVibrationThreshold = 0.3f;
static FAutoConsoleVariableRef CVarAndroidVibrationThreshold(
	TEXT("Android.VibrationThreshold"),
	GAndroidVibrationThreshold,
	TEXT("If set above 0.0 acts as on/off threshold for device vibrator (Default: 0.3)"),
	ECVF_Default);

int32 GAndroidUseControllerFeedback = 1;
static FAutoConsoleVariableRef CVarAndroidUseControllerFeedback(
	TEXT("Android.UseControllerFeedback"),
	GAndroidUseControllerFeedback,
	TEXT("If set to non-zero, controllers with force feedback support will be active (Default: 1)"),
	ECVF_Default);

int32 GAndroidOldXBoxWirelessFirmware = 0;
static FAutoConsoleVariableRef CVarAndroidOldXBoxWirelessFirmware(
	TEXT("Android.OldXBoxWirelessFirmware"),
	GAndroidOldXBoxWirelessFirmware,
	TEXT("Determines how XBox Wireless controller mapping is handled. 0 assumes new firmware, 1 will use old firmware mapping (Default: 0)"),
	ECVF_Default);

int32 AndroidUnifyMotionSpace = 0;
static FAutoConsoleVariableRef CVarAndroidUnifyMotionSpace(
	TEXT("Android.UnifyMotionSpace"),
	AndroidUnifyMotionSpace,
	TEXT("If set to non-zero, acceleration, gravity, and rotation rate will all be in the same coordinate space. 0 for legacy behaviour. 1 will match Unreal's coordinate space (left-handed, z-up, etc). 2 will be right-handed by swapping x and y. Non-zero also forces rotation rate units to be radians/s and acceleration units to be g."),
	ECVF_Default);

TSharedRef< FAndroidInputInterface > FAndroidInputInterface::Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
{
	return MakeShareable(new FAndroidInputInterface(InMessageHandler, InCursor));
}

FAndroidInputInterface::~FAndroidInputInterface()
{
}

namespace AndroidKeyNames
{
	const FGamepadKeyNames::Type Android_Back("Android_Back");
	const FGamepadKeyNames::Type Android_Menu("Android_Menu");
}

FAndroidInputInterface::FAndroidInputInterface(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
	: MessageHandler( InMessageHandler )
	, Cursor(InCursor)
{
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBlockAndroidKeysOnControllers"), bBlockAndroidKeysOnControllers, GEngineIni);
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bControllersBlockDeviceFeedback"), bControllersBlockDeviceFeedback, GEngineIni);

	ButtonMapping[0] = FGamepadKeyNames::FaceButtonBottom;
	ButtonMapping[1] = FGamepadKeyNames::FaceButtonRight;
	ButtonMapping[2] = FGamepadKeyNames::FaceButtonLeft;
	ButtonMapping[3] = FGamepadKeyNames::FaceButtonTop;
	ButtonMapping[4] = FGamepadKeyNames::LeftShoulder;
	ButtonMapping[5] = FGamepadKeyNames::RightShoulder;
	ButtonMapping[6] = FGamepadKeyNames::SpecialRight;
	ButtonMapping[7] = FGamepadKeyNames::SpecialLeft;
	ButtonMapping[8] = FGamepadKeyNames::LeftThumb;
	ButtonMapping[9] = FGamepadKeyNames::RightThumb;
	ButtonMapping[10] = FGamepadKeyNames::LeftTriggerThreshold;
	ButtonMapping[11] = FGamepadKeyNames::RightTriggerThreshold;
	ButtonMapping[12] = FGamepadKeyNames::DPadUp;
	ButtonMapping[13] = FGamepadKeyNames::DPadDown;
	ButtonMapping[14] = FGamepadKeyNames::DPadLeft;
	ButtonMapping[15] = FGamepadKeyNames::DPadRight;
	ButtonMapping[16] = AndroidKeyNames::Android_Back;  // Technically just an alias for SpecialLeft
	ButtonMapping[17] = AndroidKeyNames::Android_Menu;  // Technically just an alias for SpecialRight

	// Virtual buttons
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 0] = FGamepadKeyNames::LeftStickLeft;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 1] = FGamepadKeyNames::LeftStickRight;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 2] = FGamepadKeyNames::LeftStickUp;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 3] = FGamepadKeyNames::LeftStickDown;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 4] = FGamepadKeyNames::RightStickLeft;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 5] = FGamepadKeyNames::RightStickRight;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 6] = FGamepadKeyNames::RightStickUp;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 7] = FGamepadKeyNames::RightStickDown;

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("InitialButtonRepeatDelay"), InitialButtonRepeatDelay, GInputIni);
	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("ButtonRepeatDelay"), ButtonRepeatDelay, GInputIni);

	CurrentVibeIntensity = 0;
	FMemory::Memset(VibeValues, 0);
	
	FMemory::Memset(DeviceMapping, 0);
	FMemory::Memset(OldControllerData, 0);
	FMemory::Memset(NewControllerData, 0);
	FMemory::Memset(ControllerVibeState, 0);
}

void FAndroidInputInterface::ResetGamepadAssignments()
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		if (DeviceMapping[DeviceIndex].DeviceState == MappingState::Valid)
		{
			FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
			FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
			
			DeviceMapper.RemapControllerIdToPlatformUserAndDevice(DeviceIndex, OUT PlatformUserId, OUT DeviceId);
			DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Disconnected);
		}

		DeviceMapping[DeviceIndex].DeviceInfo.DeviceId = 0;
		DeviceMapping[DeviceIndex].DeviceState = MappingState::Unassigned;
	}
}

void FAndroidInputInterface::ResetGamepadAssignmentToController(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return;

	if (DeviceMapping[ControllerId].DeviceState == MappingState::Valid)
	{
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();

		FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
		FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
			
		DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, OUT PlatformUserId, OUT DeviceId);
		DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Disconnected);
	}

	DeviceMapping[ControllerId].DeviceInfo.DeviceId = 0;
	DeviceMapping[ControllerId].DeviceState = MappingState::Unassigned;
}

bool FAndroidInputInterface::IsControllerAssignedToGamepad(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return false;

	return (DeviceMapping[ControllerId].DeviceState == MappingState::Valid);
}

FString FAndroidInputInterface::GetGamepadControllerName(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
	{
		return FString(TEXT("None"));
	}

	if (DeviceMapping[ControllerId].DeviceState != MappingState::Valid)
	{
		return FString(TEXT("None"));
	}

	return DeviceMapping[ControllerId].DeviceInfo.Name;
}

void FAndroidInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FAndroidInputInterface::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

extern bool AndroidThunkCpp_GetInputDeviceInfo(int32 deviceId, FAndroidInputDeviceInfo &results);

void FAndroidInputInterface::Tick(float DeltaTime)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(DeltaTime);
	}
}

void FAndroidInputInterface::SetLightColor(int32 ControllerId, FColor Color)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetLightColor(ControllerId, Color);
	}
}

void FAndroidInputInterface::ResetLightColor(int32 ControllerId)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->ResetLightColor(ControllerId);
	}
}

void FAndroidInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	bool bDidFeedback = false;
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->SupportsForceFeedback(ControllerId))
		{
			bDidFeedback = true;
			(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
		}
	}

	// If didn't already assign feedback and active controller has feedback support use it, if enabled
	if (!bDidFeedback && IsControllerAssignedToGamepad(ControllerId) && GAndroidUseControllerFeedback != 0 && DeviceMapping[ControllerId].DeviceInfo.FeedbackMotorCount > 0)
	{
		switch (ChannelType)
		{
			case FForceFeedbackChannelType::LEFT_LARGE:
				ControllerVibeState[ControllerId].VibeValues.LeftLarge = Value;
				break;

			case FForceFeedbackChannelType::LEFT_SMALL:
				ControllerVibeState[ControllerId].VibeValues.LeftSmall = Value;
				break;

			case FForceFeedbackChannelType::RIGHT_LARGE:
				ControllerVibeState[ControllerId].VibeValues.RightLarge = Value;
				break;

			case FForceFeedbackChannelType::RIGHT_SMALL:
				ControllerVibeState[ControllerId].VibeValues.RightSmall = Value;
				break;

			default:
				// Unknown channel, so ignore it
			break;
		}
		bDidFeedback = true;
	}

	bDidFeedback |= IsGamepadAttached() && bControllersBlockDeviceFeedback;

	// If controller handled force feedback don't do it on the phone
	if (bDidFeedback)
	{
		VibeValues.LeftLarge = VibeValues.RightLarge = VibeValues.LeftSmall = VibeValues.RightSmall = 0.0f;
		return;
	}

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	// Save a copy of the value for future comparison
	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
			VibeValues.LeftLarge = Value;
			break;

		case FForceFeedbackChannelType::LEFT_SMALL:
			VibeValues.LeftSmall = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_LARGE:
			VibeValues.RightLarge = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_SMALL:
			VibeValues.RightSmall = Value;
			break;

		default:
			// Unknown channel, so ignore it
			break;
	}
}

void FAndroidInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	bool bDidFeedback = false;
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->SupportsForceFeedback(ControllerId))
		{
			bDidFeedback = true;
			(*DeviceIt)->SetChannelValues(ControllerId, Values);
		}
	}

	// If didn't already assign feedback and active controller has feedback support use it, if enabled
	if (!bDidFeedback && IsControllerAssignedToGamepad(ControllerId) && GAndroidUseControllerFeedback != 0 && DeviceMapping[ControllerId].DeviceInfo.FeedbackMotorCount > 0)
	{
		ControllerVibeState[ControllerId].VibeValues = Values;
		bDidFeedback = true;
	}

	bDidFeedback |= IsGamepadAttached() && bControllersBlockDeviceFeedback;

	// If controller handled force feedback don't do it on the phone
	if (bDidFeedback)
	{
		VibeValues.LeftLarge = VibeValues.RightLarge = VibeValues.LeftSmall = VibeValues.RightSmall = 0.0f;
	}
	else
	{
		VibeValues = Values;
	}
}

void FAndroidInputInterface::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		IHapticDevice* HapticDevice = (*DeviceIt)->GetHapticDevice();
		if (HapticDevice)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

extern bool AndroidThunkCpp_IsGamepadAttached();

bool FAndroidInputInterface::IsGamepadAttached() const
{
	// Check for gamepads that have already been validated
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		FAndroidGamepadDeviceMapping& CurrentDevice = DeviceMapping[DeviceIndex];

		if (CurrentDevice.DeviceState == MappingState::Valid)
		{
			return true;
		}
	}

	for (auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}

	//if all of this fails, do a check on the Java side to see if the gamepad is attached
	return AndroidThunkCpp_IsGamepadAttached();
}

static FORCEINLINE int32 ConvertToByte(float Value)
{
	int32 Setting = (int32)(Value * 255.0f);
	return Setting < 0 ? 0 : (Setting < 255 ? Setting : 255);
}

extern void AndroidThunkCpp_Vibrate(int32 Intensity, int32 Duration);
extern bool AndroidThunkCpp_SetInputDeviceVibrators(int32 deviceId, int32 leftIntensity, int32 leftDuration, int32 rightIntensity, int32 rightDuration);

void FAndroidInputInterface::UpdateVibeMotors()
{
	// Turn off vibe if not in focus
	bool bActive = CurrentVibeIntensity > 0;
	if (!FAppEventManager::GetInstance()->IsGameInFocus())
	{
		if (bActive)
		{
			AndroidThunkCpp_Vibrate(0, MaxVibeTime);
			CurrentVibeIntensity = 0;
		}
		return;
	}

	// Use largest vibration state as value
	const float MaxLeft = VibeValues.LeftLarge > VibeValues.LeftSmall ? VibeValues.LeftLarge : VibeValues.LeftSmall;
	const float MaxRight = VibeValues.RightLarge > VibeValues.RightSmall ? VibeValues.RightLarge : VibeValues.RightSmall;
	float Value = MaxLeft > MaxRight ? MaxLeft : MaxRight;

	// apply optional threshold for old behavior
	if (GAndroidVibrationThreshold > 0.0f)
	{
		Value = Value < GAndroidVibrationThreshold ? 0.0f : 1.0f;
	}

	int32 Intensity = ConvertToByte(Value);

	// if previously active and overtime, current state is off
	double CurrentTime = FPlatformTime::Seconds();
	bool bOvertime = 1000 * (CurrentTime - LastVibeUpdateTime) >= MaxVibeTime;
	if (bActive && bOvertime)
	{
		CurrentVibeIntensity = 0;
	}

	// update if not already active at same level
	if (CurrentVibeIntensity != Intensity)
	{
		AndroidThunkCpp_Vibrate(Intensity, MaxVibeTime);
		CurrentVibeIntensity = Intensity;
		LastVibeUpdateTime = CurrentTime;
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VibDevice %f: %d"), (float)LastVibeUpdateTime, Intensity);
	}
}

void FAndroidInputInterface::UpdateControllerVibeMotors(int32 ControllerId)
{
	FAndroidControllerVibeState& State = ControllerVibeState[ControllerId];

	// Turn off vibe if not in focus
	bool bActive = State.LeftIntensity > 0 || State.RightIntensity > 0;
	if (!FAppEventManager::GetInstance()->IsGameInFocus())
	{
		if (bActive)
		{
			AndroidThunkCpp_SetInputDeviceVibrators(DeviceMapping[ControllerId].DeviceInfo.DeviceId, 0, MaxVibeTime, 0, MaxVibeTime);
			State.LeftIntensity = 0;
			State.RightIntensity = 0;
		}
		return;
	}

	float MaxLeft;
	float MaxRight;

	// Use largest vibration state as value for controller type
	switch (DeviceMapping[ControllerId].ControllerClass)
	{
		case ControllerClassType::PlaystationWireless:
//			DS4 maybe should use this?  PS5 seems correct with generic
//			MaxLeft = (State.VibeValues.LeftLarge > State.VibeValues.RightLarge ? State.VibeValues.LeftLarge : State.VibeValues.RightLarge);
//			MaxRight = (State.VibeValues.LeftSmall > State.VibeValues.RightSmall ? State.VibeValues.LeftSmall : State.VibeValues.RightSmall);
//			break;

		case ControllerClassType::Generic:
		case ControllerClassType::XBoxWired:
		case ControllerClassType::XBoxWireless:
		default:
			MaxLeft = (State.VibeValues.LeftLarge > State.VibeValues.LeftSmall ? State.VibeValues.LeftLarge : State.VibeValues.LeftSmall);
			MaxRight = (State.VibeValues.RightLarge > State.VibeValues.RightSmall ? State.VibeValues.RightLarge : State.VibeValues.RightSmall);
			break;
	}

	int32 LeftIntensity = ConvertToByte(MaxLeft);
	int32 RightIntensity = ConvertToByte(MaxRight);

	// if previously active and overtime, current state is off
	double CurrentTime = FPlatformTime::Seconds();
	bool bOvertime = 1000 * (CurrentTime - ControllerVibeState[ControllerId].LastVibeUpdateTime) >= MaxVibeTime;
	if (bActive && bOvertime)
	{
		State.LeftIntensity = 0;
		State.RightIntensity = 0;
	}

	// update if not already active at same level
	if (State.LeftIntensity != LeftIntensity || State.RightIntensity != RightIntensity)
	{
		AndroidThunkCpp_SetInputDeviceVibrators(DeviceMapping[ControllerId].DeviceInfo.DeviceId, LeftIntensity, MaxVibeTime, RightIntensity, MaxVibeTime);
		State.LeftIntensity = LeftIntensity;
		State.RightIntensity = RightIntensity;
		State.LastVibeUpdateTime = CurrentTime;
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VibController %f: %d, %d"), (float)State.LastVibeUpdateTime, LeftIntensity, RightIntensity);
	}
}

static TCHAR CharMap[] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'0',
	L'1',
	L'2',
	L'3',
	L'4',
	L'5',
	L'6',
	L'7',
	L'8',
	L'9',
	L'*',
	L'#',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'a',
	L'b',
	L'c',
	L'd',
	L'e',
	L'f',
	L'g',
	L'h',
	L'i',
	L'j',
	L'k',
	L'l',
	L'm',
	L'n',
	L'o',
	L'p',
	L'q',
	L'r',
	L's',
	L't',
	L'u',
	L'v',
	L'w',
	L'x',
	L'y',
	L'z',
	L',',
	L'.',
	0,
	0,
	0,
	0,
	L'\t',
	L' ',
	0,
	0,
	0,
	L'\n',
	L'\b',
	L'`',
	L'-',
	L'=',
	L'[',
	L']',
	L'\\',
	L';',
	L'\'',
	L'/',
	L'@',
	0,
	0,
	0,   // *Camera* focus
	L'+',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'0',
	L'1',
	L'2',
	L'3',
	L'4',
	L'5',
	L'6',
	L'7',
	L'8',
	L'9',
	L'/',
	L'*',
	L'-',
	L'+',
	L'.',
	L',',
	L'\n',
	L'=',
	L'(',
	L')',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

static TCHAR CharMapShift[] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L')',
	L'!',
	L'@',
	L'#',
	L'$',
	L'%',
	L'^',
	L'&',
	L'*',
	L'(',
	L'*',
	L'#',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'A',
	L'B',
	L'C',
	L'D',
	L'E',
	L'F',
	L'G',
	L'H',
	L'I',
	L'J',
	L'K',
	L'L',
	L'M',
	L'N',
	L'O',
	L'P',
	L'Q',
	L'R',
	L'S',
	L'T',
	L'U',
	L'V',
	L'W',
	L'X',
	L'Y',
	L'Z',
	L'<',
	L'>',
	0,
	0,
	0,
	0,
	L'\t',
	L' ',
	0,
	0,
	0,
	L'\n',
	L'\b',
	L'~',
	L'_',
	L'+',
	L'{',
	L'}',
	L'|',
	L':',
	L'\"',
	L'?',
	L'@',
	0,
	0,
	0,   // *Camera* focus
	L'+',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'0',
	L'1',
	L'2',
	L'3',
	L'4',
	L'5',
	L'6',
	L'7',
	L'8',
	L'9',
	L'/',
	L'*',
	L'-',
	L'+',
	L'.',
	L',',
	L'\n',
	L'=',
	L'(',
	L')',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

extern void AndroidThunkCpp_PushSensorEvents();

void FAndroidInputInterface::SendControllerEvents()
{
	// trigger any motion updates before the lock so they can be queued
	AndroidThunkCpp_PushSensorEvents();

	FScopeLock Lock(&TouchInputCriticalSection);

	// Update device vibe motor with latest values (only one motor so look at combination of all values to pick state)
	UpdateVibeMotors();

	// Check for gamepads needing validation if enabled
	if (bAllowControllers)
	{
		for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
		{
			FAndroidGamepadDeviceMapping& CurrentDevice = DeviceMapping[DeviceIndex];

			if (CurrentDevice.DeviceState == MappingState::ToValidate)
			{
				// Query for the device type from Java side
				if (AndroidThunkCpp_GetInputDeviceInfo(CurrentDevice.DeviceInfo.DeviceId, CurrentDevice.DeviceInfo))
				{
					// It is possible this is actually a previously assigned controller if it disconnected and reconnected (device ID can change)
					int32 FoundMatch = -1;
					for (int32 DeviceScan = 0; DeviceScan < MAX_NUM_CONTROLLERS; DeviceScan++)
					{
						if (DeviceMapping[DeviceScan].DeviceState != MappingState::Valid)
							continue;

						if (DeviceMapping[DeviceScan].DeviceInfo.Descriptor.Equals(CurrentDevice.DeviceInfo.Descriptor))
						{
							FoundMatch = DeviceScan;
							break;
						}
					}

					// Deal with new controller
					if (FoundMatch == -1)
					{
						CurrentDevice.DeviceState = MappingState::Valid;

						// Generic mappings
						CurrentDevice.ControllerClass = ControllerClassType::Generic;
						CurrentDevice.ButtonRemapping = ButtonRemapType::Normal;
						CurrentDevice.LTAnalogRangeMinimum = 0.0f;
						CurrentDevice.RTAnalogRangeMinimum = 0.0f;
						CurrentDevice.bTriggersUseThresholdForClick = false;
						CurrentDevice.bSupportsHat = true;
						CurrentDevice.bMapL1R1ToTriggers = false;
						CurrentDevice.bMapZRZToTriggers = false;
						CurrentDevice.bRightStickZRZ = true;
						CurrentDevice.bRightStickRXRY = false;
						CurrentDevice.bMapRXRYToTriggers = false;

						// Use device name to decide on mapping scheme
						if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon")))
						{
							if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon Fire Game Controller")))
							{
								CurrentDevice.bSupportsHat = true;
							}
							else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon Fire TV Remote")))
							{
								CurrentDevice.bSupportsHat = false;
							}
							else
							{
								CurrentDevice.bSupportsHat = false;
							}
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("NVIDIA Corporation NVIDIA Controller")))
						{
							CurrentDevice.bSupportsHat = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Samsung Game Pad EI-GP20")))
						{
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bMapL1R1ToTriggers = true;
							CurrentDevice.bRightStickZRZ = false;
							CurrentDevice.bRightStickRXRY = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Mad Catz C.T.R.L.R")))
						{
							CurrentDevice.bSupportsHat = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Generic X-Box pad")))
						{
							CurrentDevice.ControllerClass = ControllerClassType::XBoxWired;
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bTriggersUseThresholdForClick = true;

							// different mapping before Android 12
							if (FAndroidMisc::GetAndroidBuildVersion() < 31)
							{
								CurrentDevice.bRightStickZRZ = false;
								CurrentDevice.bRightStickRXRY = true;
								CurrentDevice.bMapZRZToTriggers = true;
								CurrentDevice.LTAnalogRangeMinimum = -1.0f;
								CurrentDevice.RTAnalogRangeMinimum = -1.0f;
							}
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Xbox Wired Controller")))
						{
							CurrentDevice.ControllerClass = ControllerClassType::XBoxWired;
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bTriggersUseThresholdForClick = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Xbox Wireless Controller"))
									|| CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Xbox Elite Wireless Controller")))
						{
							CurrentDevice.ControllerClass = ControllerClassType::XBoxWireless;
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bTriggersUseThresholdForClick = true;

							if (GAndroidOldXBoxWirelessFirmware == 1)
							{
								// Apply mappings for older firmware before 3.1.1221.0
								CurrentDevice.ButtonRemapping = ButtonRemapType::XBox;
								CurrentDevice.bMapL1R1ToTriggers = false;
								CurrentDevice.bMapZRZToTriggers = true;
								CurrentDevice.bRightStickZRZ = false;
								CurrentDevice.bRightStickRXRY = true;
							}
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("SteelSeries Stratus XL")))
						{
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bTriggersUseThresholdForClick = true;

							// For some reason the left trigger is at 0.5 when at rest so we have to adjust for that.
							CurrentDevice.LTAnalogRangeMinimum = 0.5f;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("PS4 Wireless Controller")))
						{
							CurrentDevice.ControllerClass = ControllerClassType::PlaystationWireless;
							if (CurrentDevice.DeviceInfo.Name.EndsWith(TEXT(" (v2)")) && FAndroidMisc::GetCPUVendor() != TEXT("Sony")
								&& FAndroidMisc::GetAndroidBuildVersion() < 10)
							{
								// Only needed for non-Sony devices with v2 firmware
								CurrentDevice.ButtonRemapping = ButtonRemapType::PS4;
							}
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bRightStickZRZ = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("PS5 Wireless Controller")))
						{
							//FAndroidMisc::GetAndroidBuildVersion() actually returns the API Level instead of the Android Version
							bool bUseNewPS5Mapping = FAndroidMisc::GetAndroidBuildVersion() > 30;
							CurrentDevice.ButtonRemapping = bUseNewPS5Mapping ? ButtonRemapType::PS5New : ButtonRemapType::PS5;
							CurrentDevice.ControllerClass = ControllerClassType::PlaystationWireless;
							CurrentDevice.bSupportsHat = true;
							CurrentDevice.bRightStickZRZ = true;
							CurrentDevice.bMapRXRYToTriggers = !bUseNewPS5Mapping;
							CurrentDevice.LTAnalogRangeMinimum = bUseNewPS5Mapping ? 0.0f : -1.0f;
							CurrentDevice.RTAnalogRangeMinimum = bUseNewPS5Mapping ? 0.0f : -1.0f;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("glap QXPGP001")))
						{
							CurrentDevice.bSupportsHat = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("STMicroelectronics Lenovo GamePad")))
						{
							CurrentDevice.bSupportsHat = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Razer")))
						{
							CurrentDevice.bSupportsHat = true;
							if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Razer Kishi V2 Pro XBox360")))
							{
								CurrentDevice.ControllerClass = ControllerClassType::XBoxWired;
								CurrentDevice.bSupportsHat = true;
								CurrentDevice.bTriggersUseThresholdForClick = true;

								// different mapping before Android 12
								if (FAndroidMisc::GetAndroidBuildVersion() < 31)
								{
									CurrentDevice.bRightStickZRZ = false;
									CurrentDevice.bRightStickRXRY = true;
									CurrentDevice.bMapZRZToTriggers = true;
									CurrentDevice.LTAnalogRangeMinimum = -1.0f;
									CurrentDevice.RTAnalogRangeMinimum = -1.0f;
								}
							}
							else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Razer Kishi V2")))
							{
								CurrentDevice.ControllerClass = ControllerClassType::XBoxWired;
								CurrentDevice.bTriggersUseThresholdForClick = true;
							}
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Luna")))
						{
							CurrentDevice.bTriggersUseThresholdForClick = true;
						}

						IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
						FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
						FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
			
						DeviceMapper.RemapControllerIdToPlatformUserAndDevice(DeviceIndex, OUT PlatformUserId, OUT DeviceId);
						DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Connected);

						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Assigned new gamepad controller %d: DeviceId=%d, ControllerId=%d, DeviceName=%s, Descriptor=%s"),
							DeviceIndex, CurrentDevice.DeviceInfo.DeviceId, CurrentDevice.DeviceInfo.ControllerId, *CurrentDevice.DeviceInfo.Name, *CurrentDevice.DeviceInfo.Descriptor);
						continue;
					}
					else
					{
						// Already assigned this controller so reconnect it
						DeviceMapping[FoundMatch].DeviceInfo.DeviceId = CurrentDevice.DeviceInfo.DeviceId;
						CurrentDevice.DeviceInfo.DeviceId = 0;
						CurrentDevice.DeviceState = MappingState::Unassigned;

						// Transfer state back to this controller
						NewControllerData[FoundMatch] = NewControllerData[DeviceIndex];
						NewControllerData[FoundMatch].DeviceId = FoundMatch;
						OldControllerData[FoundMatch].DeviceId = FoundMatch;

						//@TODO: uncomment these line in the future when disconnects are detected
						// IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
						//
						// FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
						// FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
						//
						// DeviceMapper.RemapControllerIdToPlatformUserAndDevice(FoundMatch, OUT PlatformUserId, OUT DeviceId);
						// DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Connected);

						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Reconnected gamepad controller %d: DeviceId=%d, ControllerId=%d, DeviceName=%s, Descriptor=%s"),
							FoundMatch, DeviceMapping[FoundMatch].DeviceInfo.DeviceId, CurrentDevice.DeviceInfo.ControllerId, *CurrentDevice.DeviceInfo.Name, *CurrentDevice.DeviceInfo.Descriptor);
					}
				}
				else
				{
					// Did not find match so clear the assignment
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to assign gamepad controller %d: DeviceId=%d"), DeviceIndex, CurrentDevice.DeviceInfo.DeviceId);
				}

			}
		}
	}

	for(int i = 0; i < FAndroidInputInterface::TouchInputStack.Num(); ++i)
	{
		TouchInput Touch = FAndroidInputInterface::TouchInputStack[i];
		int32 ControllerId = FindExistingDevice(Touch.DeviceId);
		ControllerId = (ControllerId == -1) ? 0 : ControllerId;

		// send input to handler
		switch ( Touch.Type )
		{
		case TouchBegan:
			MessageHandler->OnTouchStarted(nullptr, Touch.Position, 1.0f, Touch.Handle, ControllerId);
			break;
		case TouchEnded:
			MessageHandler->OnTouchEnded(Touch.Position, Touch.Handle, ControllerId);
			break;
		case TouchMoved:
			MessageHandler->OnTouchMoved(Touch.Position, 1.0f, Touch.Handle, ControllerId);
			break;
		}
	}

	// Extract differences in new and old states and send messages
	if (bAllowControllers)
	{
		for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
		{
			// Skip unassigned or invalid controllers (treat first one as special case)
			if (ControllerIndex > 0 && (DeviceMapping[ControllerIndex].DeviceState !=  MappingState::Valid))
			{
				continue;
			}

			FAndroidControllerData& OldControllerState = OldControllerData[ControllerIndex];
			FAndroidControllerData& NewControllerState = NewControllerData[ControllerIndex];
			
			IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();		            			
			FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
			FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
			DeviceMapper.RemapControllerIdToPlatformUserAndDevice(NewControllerState.DeviceId, OUT UserId, OUT DeviceId);

			// Send controller events any time we have a large enough input threshold similarly to PC/Console (see: XInputInterface.cpp)
			const float RepeatDeadzone = 0.24f;

			if (NewControllerState.LXAnalog != OldControllerState.LXAnalog || FMath::Abs(NewControllerState.LXAnalog) >= RepeatDeadzone)
			{
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, UserId, DeviceId, NewControllerState.LXAnalog);
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 1] = NewControllerState.LXAnalog >= RepeatDeadzone;
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 0] = NewControllerState.LXAnalog <= -RepeatDeadzone;
			}
			if (NewControllerState.LYAnalog != OldControllerState.LYAnalog || FMath::Abs(NewControllerState.LYAnalog) >= RepeatDeadzone)
			{
				//LOGD("    Sending updated LeftAnalogY value of %f", NewControllerState.LYAnalog);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, UserId, DeviceId, NewControllerState.LYAnalog);
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 2] = NewControllerState.LYAnalog >= RepeatDeadzone;
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 3] = NewControllerState.LYAnalog <= -RepeatDeadzone;
			}
			if (NewControllerState.RXAnalog != OldControllerState.RXAnalog || FMath::Abs(NewControllerState.RXAnalog) >= RepeatDeadzone)
			{
				//LOGD("    Sending updated RightAnalogX value of %f", NewControllerState.RXAnalog);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, UserId, DeviceId, NewControllerState.RXAnalog);
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 5] = NewControllerState.RXAnalog >= RepeatDeadzone;
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 4] = NewControllerState.RXAnalog <= -RepeatDeadzone;
			}
			if (NewControllerState.RYAnalog != OldControllerState.RYAnalog || FMath::Abs(NewControllerState.RYAnalog) >= RepeatDeadzone)
			{
				//LOGD("    Sending updated RightAnalogY value of %f", NewControllerState.RYAnalog);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, UserId, DeviceId, NewControllerState.RYAnalog);
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 6] = NewControllerState.RYAnalog >= RepeatDeadzone;
				NewControllerState.ButtonStates[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 7] = NewControllerState.RYAnalog <= -RepeatDeadzone;
			}
			
			const bool bUseTriggerThresholdForClick = DeviceMapping[ControllerIndex].bTriggersUseThresholdForClick;
			if (NewControllerState.LTAnalog != OldControllerState.LTAnalog)
			{
				//LOGD("    Sending updated LeftTriggerAnalog value of %f", NewControllerState.LTAnalog);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, UserId, DeviceId, NewControllerState.LTAnalog);

				if (bUseTriggerThresholdForClick)
				{
					// Handle the trigger theshold "virtual" button state
					//check(ButtonMapping[10] == FGamepadKeyNames::LeftTriggerThreshold);
					NewControllerState.ButtonStates[10] = NewControllerState.LTAnalog >= ANDROID_GAMEPAD_TRIGGER_THRESHOLD;
				}
			}
			if (NewControllerState.RTAnalog != OldControllerState.RTAnalog)
			{
				//LOGD("    Sending updated RightTriggerAnalog value of %f", NewControllerState.RTAnalog);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, UserId, DeviceId, NewControllerState.RTAnalog);

				if (bUseTriggerThresholdForClick)
				{
					// Handle the trigger theshold "virtual" button state
					//check(ButtonMapping[11] == FGamepadKeyNames::RightTriggerThreshold);
					NewControllerState.ButtonStates[11] = NewControllerState.RTAnalog >= ANDROID_GAMEPAD_TRIGGER_THRESHOLD;
				}
			}

			const double CurrentTime = FPlatformTime::Seconds();

			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ButtonIndex++)
			{
				if (NewControllerState.ButtonStates[ButtonIndex] != OldControllerState.ButtonStates[ButtonIndex])
				{
					if (NewControllerState.ButtonStates[ButtonIndex])
					{
						//LOGD("    Sending joystick button down %d (first)", ButtonMapping[ButtonIndex]);
						MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], UserId, DeviceId, false);
					}
					else
					{
						//LOGD("    Sending joystick button up %d", ButtonMapping[ButtonIndex]);
						MessageHandler->OnControllerButtonReleased(ButtonMapping[ButtonIndex], UserId, DeviceId, false);
					}

					if (NewControllerState.ButtonStates[ButtonIndex])
					{
						// This button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
						NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
				}
				else if (NewControllerState.ButtonStates[ButtonIndex] && NewControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
				{
					// Send button repeat events
					MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], UserId, DeviceId, true);

					// Set the button's NextRepeatTime to the ButtonRepeatDelay
					NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}
			}

			// send controller force feedback updates if enabled
			if (GAndroidUseControllerFeedback != 0)
			{
				if (DeviceMapping[ControllerIndex].DeviceInfo.FeedbackMotorCount > 0)
				{
					UpdateControllerVibeMotors(ControllerIndex);
				}
			}

			// Update the state for next time
			OldControllerState = NewControllerState;
		}
	}

	for (int i = 0; i < FAndroidInputInterface::MotionDataStack.Num(); ++i)
	{
		MotionData motion_data = FAndroidInputInterface::MotionDataStack[i];

		MessageHandler->OnMotionDetected(
			motion_data.Tilt, motion_data.RotationRate,
			motion_data.Gravity, motion_data.Acceleration,
			0);
	}

	for (int i = 0; i < FAndroidInputInterface::MouseDataStack.Num(); ++i)
	{
		MouseData mouse_data = FAndroidInputInterface::MouseDataStack[i];

		switch (mouse_data.EventType)
		{
			case MouseEventType::MouseMove:
				if (Cursor.IsValid())
				{
					Cursor->SetPosition(mouse_data.AbsoluteX, mouse_data.AbsoluteY);
					MessageHandler->OnMouseMove();
				}
				MessageHandler->OnRawMouseMove(mouse_data.DeltaX, mouse_data.DeltaY);
				break;

			case MouseEventType::MouseWheel:
				MessageHandler->OnMouseWheel(mouse_data.WheelDelta);
				break;

			case MouseEventType::MouseButtonDown:
				MessageHandler->OnMouseDown(nullptr, mouse_data.Button);
				break;

			case MouseEventType::MouseButtonUp:
				MessageHandler->OnMouseUp(mouse_data.Button);
				break;
		}
	}

	for (int32 MessageIndex = 0; MessageIndex < FMath::Min(DeferredMessageQueueLastEntryIndex, MAX_DEFERRED_MESSAGE_QUEUE_SIZE); ++MessageIndex)
	{
		const FDeferredAndroidMessage& DeferredMessage = DeferredMessages[MessageIndex];
		const TCHAR Char = DeferredMessage.KeyEventData.modifier & AMETA_SHIFT_ON ? CharMapShift[DeferredMessage.KeyEventData.keyId] : CharMap[DeferredMessage.KeyEventData.keyId];
		
		switch (DeferredMessage.messageType)
		{

			case MessageType_KeyDown:

				MessageHandler->OnKeyDown(DeferredMessage.KeyEventData.keyId, Char, DeferredMessage.KeyEventData.isRepeat);
				MessageHandler->OnKeyChar(Char,  DeferredMessage.KeyEventData.isRepeat);
				break;

			case MessageType_KeyUp:

				MessageHandler->OnKeyUp(DeferredMessage.KeyEventData.keyId, Char, false);
				break;
		} 
	}

	if (DeferredMessageQueueDroppedCount)
	{
		//should warn that messages got dropped, which message queue?
		DeferredMessageQueueDroppedCount = 0;
	}

	DeferredMessageQueueLastEntryIndex = 0;

	FAndroidInputInterface::TouchInputStack.Empty(0);

	FAndroidInputInterface::MotionDataStack.Empty();

	FAndroidInputInterface::MouseDataStack.Empty();

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SendControllerEvents();
	}
}

void FAndroidInputInterface::QueueTouchInput(const TArray<TouchInput>& InTouchEvents)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::TouchInputStack.Append(InTouchEvents);
}

int32 FAndroidInputInterface::FindExistingDevice(int32 deviceId)
{
	if (!bAllowControllers)
	{
		return -1;
	}
	
	// Treat non-positive devices ids special
	if (deviceId < 1)
		return -1;

	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId && DeviceMapping[ControllerIndex].DeviceState == MappingState::Valid)
		{
			return ControllerIndex;
		}
	}

	// Did not find it
	return -1;
}

FAndroidGamepadDeviceMapping* FAndroidInputInterface::GetDeviceMapping(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
	{
		return nullptr;
	}
	return &DeviceMapping[ControllerId];
}

int32 FAndroidInputInterface::GetControllerIndex(int32 deviceId)
{
	if (!bAllowControllers)
	{
		return -1;
	}
	
	// Treat non-positive device ids special (always controller 0)
	if (deviceId < 1)
		return 0;

	// Look for this deviceId in controllers discovered
	int32 UnassignedIndex = -1;
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceState == MappingState::Unassigned)
		{
			if (UnassignedIndex == -1)
				UnassignedIndex = ControllerIndex;
			continue;
		}

		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId)
		{
			return ControllerIndex;
		}
	}

	// Haven't seen this one before, make sure there is room for a new one
	if (UnassignedIndex == -1)
		return -1;

	// Register it
	DeviceMapping[UnassignedIndex].DeviceInfo.DeviceId = deviceId;
	OldControllerData[UnassignedIndex].DeviceId = UnassignedIndex;
	NewControllerData[UnassignedIndex].DeviceId = UnassignedIndex;

	// Mark it for validation later
	DeviceMapping[UnassignedIndex].DeviceState = MappingState::ToValidate;

	return UnassignedIndex;
}

void FAndroidInputInterface::JoystickAxisEvent(int32 deviceId, int32 axisId, float axisValue)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// Get the controller index matching deviceId (if there is one)
	deviceId = GetControllerIndex(deviceId);
	if (deviceId == -1)
		return;

	auto RemapTriggerFunction = [](const float Minimum, const float Value)
	{
		if(Minimum != 0.0f)
		{
			const float AdjustMin = Minimum;
			const float AdjustMax = 1.0f - AdjustMin;
			return FMath::Clamp(Value - AdjustMin, 0.0f, AdjustMax) / AdjustMax;
		}
		return Value;
	};

	// Deal with left stick and triggers (generic)
	switch (axisId)
	{
		case AMOTION_EVENT_AXIS_X:			NewControllerData[deviceId].LXAnalog =  axisValue; return;
		case AMOTION_EVENT_AXIS_Y:			NewControllerData[deviceId].LYAnalog = -axisValue; return;
		case AMOTION_EVENT_AXIS_LTRIGGER:
			{
				if (!(DeviceMapping->bMapZRZToTriggers || DeviceMapping->bMapRXRYToTriggers))
				{
					NewControllerData[deviceId].LTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].LTAnalogRangeMinimum, axisValue);
					return;
				}
			}
		case AMOTION_EVENT_AXIS_RTRIGGER:
			{
				if (!(DeviceMapping->bMapZRZToTriggers || DeviceMapping->bMapRXRYToTriggers))
				{
					NewControllerData[deviceId].RTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].RTAnalogRangeMinimum, axisValue);
					return;
				}
			}
	}

	// Deal with right stick Z/RZ events
	if (DeviceMapping[deviceId].bRightStickZRZ)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_Z:		NewControllerData[deviceId].RXAnalog =  axisValue; return;
			case AMOTION_EVENT_AXIS_RZ:		NewControllerData[deviceId].RYAnalog = -axisValue; return;
		}
	}

	// Deal with right stick RX/RY events
	if (DeviceMapping[deviceId].bRightStickRXRY)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_RX:		NewControllerData[deviceId].RXAnalog =  axisValue; return;
			case AMOTION_EVENT_AXIS_RY:		NewControllerData[deviceId].RYAnalog = -axisValue; return;
		}
	}

	// Deal with Z/RZ mapping to triggers
	if (DeviceMapping[deviceId].bMapZRZToTriggers)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_Z:		NewControllerData[deviceId].LTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].LTAnalogRangeMinimum, axisValue); return;
			case AMOTION_EVENT_AXIS_RZ:		NewControllerData[deviceId].RTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].RTAnalogRangeMinimum, axisValue); return;
		}
	}

	if (DeviceMapping[deviceId].bMapRXRYToTriggers)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_RX:		NewControllerData[deviceId].LTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].LTAnalogRangeMinimum, axisValue); return;
			case AMOTION_EVENT_AXIS_RY:		NewControllerData[deviceId].RTAnalog = RemapTriggerFunction(DeviceMapping[deviceId].RTAnalogRangeMinimum, axisValue); return;
		}
	}

	// Deal with hat (convert to DPAD buttons)
	if (DeviceMapping[deviceId].bSupportsHat)
	{
		// Apply a small dead zone to hats
		const float deadZone = 0.2f;

		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_HAT_X:
				// AMOTION_EVENT_AXIS_HAT_X translates to KEYCODE_DPAD_LEFT and AKEYCODE_DPAD_RIGHT
				if (axisValue > deadZone)
				{
					NewControllerData[deviceId].ButtonStates[14] = false;	// DPAD_LEFT released
					NewControllerData[deviceId].ButtonStates[15] = true;	// DPAD_RIGHT pressed
				}
				else if (axisValue < -deadZone)
				{
					NewControllerData[deviceId].ButtonStates[14] = true;	// DPAD_LEFT pressed
					NewControllerData[deviceId].ButtonStates[15] = false;	// DPAD_RIGHT released
				}
				else
				{
					NewControllerData[deviceId].ButtonStates[14] = false;	// DPAD_LEFT released
					NewControllerData[deviceId].ButtonStates[15] = false;	// DPAD_RIGHT released
				}
				return;
			case AMOTION_EVENT_AXIS_HAT_Y:
				// AMOTION_EVENT_AXIS_HAT_Y translates to KEYCODE_DPAD_UP and AKEYCODE_DPAD_DOWN
				if (axisValue > deadZone)
				{
					NewControllerData[deviceId].ButtonStates[12] = false;	// DPAD_UP released
					NewControllerData[deviceId].ButtonStates[13] = true;	// DPAD_DOWN pressed
				}
				else if (axisValue < -deadZone)
				{
					NewControllerData[deviceId].ButtonStates[12] = true;	// DPAD_UP pressed
					NewControllerData[deviceId].ButtonStates[13] = false;	// DPAD_DOWN released
				}
				else
				{
					NewControllerData[deviceId].ButtonStates[12] = false;	// DPAD_UP released
					NewControllerData[deviceId].ButtonStates[13] = false;	// DPAD_DOWN released
				}
				return;
		}
	}
}

void FAndroidInputInterface::JoystickButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// Get the controller index matching deviceId (if there is one)
	deviceId = GetControllerIndex(deviceId);
	if (deviceId == -1)
		return;

	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("JoystickButtonEvent[%d]: %d"), (int)DeviceMapping[deviceId].ButtonRemapping, buttonId);

	if (DeviceMapping[deviceId].ControllerClass == ControllerClassType::PlaystationWireless)
	{
		if (buttonId == 3002)
		{
			NewControllerData[deviceId].ButtonStates[7] = buttonDown;  // Touchpad = Special Left
			return;
		}
	}

	// Deal with button remapping
	switch (DeviceMapping[deviceId].ButtonRemapping)
	{
		case ButtonRemapType::Normal:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_A:
				case AKEYCODE_DPAD_CENTER:   NewControllerData[deviceId].ButtonStates[0] = buttonDown; break;
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break;
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break;
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break;
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[4] = buttonDown;
											 if (DeviceMapping[deviceId].bMapL1R1ToTriggers)
											 {
												 NewControllerData[deviceId].ButtonStates[10] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[5] = buttonDown;
											 if (DeviceMapping[deviceId].bMapL1R1ToTriggers)
											 {
												 NewControllerData[deviceId].ButtonStates[11] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_START:
				case AKEYCODE_MENU:          NewControllerData[deviceId].ButtonStates[6] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
												 NewControllerData[deviceId].ButtonStates[17] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_SELECT:
				case AKEYCODE_BACK:          NewControllerData[deviceId].ButtonStates[7] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
												 NewControllerData[deviceId].ButtonStates[16] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_THUMBL: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break;
				case AKEYCODE_BUTTON_THUMBR: NewControllerData[deviceId].ButtonStates[9] = buttonDown; break;
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[10] = buttonDown; break;
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break;
				case AKEYCODE_DPAD_UP:       NewControllerData[deviceId].ButtonStates[12] = buttonDown; break;
				case AKEYCODE_DPAD_DOWN:     NewControllerData[deviceId].ButtonStates[13] = buttonDown; break;
				case AKEYCODE_DPAD_LEFT:     NewControllerData[deviceId].ButtonStates[14] = buttonDown; break;
				case AKEYCODE_DPAD_RIGHT:    NewControllerData[deviceId].ButtonStates[15] = buttonDown; break;
			}
			break;

		case ButtonRemapType::XBox:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_A:      NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // A
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // B
				case AKEYCODE_BUTTON_C:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // X
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Y
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_Z:      NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[6] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
												 NewControllerData[deviceId].ButtonStates[17] = buttonDown; // Menu
											 }
											 break;
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[7] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
												 NewControllerData[deviceId].ButtonStates[16] = buttonDown; // View
											 }
											 break;
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // ThumbL
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // ThumbR
			}
			break;

		case ButtonRemapType::PS4:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // Cross
				case AKEYCODE_BUTTON_C:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // Circle
				case AKEYCODE_BUTTON_A:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // Square
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Triangle
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_Z:      NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[6] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
											 	NewControllerData[deviceId].ButtonStates[17] = buttonDown; // Options
											 }
											 break;
				case AKEYCODE_MENU:          NewControllerData[deviceId].ButtonStates[7] = buttonDown;
											 if (!bBlockAndroidKeysOnControllers)
											 {
												 NewControllerData[deviceId].ButtonStates[16] = buttonDown; // Touchpad
											 }
											 break;
				case AKEYCODE_BUTTON_SELECT: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // ThumbL
				case AKEYCODE_BUTTON_START:  NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // ThumbR
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[10] = buttonDown; break; // L2
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break; // R2
			}
			break;
		case ButtonRemapType::PS5:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // Cross
				case AKEYCODE_BUTTON_C:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // Circle
				case AKEYCODE_BUTTON_A:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // Square
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Triangle
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_Z:      NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[6] = buttonDown;
					if (!bBlockAndroidKeysOnControllers)
					{
						NewControllerData[deviceId].ButtonStates[17] = buttonDown; // Options
					}
					break;
				case AKEYCODE_BUTTON_THUMBL:          NewControllerData[deviceId].ButtonStates[7] = buttonDown;
					if (!bBlockAndroidKeysOnControllers)
					{
						NewControllerData[deviceId].ButtonStates[16] = buttonDown; // Touchpad
					}
					break;
				case AKEYCODE_BUTTON_SELECT: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // ThumbL
				case AKEYCODE_BUTTON_START:  NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // ThumbR
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[10]= buttonDown; break; // L2
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break; // R2
			}
			break;
		case ButtonRemapType::PS5New:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_A:		NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // Cross
				case AKEYCODE_BUTTON_B:		NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // Circle
				case AKEYCODE_BUTTON_X:		NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // Triangle
				case AKEYCODE_BUTTON_Y:		NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Square
				case AKEYCODE_BUTTON_L1:	NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_R1:	NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_THUMBL:NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // L3
				case AKEYCODE_BUTTON_THUMBR:NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // R3
				case AKEYCODE_BUTTON_L2:	NewControllerData[deviceId].ButtonStates[10] = buttonDown; break; // L2
				case AKEYCODE_BUTTON_R2:	NewControllerData[deviceId].ButtonStates[11] = buttonDown; break; // R2
				case 3002:		NewControllerData[deviceId].ButtonStates[16] = buttonDown; break; // Touchpad
				case AKEYCODE_BUTTON_START:	NewControllerData[deviceId].ButtonStates[6] = buttonDown; // Options
					if (!bBlockAndroidKeysOnControllers)
					{
						NewControllerData[deviceId].ButtonStates[17] = buttonDown; // Options
					}
					break;
			}
			break;
	}
}


int32 FAndroidInputInterface::GetAlternateKeyEventForMouse(int32 deviceID, int32 buttonID)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	const int32 ControllerIndex = FindExistingDevice(deviceID);
	if(ControllerIndex != -1
		&& buttonID == 0
		&& DeviceMapping[ControllerIndex].DeviceState == Valid
		&& DeviceMapping[ControllerIndex].ControllerClass == PlaystationWireless)
	{
		return 3002;
	}
	return 0;
}

void FAndroidInputInterface::MouseMoveEvent(int32 deviceId, float absoluteX, float absoluteY, float deltaX, float deltaY)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::MouseDataStack.Push(
		MouseData{ MouseEventType::MouseMove, EMouseButtons::Invalid, (int32)absoluteX, (int32)absoluteY, (int32)deltaX, (int32)deltaY, 0.0f });
}

void FAndroidInputInterface::MouseWheelEvent(int32 deviceId, float wheelDelta)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::MouseDataStack.Push(
		MouseData{ MouseEventType::MouseWheel, EMouseButtons::Invalid, 0, 0, 0, 0, wheelDelta });
}

void FAndroidInputInterface::MouseButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	MouseEventType EventType = buttonDown ? MouseEventType::MouseButtonDown : MouseEventType::MouseButtonUp;
	EMouseButtons::Type UnrealButton = (buttonId == 0) ? EMouseButtons::Left : (buttonId == 1) ? EMouseButtons::Right : EMouseButtons::Middle;
	FAndroidInputInterface::MouseDataStack.Push(
		MouseData{ EventType, UnrealButton, 0, 0, 0, 0, 0.0f });
}

void FAndroidInputInterface::DeferMessage(const FDeferredAndroidMessage& DeferredMessage)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	// Get the index we should be writing to
	int32 Index = DeferredMessageQueueLastEntryIndex++;

	if (Index >= MAX_DEFERRED_MESSAGE_QUEUE_SIZE)
	{
		// Actually, if the queue is full, drop the message and increment a counter of drops
		DeferredMessageQueueDroppedCount++;
		return;
	}
	DeferredMessages[Index] = DeferredMessage;
}

void FAndroidInputInterface::QueueMotionData(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	EDeviceScreenOrientation ScreenOrientation = FPlatformMisc::GetDeviceOrientation();
	FVector TempRotationRate = RotationRate;

	if (AndroidUnifyMotionSpace != 0)
	{
		FVector TempTilt = Tilt;
		FVector TempGravity = Gravity;
		FVector TempAcceleration = Acceleration;

		auto ReorientLandscapeLeft = [](FVector InValue)
		{
			return AndroidUnifyMotionSpace == 1 ? FVector(-InValue.Z, -InValue.Y, InValue.X) : FVector(-InValue.Y, -InValue.Z, InValue.X);
		};

		auto ReorientLandscapeRight = [](FVector InValue)
		{
			return AndroidUnifyMotionSpace == 1 ? FVector(-InValue.Z, InValue.Y, -InValue.X) : FVector(InValue.Y, -InValue.Z, -InValue.X);
		};

		auto ReorientPortrait = [](FVector InValue)
		{
			return AndroidUnifyMotionSpace == 1 ? FVector(-InValue.Z, InValue.X, InValue.Y) : FVector(InValue.X, -InValue.Z, InValue.Y);
		};

		const float ToG = 1.f / 9.8f;

		switch (ScreenOrientation)
		{
			// the x tilt is inverted in LandscapeLeft.
		case EDeviceScreenOrientation::LandscapeLeft:
			TempTilt = -ReorientLandscapeLeft(TempTilt);
			TempRotationRate = -ReorientLandscapeLeft(TempRotationRate);
			TempGravity = ReorientLandscapeLeft(TempGravity) * ToG;
			TempAcceleration = ReorientLandscapeLeft(TempAcceleration) * ToG;
			break;
			// the y tilt is inverted in LandscapeRight.
		case EDeviceScreenOrientation::LandscapeRight:
			TempTilt = -ReorientLandscapeRight(TempTilt);
			TempRotationRate = -ReorientLandscapeRight(TempRotationRate);
			TempGravity = ReorientLandscapeRight(TempGravity) * ToG;
			TempAcceleration = ReorientLandscapeRight(TempAcceleration) * ToG;
			break;
		case EDeviceScreenOrientation::Portrait:
			TempTilt = -ReorientPortrait(TempTilt);
			TempRotationRate = -ReorientPortrait(TempRotationRate);
			TempGravity = ReorientPortrait(TempGravity) * ToG;
			TempAcceleration = ReorientPortrait(TempAcceleration) * ToG;
			break;
		}

		if (AndroidUnifyMotionSpace == 2)
		{
			TempRotationRate = -TempRotationRate;
		}

		FAndroidInputInterface::MotionDataStack.Push(
			MotionData{ TempTilt, TempRotationRate, TempGravity, TempAcceleration });
	}
	else
	{
		switch (ScreenOrientation)
		{
			// the x tilt is inverted in LandscapeLeft.
		case EDeviceScreenOrientation::LandscapeLeft:
			TempRotationRate.X *= -1.0f;
			break;
			// the y tilt is inverted in LandscapeRight.
		case EDeviceScreenOrientation::LandscapeRight:
			TempRotationRate.Y *= -1.0f;
			break;
		}

		FAndroidInputInterface::MotionDataStack.Push(
			MotionData{ Tilt, TempRotationRate, Gravity, Acceleration });
	}
}

#endif
