// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusInput.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "OculusHandTracking.h"
#include "OculusMRFunctionLibrary.h"
#include "Misc/CoreDelegates.h"
#include "Features/IModularFeatures.h"
#include "Misc/ConfigCacheIni.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

#define OVR_DEBUG_LOGGING 0

#define LOCTEXT_NAMESPACE "OculusInput"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

namespace OculusInput
{

const FKey FOculusKey::OculusTouch_Left_Thumbstick("OculusTouch_Left_Thumbstick");
const FKey FOculusKey::OculusTouch_Left_Trigger("OculusTouch_Left_Trigger");
const FKey FOculusKey::OculusTouch_Left_FaceButton1("OculusTouch_Left_FaceButton1");
const FKey FOculusKey::OculusTouch_Left_FaceButton2("OculusTouch_Left_FaceButton2");
const FKey FOculusKey::OculusTouch_Left_IndexPointing("OculusTouch_Left_IndexPointing");
const FKey FOculusKey::OculusTouch_Left_ThumbUp("OculusTouch_Left_ThumbUp");
const FKey FOculusKey::OculusTouch_Left_ThumbRest("OculusTouch_Left_ThumbRest");

const FKey FOculusKey::OculusTouch_Right_Thumbstick("OculusTouch_Right_Thumbstick");
const FKey FOculusKey::OculusTouch_Right_Trigger("OculusTouch_Right_Trigger");
const FKey FOculusKey::OculusTouch_Right_FaceButton1("OculusTouch_Right_FaceButton1");
const FKey FOculusKey::OculusTouch_Right_FaceButton2("OculusTouch_Right_FaceButton2");
const FKey FOculusKey::OculusTouch_Right_IndexPointing("OculusTouch_Right_IndexPointing");
const FKey FOculusKey::OculusTouch_Right_ThumbUp("OculusTouch_Right_ThumbUp");
const FKey FOculusKey::OculusTouch_Right_ThumbRest("OculusTouch_Right_ThumbRest");

const FKey FOculusKey::OculusRemote_DPad_Down("OculusRemote_DPad_Down");
const FKey FOculusKey::OculusRemote_DPad_Up("OculusRemote_DPad_Up");
const FKey FOculusKey::OculusRemote_DPad_Left("OculusRemote_DPad_Left");
const FKey FOculusKey::OculusRemote_DPad_Right("OculusRemote_DPad_Right");
const FKey FOculusKey::OculusRemote_Enter("OculusRemote_Enter");
const FKey FOculusKey::OculusRemote_Back("OculusRemote_Back");
const FKey FOculusKey::OculusRemote_VolumeUp("OculusRemote_VolumeUp");
const FKey FOculusKey::OculusRemote_VolumeDown("OculusRemote_VolumeDown");
const FKey FOculusKey::OculusRemote_Home("OculusRemote_Home");

const FKey FOculusKey::OculusHand_Left_ThumbPinch("OculusHand_Left_ThumbPinch");
const FKey FOculusKey::OculusHand_Left_IndexPinch("OculusHand_Left_IndexPinch");
const FKey FOculusKey::OculusHand_Left_MiddlePinch("OculusHand_Left_MiddlePinch");
const FKey FOculusKey::OculusHand_Left_RingPinch("OculusHand_Left_RingPinch");
const FKey FOculusKey::OculusHand_Left_PinkyPinch("OculusHand_Left_PinkPinch");

const FKey FOculusKey::OculusHand_Right_ThumbPinch("OculusHand_Right_ThumbPinch");
const FKey FOculusKey::OculusHand_Right_IndexPinch("OculusHand_Right_IndexPinch");
const FKey FOculusKey::OculusHand_Right_MiddlePinch("OculusHand_Right_MiddlePinch");
const FKey FOculusKey::OculusHand_Right_RingPinch("OculusHand_Right_RingPinch");
const FKey FOculusKey::OculusHand_Right_PinkyPinch("OculusHand_Right_PinkPinch");

const FKey FOculusKey::OculusHand_Left_SystemGesture("OculusHand_Left_SystemGesture");
const FKey FOculusKey::OculusHand_Right_SystemGesture("OculusHand_Right_SystemGesture");

const FKey FOculusKey::OculusHand_Left_ThumbPinchStrength("OculusHand_Left_ThumbPinchStrength");
const FKey FOculusKey::OculusHand_Left_IndexPinchStrength("OculusHand_Left_IndexPinchStrength");
const FKey FOculusKey::OculusHand_Left_MiddlePinchStrength("OculusHand_Left_MiddlePinchStrength");
const FKey FOculusKey::OculusHand_Left_RingPinchStrength("OculusHand_Left_RingPinchStrength");
const FKey FOculusKey::OculusHand_Left_PinkyPinchStrength("OculusHand_Left_PinkPinchStrength");

const FKey FOculusKey::OculusHand_Right_ThumbPinchStrength("OculusHand_Right_ThumbPinchStrength");
const FKey FOculusKey::OculusHand_Right_IndexPinchStrength("OculusHand_Right_IndexPinchStrength");
const FKey FOculusKey::OculusHand_Right_MiddlePinchStrength("OculusHand_Right_MiddlePinchStrength");
const FKey FOculusKey::OculusHand_Right_RingPinchStrength("OculusHand_Right_RingPinchStrength");
const FKey FOculusKey::OculusHand_Right_PinkyPinchStrength("OculusHand_Right_PinkPinchStrength");

const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_Thumbstick("OculusTouch_Left_Thumbstick");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_Trigger("OculusTouch_Left_Trigger");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_FaceButton1("OculusTouch_Left_FaceButton1");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_FaceButton2("OculusTouch_Left_FaceButton2");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_IndexPointing("OculusTouch_Left_IndexPointing");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_ThumbUp("OculusTouch_Left_ThumbUp");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Left_ThumbRest("OculusTouch_Left_ThumbRest");

const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_Thumbstick("OculusTouch_Right_Thumbstick");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_Trigger("OculusTouch_Right_Trigger");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_FaceButton1("OculusTouch_Right_FaceButton1");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_FaceButton2("OculusTouch_Right_FaceButton2");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_IndexPointing("OculusTouch_Right_IndexPointing");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_ThumbUp("OculusTouch_Right_ThumbUp");
const FOculusKeyNames::Type FOculusKeyNames::OculusTouch_Right_ThumbRest("OculusTouch_Right_ThumbRest");

const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_DPad_Down("OculusRemote_DPad_Down");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_DPad_Up("OculusRemote_DPad_Up");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_DPad_Left("OculusRemote_DPad_Left");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_DPad_Right("OculusRemote_DPad_Right");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_Enter("OculusRemote_Enter");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_Back("OculusRemote_Back");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_VolumeUp("OculusRemote_VolumeUp");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_VolumeDown("OculusRemote_VolumeDown");
const FOculusKeyNames::Type FOculusKeyNames::OculusRemote_Home("OculusRemote_Home");

const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_ThumbPinch("OculusHand_Left_ThumbPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_IndexPinch("OculusHand_Left_IndexPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_MiddlePinch("OculusHand_Left_MiddlePinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_RingPinch("OculusHand_Left_RingPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_PinkyPinch("OculusHand_Left_PinkPinch");

const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_ThumbPinch("OculusHand_Right_ThumbPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_IndexPinch("OculusHand_Right_IndexPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_MiddlePinch("OculusHand_Right_MiddlePinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_RingPinch("OculusHand_Right_RingPinch");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_PinkyPinch("OculusHand_Right_PinkPinch");

const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_SystemGesture("OculusHand_Left_SystemGesture");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_SystemGesture("OculusHand_Right_SystemGesture");

const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_ThumbPinchStrength("OculusHand_Left_ThumbPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_IndexPinchStrength("OculusHand_Left_IndexPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_MiddlePinchStrength("OculusHand_Left_MiddlePinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_RingPinchStrength("OculusHand_Left_RingPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Left_PinkyPinchStrength("OculusHand_Left_PinkPinchStrength");

const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_ThumbPinchStrength("OculusHand_Right_ThumbPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_IndexPinchStrength("OculusHand_Right_IndexPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_MiddlePinchStrength("OculusHand_Right_MiddlePinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_RingPinchStrength("OculusHand_Right_RingPinchStrength");
const FOculusKeyNames::Type FOculusKeyNames::OculusHand_Right_PinkyPinchStrength("OculusHand_Right_PinkPinchStrength");

/** Threshold for treating trigger pulls as button presses, from 0.0 to 1.0 */
float FOculusInput::TriggerThreshold = 0.8f;

/** Are Remote keys mapped to gamepad or not. */
bool FOculusInput::bRemoteKeysMappedToGamepad = true;

float FOculusInput::InitialButtonRepeatDelay = 0.2f;
float FOculusInput::ButtonRepeatDelay = 0.1f;

FOculusInput::FOculusInput( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: OVRPluginHandle(nullptr)
	, MessageHandler( InMessageHandler )
	, ControllerPairs()
{
	// take care of backward compatibility of Remote with Gamepad
	if (bRemoteKeysMappedToGamepad)
	{
		Remote.MapKeysToGamepad();
	}

	OVRPluginHandle = FOculusHMDModule::GetOVRPluginHandle();

	FOculusControllerPair& ControllerPair = *new(ControllerPairs) FOculusControllerPair();

	// TODO: Map the oculus controllers uniquely instead of using the default
	ControllerPair.DeviceId = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();

	IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );

	LocalTrackingSpaceRecenterCount = 0;

	UE_LOG(LogOcInput, Log, TEXT("OculusInput is initialized"));
}


FOculusInput::~FOculusInput()
{
	IModularFeatures::Get().UnregisterModularFeature( GetModularFeatureName(), this );

	if (OVRPluginHandle)
	{
		FPlatformProcess::FreeDllHandle(OVRPluginHandle);
		OVRPluginHandle = nullptr;
	}
}

void FOculusInput::PreInit()
{
	// Load the config, even if we failed to initialize a controller
	LoadConfig();

	// Register the FKeys
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_Thumbstick, LOCTEXT("OculusTouch_Left_Thumbstick", "Oculus Touch (L) Thumbstick CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_FaceButton1, LOCTEXT("OculusTouch_Left_FaceButton1", "Oculus Touch (L) X Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_Trigger, LOCTEXT("OculusTouch_Left_Trigger", "Oculus Touch (L) Trigger CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_FaceButton2, LOCTEXT("OculusTouch_Left_FaceButton2", "Oculus Touch (L) Y Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_IndexPointing, LOCTEXT("OculusTouch_Left_IndexPointing", "Oculus Touch (L) Pointing CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_ThumbUp, LOCTEXT("OculusTouch_Left_ThumbUp", "Oculus Touch (L) Thumb Up CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Left_ThumbRest, LOCTEXT("OculusTouch_Left_ThumbRest", "Oculus Touch (L) Thumb Rest CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_Thumbstick, LOCTEXT("OculusTouch_Right_Thumbstick", "Oculus Touch (R) Thumbstick CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_FaceButton1, LOCTEXT("OculusTouch_Right_FaceButton1", "Oculus Touch (R) A Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_Trigger, LOCTEXT("OculusTouch_Right_Trigger", "Oculus Touch (R) Trigger CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_FaceButton2, LOCTEXT("OculusTouch_Right_FaceButton2", "Oculus Touch (R) B Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_IndexPointing, LOCTEXT("OculusTouch_Right_IndexPointing", "Oculus Touch (R) Pointing CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_ThumbUp, LOCTEXT("OculusTouch_Right_ThumbUp", "Oculus Touch (R) Thumb Up CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusTouch_Right_ThumbRest, LOCTEXT("OculusTouch_Right_ThumbRest", "Oculus Touch (R) Thumb Rest CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "OculusTouch"));

	EKeys::AddMenuCategoryDisplayInfo("OculusRemote", LOCTEXT("OculusRemoteSubCategory", "Oculus Remote"), TEXT("GraphEditor.PadEvent_16x"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_DPad_Up, LOCTEXT("OculusRemote_DPad_Up", "Oculus Remote D-pad Up"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_DPad_Down, LOCTEXT("OculusRemote_DPad_Down", "Oculus Remote D-pad Down"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_DPad_Left, LOCTEXT("OculusRemote_DPad_Left", "Oculus Remote D-pad Left"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_DPad_Right, LOCTEXT("OculusRemote_DPad_Right", "Oculus Remote D-pad Right"), FKeyDetails::GamepadKey, "OculusRemote"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_Enter, LOCTEXT("OculusRemote_Enter", "Oculus Remote Enter"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_Back, LOCTEXT("OculusRemote_Back", "Oculus Remote Back"), FKeyDetails::GamepadKey, "OculusRemote"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_VolumeUp, LOCTEXT("OculusRemote_VolumeUp", "Oculus Remote Volume Up"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_VolumeDown, LOCTEXT("OculusRemote_VolumeDown", "Oculus Remote Volume Down"), FKeyDetails::GamepadKey, "OculusRemote"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusRemote_Home, LOCTEXT("OculusRemote_Home", "Oculus Remote Home"), FKeyDetails::GamepadKey, "OculusRemote"));

	EKeys::AddMenuCategoryDisplayInfo("OculusHand", LOCTEXT("OculusHandSubCategory", "Oculus Hand"), TEXT("GraphEditor.PadEvent_16x"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_ThumbPinch, LOCTEXT("OculusHand_Left_ThumbPinch", "Oculus Hand (L) Thumb Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_IndexPinch, LOCTEXT("OculusHand_Left_IndexPinch", "Oculus Hand (L) Index Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_MiddlePinch, LOCTEXT("OculusHand_Left_MiddlePinch", "Oculus Hand (L) Middle Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_RingPinch, LOCTEXT("OculusHand_Left_RingPinch", "Oculus Hand (L) Ring Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_PinkyPinch, LOCTEXT("OculusHand_Left_PinkyPinch", "Oculus Hand (L) Pinky Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_ThumbPinch, LOCTEXT("OculusHand_Right_ThumbPinch", "Oculus Hand (R) Thumb Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_IndexPinch, LOCTEXT("OculusHand_Right_IndexPinch", "Oculus Hand (R) Index Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_MiddlePinch, LOCTEXT("OculusHand_Right_MiddlePinch", "Oculus Hand (R) Middle Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_RingPinch, LOCTEXT("OculusHand_Right_RingPinch", "Oculus Hand (R) Ring Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_PinkyPinch, LOCTEXT("OculusHand_Right_PinkyPinch", "Oculus Hand (R) Pinky Pinch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_SystemGesture, LOCTEXT("OculusHand_Left_SystemGesture", "Oculus Hand (L) System Gesture"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_SystemGesture, LOCTEXT("OculusHand_Right_SystemGesture", "Oculus Hand (R) System Gesture"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "OculusHand"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_ThumbPinchStrength, LOCTEXT("OculusHand_Left_ThumbPinchStrength", "Oculus Hand (L) Thumb Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_IndexPinchStrength, LOCTEXT("OculusHand_Left_IndexPinchStrength", "Oculus Hand (L) Index Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_MiddlePinchStrength, LOCTEXT("OculusHand_Left_MiddlePinchStrength", "Oculus Hand (L) Middle Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_RingPinchStrength, LOCTEXT("OculusHand_Left_RingPinchStrength", "Oculus Hand (L) Ring Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Left_PinkyPinchStrength, LOCTEXT("OculusHand_Left_PinkyPinchStrength", "Oculus Hand (L) Pinky Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));

	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_ThumbPinchStrength, LOCTEXT("OculusHand_Right_ThumbPinchStrength", "Oculus Hand (R) Thumb Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_IndexPinchStrength, LOCTEXT("OculusHand_Right_IndexPinchStrength", "Oculus Hand (R) Index Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_MiddlePinchStrength, LOCTEXT("OculusHand_Right_MiddlePinchStrength", "Oculus Hand (R) Middle Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_RingPinchStrength, LOCTEXT("OculusHand_Right_RingPinchStrength", "Oculus Hand (R) Ring Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));
	EKeys::AddKey(FKeyDetails(FOculusKey::OculusHand_Right_PinkyPinchStrength, LOCTEXT("OculusHand_Right_PinkyPinchStrength", "Oculus Hand (R) Pinky Pinch Strength"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D, "OculusHand"));

	UE_LOG(LogOcInput, Log, TEXT("OculusInput pre-init called"));
}

void FOculusInput::LoadConfig()
{
	const TCHAR* OculusTouchSettings = TEXT("OculusTouch.Settings");
	float ConfigThreshold = TriggerThreshold;
	if (GConfig->GetFloat(OculusTouchSettings, TEXT("TriggerThreshold"), ConfigThreshold, GEngineIni))
	{
		TriggerThreshold = ConfigThreshold;
	}

	const TCHAR* OculusRemoteSettings = TEXT("OculusRemote.Settings");
	bool bConfigRemoteKeysMappedToGamepad;
	if (GConfig->GetBool(OculusRemoteSettings, TEXT("bRemoteKeysMappedToGamepad"), bConfigRemoteKeysMappedToGamepad, GEngineIni))
	{
		bRemoteKeysMappedToGamepad = bConfigRemoteKeysMappedToGamepad;
	}

	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("InitialButtonRepeatDelay"), InitialButtonRepeatDelay, GInputIni);
	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("ButtonRepeatDelay"), ButtonRepeatDelay, GInputIni);
}

void FOculusInput::Tick( float DeltaTime )
{
	// Nothing to do when ticking, for now.  SendControllerEvents() handles everything.
}


void FOculusInput::SendControllerEvents()
{
	const double CurrentTime = FPlatformTime::Seconds();
	const float AnalogButtonPressThreshold = TriggerThreshold;

	if(IOculusHMDModule::IsAvailable() && FOculusHMDModule::GetPluginWrapper().GetInitialized() && FApp::HasVRFocus())
	{
		if (MessageHandler.IsValid() && GEngine->XRSystem->GetHMDDevice())
		{
			FPlatformUserId PlatUser = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
			FInputDeviceId DeviceId = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
			
			OculusHMD::FOculusHMD* OculusHMD = static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem->GetHMDDevice());
			OculusHMD->StartGameFrame_GameThread();

			ovrpControllerState4 OvrpControllerState;

			if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerState4(ovrpController_Remote, &OvrpControllerState)) &&
				(OvrpControllerState.ConnectedControllerTypes & ovrpController_Remote))
			{
				for (int32 ButtonIndex = 0; ButtonIndex < (int32)EOculusRemoteControllerButton::TotalButtonCount; ++ButtonIndex)
				{
					FOculusButtonState& ButtonState = Remote.Buttons[ButtonIndex];
					check(!ButtonState.Key.IsNone()); // is button's name initialized?

					// Determine if the button is pressed down
					bool bButtonPressed = false;
					switch ((EOculusRemoteControllerButton)ButtonIndex)
					{
					case EOculusRemoteControllerButton::DPad_Up:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Up) != 0;
						break;

					case EOculusRemoteControllerButton::DPad_Down:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Down) != 0;
						break;

					case EOculusRemoteControllerButton::DPad_Left:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Left) != 0;
						break;

					case EOculusRemoteControllerButton::DPad_Right:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Right) != 0;
						break;

					case EOculusRemoteControllerButton::Enter:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Start) != 0;
						break;

					case EOculusRemoteControllerButton::Back:
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Back) != 0;
						break;

					case EOculusRemoteControllerButton::VolumeUp:
						#ifdef SUPPORT_INTERNAL_BUTTONS
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_VolUp) != 0;
						#endif
						break;

					case EOculusRemoteControllerButton::VolumeDown:
						#ifdef SUPPORT_INTERNAL_BUTTONS
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_VolDown) != 0;
						#endif
						break;

					case EOculusRemoteControllerButton::Home:
						#ifdef SUPPORT_INTERNAL_BUTTONS
						bButtonPressed = (OvrpControllerState.Buttons & ovrpButton_Home) != 0;
						#endif
						break;

					default:
						check(0); // unhandled button, shouldn't happen
						break;
					}

					// Update button state
					if (bButtonPressed != ButtonState.bIsPressed)
					{
						ButtonState.bIsPressed = bButtonPressed;
						if (ButtonState.bIsPressed)
						{
							OnControllerButtonPressed(ButtonState, PlatUser, DeviceId, false);

							// Set the timer for the first repeat
							ButtonState.NextRepeatTime = CurrentTime + InitialButtonRepeatDelay;
						}
						else
						{
							OnControllerButtonReleased(ButtonState, PlatUser, DeviceId, false);
						}
					}

					// Apply key repeat, if its time for that
					if (ButtonState.bIsPressed && ButtonState.NextRepeatTime <= CurrentTime)
					{
						OnControllerButtonPressed(ButtonState, PlatUser, DeviceId, true);

						// Set the timer for the next repeat
						ButtonState.NextRepeatTime = CurrentTime + ButtonRepeatDelay;
					}
				}
			}

			if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerState4((ovrpController)(ovrpController_LTrackedRemote | ovrpController_RTrackedRemote | ovrpController_Touch), &OvrpControllerState)))
			{
				UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ButtonState = 0x%X"), OvrpControllerState.Buttons);
				UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: Touches = 0x%X"), OvrpControllerState.Touches);

				// If using touch controllers (Quest) use the local tracking space recentering as a signal for recenter
				if ((OvrpControllerState.ConnectedControllerTypes & ovrpController_LTouch) != 0 || (OvrpControllerState.ConnectedControllerTypes & ovrpController_RTouch) != 0)
				{
					int32 recenterCount = 0;
					if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetLocalTrackingSpaceRecenterCount(&recenterCount)))
					{
						if (LocalTrackingSpaceRecenterCount != recenterCount)
						{
							FCoreDelegates::VRControllerRecentered.Broadcast();
							LocalTrackingSpaceRecenterCount = recenterCount;
						}
					}
				}

				for (FOculusControllerPair& ControllerPair : ControllerPairs)
				{
					FPlatformUserId PlatformUser = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(ControllerPair.DeviceId);
					for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(ControllerPair.TouchControllerStates); ++HandIndex)
					{
						FOculusTouchControllerState& State = ControllerPair.TouchControllerStates[HandIndex];
						bool bIsLeft = (HandIndex == (int32)EControllerHand::Left);

						bool bIsMobileController = bIsLeft ? (OvrpControllerState.ConnectedControllerTypes & ovrpController_LTrackedRemote) != 0 : (OvrpControllerState.ConnectedControllerTypes & ovrpController_RTrackedRemote) != 0;
						bool bIsTouchController = bIsLeft ? (OvrpControllerState.ConnectedControllerTypes & ovrpController_LTouch) != 0 : (OvrpControllerState.ConnectedControllerTypes & ovrpController_RTouch) != 0;
						bool bIsCurrentlyTracked = bIsMobileController || bIsTouchController;

						if (bIsCurrentlyTracked)
						{
							ovrpNode OvrpNode = (HandIndex == (int32)EControllerHand::Left) ? ovrpNode_HandLeft : ovrpNode_HandRight;

							State.bIsConnected = true;
							ovrpBool bResult = true;
							State.bIsPositionTracked = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodePositionTracked2(OvrpNode, &bResult)) && bResult;
							State.bIsPositionValid = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodePositionValid(OvrpNode, &bResult)) && bResult;
							State.bIsOrientationTracked = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodeOrientationTracked2(OvrpNode, &bResult)) && bResult;
							State.bIsOrientationValid = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodeOrientationValid(OvrpNode, &bResult)) && bResult;

							const float OvrTriggerAxis = OvrpControllerState.IndexTrigger[HandIndex];
							const float OvrGripAxis = OvrpControllerState.HandTrigger[HandIndex];

							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: IndexTrigger[%d] = %f"), int(HandIndex), OvrTriggerAxis);
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: HandTrigger[%d] = %f"), int(HandIndex), OvrGripAxis);
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ThumbStick[%d] = { %f, %f }"), int(HandIndex), OvrpControllerState.Thumbstick[HandIndex].x, OvrpControllerState.Thumbstick[HandIndex].y);

							if (bIsMobileController)
							{
								if (OvrpControllerState.RecenterCount[HandIndex] != State.RecenterCount)
								{
									State.RecenterCount = OvrpControllerState.RecenterCount[HandIndex];
									FCoreDelegates::VRControllerRecentered.Broadcast();
								}
							}

							if (OvrTriggerAxis != State.TriggerAxis)
							{
								State.TriggerAxis = OvrTriggerAxis;
								MessageHandler->OnControllerAnalog(bIsLeft ? EKeys::OculusTouch_Left_Trigger_Axis.GetFName() : EKeys::OculusTouch_Right_Trigger_Axis.GetFName(), PlatformUser, ControllerPair.DeviceId, State.TriggerAxis);
							}

							if (OvrGripAxis != State.GripAxis)
							{
								State.GripAxis = OvrGripAxis;
								MessageHandler->OnControllerAnalog(bIsLeft ? EKeys::OculusTouch_Left_Grip_Axis.GetFName() : EKeys::OculusTouch_Right_Grip_Axis.GetFName(), PlatformUser, ControllerPair.DeviceId, State.GripAxis);
							}

							ovrpVector2f ThumbstickValue = OvrpControllerState.Thumbstick[HandIndex];
							ovrpVector2f TouchpadValue = OvrpControllerState.Touchpad[HandIndex];

							if (ThumbstickValue.x != State.ThumbstickAxes.X)
							{
								State.ThumbstickAxes.X = ThumbstickValue.x;
								MessageHandler->OnControllerAnalog(bIsLeft ? EKeys::OculusTouch_Left_Thumbstick_X.GetFName() : EKeys::OculusTouch_Right_Thumbstick_X.GetFName(), PlatformUser, ControllerPair.DeviceId, State.ThumbstickAxes.X);
							}

							if (ThumbstickValue.y != State.ThumbstickAxes.Y)
							{
								State.ThumbstickAxes.Y = ThumbstickValue.y;
								MessageHandler->OnControllerAnalog(bIsLeft ? EKeys::OculusTouch_Left_Thumbstick_Y.GetFName() : EKeys::OculusTouch_Right_Thumbstick_Y.GetFName(), PlatformUser, ControllerPair.DeviceId, State.ThumbstickAxes.Y);
							}

							if (TouchpadValue.x != State.TouchpadAxes.X)
							{
								State.TouchpadAxes.X = TouchpadValue.x;
							}

							if (TouchpadValue.y != State.TouchpadAxes.Y)
							{
								State.TouchpadAxes.Y = TouchpadValue.y;
							}

							for (int32 ButtonIndex = 0; ButtonIndex < (int32)EOculusTouchControllerButton::TotalButtonCount; ++ButtonIndex)
							{
								FOculusButtonState& ButtonState = State.Buttons[ButtonIndex];
								check(!ButtonState.Key.IsNone()); // is button's name initialized?

								// Determine if the button is pressed down
								bool bButtonPressed = false;
								switch ((EOculusTouchControllerButton)ButtonIndex)
								{
								case EOculusTouchControllerButton::Trigger:
									bButtonPressed = State.TriggerAxis >= AnalogButtonPressThreshold;
									break;

								case EOculusTouchControllerButton::Grip:
									bButtonPressed = State.GripAxis >= AnalogButtonPressThreshold;
									break;

								case EOculusTouchControllerButton::XA:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Buttons & ovrpButton_X) != 0 : (OvrpControllerState.Buttons & ovrpButton_A) != 0;
									break;

								case EOculusTouchControllerButton::YB:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Buttons & ovrpButton_Y) != 0 : (OvrpControllerState.Buttons & ovrpButton_B) != 0;
									break;

								case EOculusTouchControllerButton::Thumbstick:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Buttons & ovrpButton_LThumb) != 0 : (OvrpControllerState.Buttons & ovrpButton_RThumb) != 0;
									break;

								case EOculusTouchControllerButton::Thumbstick_Up:
									if (bIsTouchController && State.ThumbstickAxes.Size() > 0.7f ||
										bIsMobileController && State.Buttons[(int)EOculusTouchControllerButton::Thumbstick].bIsPressed && State.ThumbstickAxes.Size() > 0.5f)
									{
										float Angle = FMath::Atan2(State.ThumbstickAxes.Y, State.ThumbstickAxes.X);
										bButtonPressed = Angle >= (1.0f / 8.0f) * PI && Angle <= (7.0f / 8.0f) * PI;
									}
									break;

								case EOculusTouchControllerButton::Thumbstick_Down:
									if (bIsTouchController && State.ThumbstickAxes.Size() > 0.7f ||
										bIsMobileController && State.Buttons[(int)EOculusTouchControllerButton::Thumbstick].bIsPressed && State.ThumbstickAxes.Size() > 0.5f)
									{
										float Angle = FMath::Atan2(State.ThumbstickAxes.Y, State.ThumbstickAxes.X);
										bButtonPressed = Angle >= (-7.0f / 8.0f) * PI && Angle <= (-1.0f / 8.0f) * PI;
									}
									break;

								case EOculusTouchControllerButton::Thumbstick_Left:
									if (bIsTouchController && State.ThumbstickAxes.Size() > 0.7f ||
										bIsMobileController && State.Buttons[(int)EOculusTouchControllerButton::Thumbstick].bIsPressed && State.ThumbstickAxes.Size() > 0.5f)
									{
										float Angle = FMath::Atan2(State.ThumbstickAxes.Y, State.ThumbstickAxes.X);
										bButtonPressed = Angle <= (-5.0f / 8.0f) * PI || Angle >= (5.0f / 8.0f) * PI;
									}
									break;

								case EOculusTouchControllerButton::Thumbstick_Right:
									if (bIsTouchController && State.ThumbstickAxes.Size() > 0.7f ||
										bIsMobileController && State.Buttons[(int)EOculusTouchControllerButton::Thumbstick].bIsPressed && State.ThumbstickAxes.Size() > 0.5f)
									{
										float Angle = FMath::Atan2(State.ThumbstickAxes.Y, State.ThumbstickAxes.X);
										bButtonPressed = Angle >= (-3.0f / 8.0f) * PI && Angle <= (3.0f / 8.0f) * PI;
									}
									break;

								case EOculusTouchControllerButton::Menu:
									bButtonPressed = bIsLeft && (OvrpControllerState.Buttons & ovrpButton_Start);
									break;

								case EOculusTouchControllerButton::Thumbstick_Touch:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Touches & ovrpTouch_LThumb) != 0 : (OvrpControllerState.Touches & ovrpTouch_RThumb) != 0;
									break;

								case EOculusTouchControllerButton::Trigger_Touch:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Touches & ovrpTouch_LIndexTrigger) != 0 : (OvrpControllerState.Touches & ovrpTouch_RIndexTrigger) != 0;
									break;

								case EOculusTouchControllerButton::XA_Touch:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Touches & ovrpTouch_X) != 0 : (OvrpControllerState.Touches & ovrpTouch_A) != 0;
									break;

								case EOculusTouchControllerButton::YB_Touch:
									bButtonPressed = bIsLeft ? (OvrpControllerState.Touches & ovrpTouch_Y) != 0 : (OvrpControllerState.Touches & ovrpTouch_B) != 0;
									break;

								default:
									check(0);
									break;
								}

								// Update button state
								if (bButtonPressed != ButtonState.bIsPressed)
								{
									ButtonState.bIsPressed = bButtonPressed;
									if (ButtonState.bIsPressed)
									{
										OnControllerButtonPressed(ButtonState, PlatformUser, ControllerPair.DeviceId, false);

										// Set the timer for the first repeat
										ButtonState.NextRepeatTime = CurrentTime + InitialButtonRepeatDelay;
									}
									else
									{
										OnControllerButtonReleased(ButtonState, PlatformUser, ControllerPair.DeviceId, false);
									}
								}

								// Apply key repeat, if its time for that
								if (ButtonState.bIsPressed && ButtonState.NextRepeatTime <= CurrentTime)
								{
									OnControllerButtonPressed(ButtonState, PlatformUser, ControllerPair.DeviceId, true);

									// Set the timer for the next repeat
									ButtonState.NextRepeatTime = CurrentTime + ButtonRepeatDelay;
								}
							}

							// Handle Capacitive States
							for (int32 CapTouchIndex = 0; CapTouchIndex < (int32)EOculusTouchCapacitiveAxes::TotalAxisCount; ++CapTouchIndex)
							{
								FOculusAxisState& CapState = State.CapacitiveAxes[CapTouchIndex];

								float CurrentAxisVal = 0.f;
								switch ((EOculusTouchCapacitiveAxes)CapTouchIndex)
								{
								case EOculusTouchCapacitiveAxes::XA:
								{
									const uint32 mask = (bIsLeft) ? ovrpTouch_X : ovrpTouch_A;
									CurrentAxisVal = (OvrpControllerState.Touches & mask) != 0 ? 1.f : 0.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::YB:
								{
									const uint32 mask = (bIsLeft) ? ovrpTouch_Y : ovrpTouch_B;
									CurrentAxisVal = (OvrpControllerState.Touches & mask) != 0 ? 1.f : 0.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::Thumbstick:
								{
									const uint32 mask = bIsMobileController ? ((bIsLeft) ? ovrpTouch_LTouchpad : ovrpTouch_RTouchpad) : ((bIsLeft) ? ovrpTouch_LThumb : ovrpTouch_RThumb);
									CurrentAxisVal = (OvrpControllerState.Touches & mask) != 0 ? 1.f : 0.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::Trigger:
								{
									const uint32 mask = (bIsLeft) ? ovrpTouch_LIndexTrigger : ovrpTouch_RIndexTrigger;
									CurrentAxisVal = (OvrpControllerState.Touches & mask) != 0 ? 1.f : 0.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::IndexPointing:
								{
									const uint32 mask = (bIsLeft) ? ovrpNearTouch_LIndexTrigger : ovrpNearTouch_RIndexTrigger;
									CurrentAxisVal = (OvrpControllerState.NearTouches & mask) != 0 ? 0.f : 1.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::ThumbUp:
								{
									const uint32 mask = (bIsLeft) ? ovrpNearTouch_LThumbButtons : ovrpNearTouch_RThumbButtons;
									CurrentAxisVal = (OvrpControllerState.NearTouches & mask) != 0 ? 0.f : 1.f;
									break;
								}
								case EOculusTouchCapacitiveAxes::ThumbRest:
								{
									const uint32 mask = (bIsLeft) ? ovrpTouch_LThumbRest : ovrpTouch_RThumbRest;
									CurrentAxisVal = (OvrpControllerState.Touches & mask) != 0 ? 1.f : 0.f;
									break;
								}
								default:
									check(0);
								}

								if (CurrentAxisVal != CapState.State)
								{
									MessageHandler->OnControllerAnalog(CapState.Axis, PlatformUser, ControllerPair.DeviceId, CurrentAxisVal);

									CapState.State = CurrentAxisVal;
								}
							}
						}
						else
						{
							// Controller isn't available right now.  Zero out input state, so that if it comes back it will send fresh event deltas
							State = FOculusTouchControllerState((EControllerHand)HandIndex);
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: Controller for the hand %d is not tracked"), int(HandIndex));
						}
					}
				}
			}
			else
			{
				// Controller isn't available right now.  Zero out input state, so that if it comes back it will send fresh event deltas
				for (FOculusControllerPair& ControllerPair : ControllerPairs)
				{
					for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(ControllerPair.TouchControllerStates); ++HandIndex)
					{
						FOculusTouchControllerState& State = ControllerPair.TouchControllerStates[HandIndex];
						State = FOculusTouchControllerState((EControllerHand)HandIndex);
					}
				}
			}

			if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerState4((ovrpController)(ovrpController_LHand | ovrpController_RHand | ovrpController_Hands), &OvrpControllerState)))
			{
				for (FOculusControllerPair& ControllerPair : ControllerPairs)
				{
					FPlatformUserId PlatformUser = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(ControllerPair.DeviceId);
					for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(ControllerPair.HandControllerStates); ++HandIndex)
					{
						FOculusHandControllerState& State = ControllerPair.HandControllerStates[HandIndex];

						bool bIsLeft = (HandIndex == (int32)EControllerHand::Left);
						bool bIsCurrentlyTracked = bIsLeft ? (OvrpControllerState.ConnectedControllerTypes & ovrpController_LHand) != 0 : (OvrpControllerState.ConnectedControllerTypes & ovrpController_RHand) != 0;

						if (bIsCurrentlyTracked)
						{
							State.bIsConnected = true;
							ovrpBool bResult = true;

							// Hand Tracking requires the frame number for accurate results
							OculusHMD::FGameFrame* CurrentFrame;
							if (IsInGameThread())
							{
								CurrentFrame = OculusHMD->GetNextFrameToRender();
							}
							else
							{
								CurrentFrame = OculusHMD->GetFrame_RenderThread();
							}

							// Poll for Hand Tracking State
							ovrpHandState HandState;
							if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetHandState2(ovrpStep_Render, CurrentFrame ? CurrentFrame->FrameNumber : OVRP_CURRENT_FRAMEINDEX, (ovrpHand)HandIndex, &HandState)))
							{
								// Update various data about hands
								State.HandScale = HandState.HandScale;

								// Update Bone Rotations
								for (uint32 BoneIndex = 0; BoneIndex < UE_ARRAY_COUNT(State.BoneRotations); BoneIndex++)
								{
									ovrpQuatf RawRotation = HandState.BoneRotations[BoneIndex];
									FQuat BoneRotation = FOculusHandTracking::OvrBoneQuatToFQuat(RawRotation);
									BoneRotation.Normalize();
									State.BoneRotations[BoneIndex] = BoneRotation;
								}
								
								// Update Pinch State and Pinch Strength
								bool bTracked = (HandState.Status & ovrpHandStatus_HandTracked) != 0;
								State.TrackingConfidence = FOculusHandTracking::ToETrackingConfidence(HandState.HandConfidence);

								State.bIsPositionTracked = bTracked && State.TrackingConfidence == ETrackingConfidence::High;
								State.bIsPositionValid = bTracked;
								State.bIsOrientationTracked = bTracked && State.TrackingConfidence == ETrackingConfidence::High;
								State.bIsOrientationValid = bTracked;

								State.bIsPointerPoseValid = (HandState.Status & ovrpHandStatus_InputValid) != 0;

								ovrpPosef PointerPose = HandState.PointerPose;
								State.PointerPose.SetTranslation(OculusHMD::ToFVector(PointerPose.Position));
								State.PointerPose.SetRotation(OculusHMD::ToFQuat(PointerPose.Orientation));

								State.bIsDominantHand = (HandState.Status & ovrpHandStatus_DominantHand) != 0;
								
								// Poll for finger confidence
								for (uint32 FingerIndex = 0; FingerIndex < (int32)EOculusHandAxes::TotalAxisCount; FingerIndex++)
								{
									State.FingerConfidences[FingerIndex] = FOculusHandTracking::ToETrackingConfidence(HandState.FingerConfidences[FingerIndex]);
								}

								// Poll for finger pinches
								for (uint32 FingerIndex = 0; FingerIndex < (uint32)EOculusHandButton::TotalButtonCount; FingerIndex++)
								{
									FOculusButtonState& PinchState = State.HandButtons[FingerIndex];
									check(!PinchState.Key.IsNone());

									bool bPressed = false;
									if (FingerIndex < (uint32)EOculusHandButton::System)
									{
										bPressed = (((uint32)HandState.Pinches & (1 << FingerIndex)) != 0);
										bPressed &= (HandState.HandConfidence == ovrpTrackingConfidence_High) && (HandState.FingerConfidences[FingerIndex] == ovrpTrackingConfidence_High);
									}
									else if(FingerIndex == (uint32)EOculusHandButton::System)
									{
										bPressed = (HandState.Status & ovrpHandStatus_SystemGestureInProgress) != 0;
									}
									else
									{
										bPressed = (OvrpControllerState.Buttons & ovrpButton_Start) != 0 && !State.bIsDominantHand;
									}

									if (bPressed != PinchState.bIsPressed)
									{
										PinchState.bIsPressed = bPressed;
										if (PinchState.bIsPressed)
										{
											OnControllerButtonPressed(PinchState, PlatformUser, ControllerPair.DeviceId, false);
										}
										else
										{
											OnControllerButtonReleased(PinchState, PlatformUser, ControllerPair.DeviceId, false);
										}
									}
								}

								// Poll for finger strength
								for (uint32 FingerIndex = 0; FingerIndex < (uint32)EOculusHandAxes::TotalAxisCount; FingerIndex++)
								{
									FOculusAxisState& PinchStrength = State.HandAxes[FingerIndex];
									check(!PinchStrength.Axis.IsNone());

									float PinchValue = 0.0f;
									if (HandState.HandConfidence == ovrpTrackingConfidence_High)
									{
										PinchValue = HandState.PinchStrength[FingerIndex];
									}

									if (PinchValue != PinchStrength.State)
									{
										MessageHandler->OnControllerAnalog(PinchStrength.Axis, PlatformUser, ControllerPair.DeviceId, PinchValue);
										PinchStrength.State = PinchValue;
									}
								}
							}
						}
						else
						{
							State = FOculusHandControllerState((EControllerHand)HandIndex);
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: Hand for the hand %d is not tracked"), int32(HandIndex));
						}
					}
				}
			}
			else
			{
				// Hands are not availble right now, zero out the hand state
				for (FOculusControllerPair& ControllerPair : ControllerPairs)
				{
					for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(ControllerPair.HandControllerStates); ++HandIndex)
					{
						FOculusHandControllerState& State = ControllerPair.HandControllerStates[HandIndex];
						State = FOculusHandControllerState((EControllerHand)HandIndex);
					}
				}
			}
		}
	}
	UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT(""));
}


void FOculusInput::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}


bool FOculusInput::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// No exec commands supported, for now.
	return false;
}

void FOculusInput::SetChannelValue( int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value )
{
	const EControllerHand Hand = ( ChannelType == FForceFeedbackChannelType::LEFT_LARGE || ChannelType == FForceFeedbackChannelType::LEFT_SMALL ) ? EControllerHand::Left : EControllerHand::Right;

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerId);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, InPlatformUser, InDeviceId);
	
	for( FOculusControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.DeviceId == InDeviceId )
		{
			FOculusTouchControllerState& ControllerState = ControllerPair.TouchControllerStates[ (int32)Hand ];

			if (ControllerState.bPlayingHapticEffect)
			{
				continue;
			}

			// @todo: The SMALL channel controls frequency, the LARGE channel controls amplitude.  This is a bit of a weird fit.
			if( ChannelType == FForceFeedbackChannelType::LEFT_SMALL || ChannelType == FForceFeedbackChannelType::RIGHT_SMALL )
			{
				ControllerState.ForceFeedbackHapticFrequency = Value;
			}
			else
			{
				ControllerState.ForceFeedbackHapticAmplitude = Value;
			}

			UpdateForceFeedback( ControllerPair, Hand );

			break;
		}
	}
}

void FOculusInput::SetChannelValues( int32 ControllerId, const FForceFeedbackValues& Values )
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerId);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, InPlatformUser, InDeviceId);
	
	for( FOculusControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.DeviceId == InDeviceId )
		{
			// @todo: The SMALL channel controls frequency, the LARGE channel controls amplitude.  This is a bit of a weird fit.
			FOculusTouchControllerState& LeftControllerState = ControllerPair.TouchControllerStates[ (int32)EControllerHand::Left ];
			if (!LeftControllerState.bPlayingHapticEffect)
			{
				LeftControllerState.ForceFeedbackHapticFrequency = Values.LeftSmall;
				LeftControllerState.ForceFeedbackHapticAmplitude = Values.LeftLarge;
				UpdateForceFeedback(ControllerPair, EControllerHand::Left);
			}

			FOculusTouchControllerState& RightControllerState = ControllerPair.TouchControllerStates[(int32)EControllerHand::Right];
			if (!RightControllerState.bPlayingHapticEffect)
			{
				RightControllerState.ForceFeedbackHapticFrequency = Values.RightSmall;
				RightControllerState.ForceFeedbackHapticAmplitude = Values.RightLarge;
				UpdateForceFeedback(ControllerPair, EControllerHand::Right);
			}
		}
	}
}

bool FOculusInput::SupportsForceFeedback(int32 ControllerId)
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerId);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, InPlatformUser, InDeviceId);
	
	for (FOculusControllerPair& ControllerPair : ControllerPairs)
	{
		if (ControllerPair.DeviceId == InDeviceId)
		{
			const FOculusTouchControllerState& ControllerStateLeft = ControllerPair.TouchControllerStates[(int32)EControllerHand::Left];
			const FOculusTouchControllerState& ControllerStateRight = ControllerPair.TouchControllerStates[(int32)EControllerHand::Right];

			if (!(ControllerStateLeft.bIsConnected || ControllerStateRight.bIsConnected))
			{
				// neither hand connected, won't be receiving force feedback
				continue;
			}

			if (IOculusHMDModule::IsAvailable() && FOculusHMDModule::GetPluginWrapper().GetInitialized())
			{
				// available so could receive feedback
				return true;
			}
		}
	}

	// not handling force feedback
	return false;
}

void FOculusInput::UpdateForceFeedback( const FOculusControllerPair& ControllerPair, const EControllerHand Hand )
{
	const FOculusTouchControllerState& ControllerState = ControllerPair.TouchControllerStates[ (int32)Hand ];

	if( ControllerState.bIsConnected && !ControllerState.bPlayingHapticEffect)
	{
		if(IOculusHMDModule::IsAvailable() && FOculusHMDModule::GetPluginWrapper().GetInitialized() && FApp::HasVRFocus())
		{
			ovrpControllerState4 OvrpControllerState;

			if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerState4((ovrpController)(ovrpController_Active | ovrpController_LTrackedRemote | ovrpController_RTrackedRemote), &OvrpControllerState)) &&
				(OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch | ovrpController_LTrackedRemote | ovrpController_RTrackedRemote)))
			{
				float FreqMin, FreqMax = 0.f;
				GetHapticFrequencyRange(FreqMin, FreqMax);

				// Map the [0.0 - 1.0] range to a useful range of frequencies for the Oculus controllers
				const float ActualFrequency = FMath::Lerp(FreqMin, FreqMax, FMath::Clamp(ControllerState.ForceFeedbackHapticFrequency, 0.0f, 1.0f));

				// Oculus SDK wants amplitude values between 0.0 and 1.0
				const float ActualAmplitude = ControllerState.ForceFeedbackHapticAmplitude * GetHapticAmplitudeScale();

				ovrpController OvrController = ovrpController_None;
				if (OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch))
				{
					OvrController = ( Hand == EControllerHand::Left ) ? ovrpController_LTouch : ovrpController_RTouch;
				}
				else if (OvrpControllerState.ConnectedControllerTypes & (ovrpController_LTrackedRemote | ovrpController_RTrackedRemote))
				{
					OvrController = ( Hand == EControllerHand::Left ) ? ovrpController_LTrackedRemote : ovrpController_RTrackedRemote;
				}

				static float LastAmplitudeSent = -1;
				if (ActualAmplitude != LastAmplitudeSent)
				{
					FOculusHMDModule::GetPluginWrapper().SetControllerVibration2(OvrController, ActualFrequency, ActualAmplitude);
					LastAmplitudeSent = ActualAmplitude;
				}
			}
		}
	}
}

bool FOculusInput::OnControllerButtonPressed(const FOculusButtonState& ButtonState, FPlatformUserId UserId, FInputDeviceId DeviceId, bool IsRepeat)
{
	bool result = MessageHandler->OnControllerButtonPressed(ButtonState.Key, UserId, DeviceId, IsRepeat);

	if (!ButtonState.EmulatedKey.IsNone())
	{
		MessageHandler->OnControllerButtonPressed(ButtonState.EmulatedKey, UserId, DeviceId, IsRepeat);
	}

	return result;
}

bool FOculusInput::OnControllerButtonReleased(const FOculusButtonState& ButtonState, FPlatformUserId UserId, FInputDeviceId DeviceId, bool IsRepeat)
{
	bool result = MessageHandler->OnControllerButtonReleased(ButtonState.Key, UserId, DeviceId, IsRepeat);

	if (!ButtonState.EmulatedKey.IsNone())
	{
		MessageHandler->OnControllerButtonReleased(ButtonState.EmulatedKey, UserId, DeviceId, IsRepeat);
	}

	return result;
}

FName FOculusInput::GetMotionControllerDeviceTypeName() const
{
	const static FName DefaultName(TEXT("OculusInputDevice"));
	return DefaultName;
}

bool FOculusInput::GetControllerOrientationAndPosition( const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	// Don't do renderthread pose update if MRC is active due to controller jitter issues with SceneCaptures
	if (IsInGameThread() || !UDEPRECATED_UOculusMRFunctionLibrary::IsMrcActive())
	{
		for (const FOculusControllerPair& ControllerPair : ControllerPairs)
		{
			if (ControllerPair.DeviceId == InDeviceId)
			{
				if ((DeviceHand == EControllerHand::Left) || (DeviceHand == EControllerHand::Right))
				{
					if (IOculusHMDModule::IsAvailable() && FOculusHMDModule::GetPluginWrapper().GetInitialized())
					{
						OculusHMD::FOculusHMD* OculusHMD = static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem->GetHMDDevice());
						ovrpNode Node = DeviceHand == EControllerHand::Left ? ovrpNode_HandLeft : ovrpNode_HandRight;

						ovrpBool bResult = true;
						bool bIsPositionValid = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodePositionValid(Node, &bResult)) && bResult;
						bool bIsOrientationValid = OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodeOrientationValid(Node, &bResult)) && bResult;

						if (bIsPositionValid || bIsOrientationValid)
						{
							OculusHMD::FSettings* Settings;
							OculusHMD::FGameFrame* CurrentFrame;

							if (IsInGameThread())
							{
								Settings = OculusHMD->GetSettings();
								CurrentFrame = OculusHMD->GetNextFrameToRender();
							}
							else
							{
								Settings = OculusHMD->GetSettings_RenderThread();
								CurrentFrame = OculusHMD->GetFrame_RenderThread();
							}

							if (Settings)
							{
								ovrpPoseStatef InPoseState;
								OculusHMD::FPose OutPose;

								if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetNodePoseState3(ovrpStep_Render, CurrentFrame ? CurrentFrame->FrameNumber : OVRP_CURRENT_FRAMEINDEX, Node, &InPoseState)) &&
									OculusHMD->ConvertPose_Internal(InPoseState.Pose, OutPose, Settings, WorldToMetersScale))
								{
									if (bIsPositionValid)
									{
										OutPosition = OutPose.Position;
									}

									if (bIsOrientationValid)
									{
										OutOrientation = OutPose.Orientation.Rotator();
									}

									auto bSuccess = true;
									UDEPRECATED_UOculusInputFunctionLibrary::HandMovementFilter.Broadcast(
										DeviceHand,
										&OutPosition,
										&OutOrientation,
										&bSuccess);

									return bSuccess;
								}
							}
						}
					}
				}

				break;
			}
		}
	}

	auto bSuccess = false;
	UDEPRECATED_UOculusInputFunctionLibrary::HandMovementFilter.Broadcast(
		DeviceHand,
		&OutPosition,
		&OutOrientation,
		&bSuccess);
	return bSuccess;
}

ETrackingStatus FOculusInput::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	if (DeviceHand != EControllerHand::Left && DeviceHand != EControllerHand::Right)
	{
		return TrackingStatus;
	}

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	for( const FOculusControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.DeviceId == InDeviceId )
		{
			const FOculusTouchControllerState& ControllerState = ControllerPair.TouchControllerStates[ (int32)DeviceHand ];
			if (ControllerState.bIsConnected)
			{
				if (ControllerState.bIsPositionTracked && ControllerState.bIsOrientationTracked)
				{
					TrackingStatus = ETrackingStatus::Tracked;
				}
				else if (ControllerState.bIsPositionValid && ControllerState.bIsOrientationValid)
				{
					TrackingStatus = ETrackingStatus::InertialOnly;
				}

				break;
			}

			const FOculusHandControllerState& HandState = ControllerPair.HandControllerStates[(int32)DeviceHand];
			if (HandState.bIsConnected)
			{
				if (HandState.bIsPositionTracked && HandState.bIsOrientationTracked)
				{
					TrackingStatus = ETrackingStatus::Tracked;
				}

				break;
			}
		}
	}

	return TrackingStatus;
}

void FOculusInput::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerId);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerId, InPlatformUser, InDeviceId);
	
	for (FOculusControllerPair& ControllerPair : ControllerPairs)
	{
		if (ControllerPair.DeviceId == InDeviceId)
		{
			FOculusTouchControllerState& ControllerState = ControllerPair.TouchControllerStates[Hand];
			if (ControllerState.bIsConnected)
			{
				if(IOculusHMDModule::IsAvailable() && FOculusHMDModule::GetPluginWrapper().GetInitialized() && FApp::HasVRFocus())
				{
					static bool pulledHapticsDesc = false;
					if (!pulledHapticsDesc)
					{
						// Buffered haptics is currently only supported on Touch
						FOculusHMDModule::GetPluginWrapper().GetControllerHapticsDesc2(ovrpController_RTouch, &OvrpHapticsDesc);
						pulledHapticsDesc = true;
					}

					ovrpControllerState4 OvrpControllerState;

					ovrpController ControllerTypes = (ovrpController)(ovrpController_Active | ovrpController_LTrackedRemote | ovrpController_RTrackedRemote);

#ifdef USE_ANDROID_INPUT
					ControllerTypes = (ovrpController)(ControllerTypes | ovrpController_Touch);
#endif

					if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerState4(ControllerTypes, &OvrpControllerState)) &&
						(OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch | ovrpController_LTrackedRemote | ovrpController_RTrackedRemote)))
					{
						// Buffered haptics is currently only supported on Touch
						if (Values.HapticBuffer)
						{
							ControllerState.ResampleHapticBufferData(*Values.HapticBuffer, ResampledRawDataCache);
						}
						FHapticFeedbackBuffer* HapticBuffer = &ControllerState.ResampledHapticBuffer;
						if ( (OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch)) &&
							Values.HapticBuffer && HapticBuffer->SamplingRate == OvrpHapticsDesc.SampleRateHz)
						{
							const ovrpController OvrpController = (EControllerHand(Hand) == EControllerHand::Left) ? ovrpController_LTouch : ovrpController_RTouch;

							ovrpHapticsState OvrpHapticsState;
							if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetControllerHapticsState2(OvrpController, &OvrpHapticsState)))
							{
								float appFrameRate = 90.f;
								FOculusHMDModule::GetPluginWrapper().GetAppFramerate2(&appFrameRate);

								int wanttosend = (int)ceil((float)OvrpHapticsDesc.SampleRateHz / appFrameRate) + 1;
								wanttosend = FMath::Min(wanttosend, OvrpHapticsDesc.MaximumBufferSamplesCount);
								wanttosend = FMath::Max(wanttosend, OvrpHapticsDesc.MinimumBufferSamplesCount);

								if (OvrpHapticsState.SamplesQueued < OvrpHapticsDesc.MinimumSafeSamplesQueued + wanttosend) //trying to minimize latency
								{
									wanttosend = (OvrpHapticsDesc.MinimumSafeSamplesQueued + wanttosend - OvrpHapticsState.SamplesQueued);
									void *bufferToFree = NULL;
									ovrpHapticsBuffer OvrpHapticsBuffer;
									OvrpHapticsBuffer.SamplesCount = FMath::Min(wanttosend, HapticBuffer->BufferLength - HapticBuffer->SamplesSent);

									if (OvrpHapticsBuffer.SamplesCount == 0 && OvrpHapticsState.SamplesQueued == 0)
									{
										Values.HapticBuffer->bFinishedPlaying = HapticBuffer->bFinishedPlaying = true;

										ControllerState.bPlayingHapticEffect = false;
									}
									else
									{
										if (OvrpHapticsDesc.SampleSizeInBytes == 1)
										{
											uint8* samples = (uint8*)FMemory::Malloc(OvrpHapticsBuffer.SamplesCount * sizeof(*samples));
											for (int i = 0; i < OvrpHapticsBuffer.SamplesCount; i++)
											{
												samples[i] = static_cast<uint8>(HapticBuffer->RawData[HapticBuffer->CurrentPtr + i] * HapticBuffer->ScaleFactor);
											}
											OvrpHapticsBuffer.Samples = bufferToFree = samples;
										}
										else if (OvrpHapticsDesc.SampleSizeInBytes == 2)
										{
											uint16* samples = (uint16*)FMemory::Malloc(OvrpHapticsBuffer.SamplesCount * sizeof(*samples));
											for (int i = 0; i < OvrpHapticsBuffer.SamplesCount; i++)
											{
												const uint32 DataIndex = HapticBuffer->CurrentPtr + (i * 2);
												const uint16* const RawData = reinterpret_cast<const uint16*>(&HapticBuffer->RawData[DataIndex]);
												samples[i] = static_cast<uint16>(*RawData * HapticBuffer->ScaleFactor);
											}
											OvrpHapticsBuffer.Samples = bufferToFree = samples;
										}
										else if (OvrpHapticsDesc.SampleSizeInBytes == 4)
										{
											uint32* samples = (uint32*)FMemory::Malloc(OvrpHapticsBuffer.SamplesCount * sizeof(*samples));
											for (int i = 0; i < OvrpHapticsBuffer.SamplesCount; i++)
											{
												const uint32 DataIndex = HapticBuffer->CurrentPtr + (i * 4);
												const uint32* const RawData = reinterpret_cast<const uint32*>(&HapticBuffer->RawData[DataIndex]);
												samples[i] = static_cast<uint32>(*RawData * HapticBuffer->ScaleFactor);
											}
											OvrpHapticsBuffer.Samples = bufferToFree = samples;
										}

										FOculusHMDModule::GetPluginWrapper().SetControllerHaptics2(OvrpController, OvrpHapticsBuffer);

										if (bufferToFree)
										{
											FMemory::Free(bufferToFree);
										}

										HapticBuffer->CurrentPtr += (OvrpHapticsBuffer.SamplesCount * OvrpHapticsDesc.SampleSizeInBytes);
										HapticBuffer->SamplesSent += OvrpHapticsBuffer.SamplesCount;

										ControllerState.bPlayingHapticEffect = true;
									}
								}
							}
						}
						else
						{
							// Buffered haptics is currently only supported on Touch
							if ( (OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch)) && (HapticBuffer) )
							{
								UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("Haptic Buffer not sampled at the correct frequency : %d vs %d"), OvrpHapticsDesc.SampleRateHz, HapticBuffer->SamplingRate);
							}
							float FreqMin, FreqMax = 0.f;
							GetHapticFrequencyRange(FreqMin, FreqMax);

							const float InitialFreq = (Values.Frequency > 0.0f) ? Values.Frequency : 1.0f;
							const float Frequency = FMath::Lerp(FreqMin, FreqMax, FMath::Clamp(InitialFreq, 0.f, 1.f));

							const float Amplitude = Values.Amplitude * GetHapticAmplitudeScale();

							if (ControllerState.HapticAmplitude != Amplitude || ControllerState.HapticFrequency != Frequency)
							{
								ControllerState.HapticAmplitude = Amplitude;
								ControllerState.HapticFrequency = Frequency;

								ovrpController OvrController = ovrpController_None;
								if (OvrpControllerState.ConnectedControllerTypes & (ovrpController_Touch))
								{
									OvrController = (EControllerHand(Hand) == EControllerHand::Left) ? ovrpController_LTouch : ovrpController_RTouch;
								}
								else if (OvrpControllerState.ConnectedControllerTypes & (ovrpController_LTrackedRemote | ovrpController_RTrackedRemote))
								{
									OvrController = (EControllerHand(Hand) == EControllerHand::Left) ? ovrpController_LTrackedRemote : ovrpController_RTrackedRemote;
								}

								FOculusHMDModule::GetPluginWrapper().SetControllerVibration2(OvrController, Frequency, Amplitude);

								ControllerState.bPlayingHapticEffect = (Amplitude != 0.f) && (Frequency != 0.f);
							}
						}
					}
				}
			}

			break;
		}
	}
}

void FOculusTouchControllerState::ResampleHapticBufferData(const FHapticFeedbackBuffer& HapticBuffer, TMap<const uint8*, TSharedPtr<TArray<uint8>>>& ResampledRawDataCache)
{
	const uint8* OriginalRawData = HapticBuffer.RawData;
	TSharedPtr<TArray<uint8>>* ResampledRawDataSharedPtrPtr = ResampledRawDataCache.Find(OriginalRawData);
	if (ResampledRawDataSharedPtrPtr == nullptr)
	{
		// We need to resample and cache the resampled data.

		ResampledHapticBuffer = HapticBuffer;

		int32 SampleRate = HapticBuffer.SamplingRate;
		int TargetFrequency = 320;
		int TargetBufferSize = (HapticBuffer.BufferLength * TargetFrequency) / (SampleRate * 2) + 1; //2 because we're only using half of the 16bit source PCM buffer
		ResampledHapticBuffer.BufferLength = TargetBufferSize;
		ResampledHapticBuffer.CurrentPtr = 0;
		ResampledHapticBuffer.SamplingRate = TargetFrequency;

		TSharedPtr<TArray<uint8>>& NewResampledRawDataSharedPtr = ResampledRawDataCache.Add(OriginalRawData);
		NewResampledRawDataSharedPtr = MakeShared<TArray<uint8>>();
		ResampledRawDataSharedPtrPtr = &NewResampledRawDataSharedPtr;
		TArray<uint8>& ResampledRawData = *NewResampledRawDataSharedPtr;
		ResampledRawData.SetNum(TargetBufferSize);

		const uint8* PCMData = HapticBuffer.RawData;

		int previousTargetIndex = -1;
		int currentMin = 0;
		for (int i = 1; i < HapticBuffer.BufferLength; i += 2)
		{
			int targetIndex = i * TargetFrequency / (SampleRate * 2);
			int val = PCMData[i];
			if (val & 0x80)
			{
				val = ~val;
			}
			currentMin = FMath::Min(currentMin, val);

			if (targetIndex != previousTargetIndex)
			{

				ResampledRawData[targetIndex] = val * 2;// *Scale;
				previousTargetIndex = targetIndex;
				currentMin = 0;
			}
		}

		ResampledHapticBuffer.RawData = ResampledRawData.GetData();
	}
	else if (ResampledHapticBuffer.RawData != (*ResampledRawDataSharedPtrPtr)->GetData())
	{
		// If this a cached effect, but not the same one we played last so we need to copy the new one's buffer and reference its cached resampled data.
		ResampledHapticBuffer = HapticBuffer;
		ResampledHapticBuffer.RawData = (*ResampledRawDataSharedPtrPtr)->GetData();
	}
}

void FOculusInput::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = 0.f;
	MaxFrequency = 1.f;
}

float FOculusInput::GetHapticAmplitudeScale() const
{
	return 1.f;
}

uint32 FOculusInput::GetNumberOfTouchControllers() const
{
	uint32 RetVal = 0;

	for (FOculusControllerPair Pair : ControllerPairs)
	{
		RetVal += (Pair.TouchControllerStates[0].bIsConnected ? 1 : 0);
		RetVal += (Pair.TouchControllerStates[1].bIsConnected ? 1 : 0);
	}

	return RetVal;
}

uint32 FOculusInput::GetNumberOfHandControllers() const
{
	uint32 RetVal = 0;

	for (FOculusControllerPair Pair : ControllerPairs)
	{
		RetVal += (Pair.HandControllerStates[0].bIsConnected ? 1 : 0);
		RetVal += (Pair.HandControllerStates[1].bIsConnected ? 1 : 0);
	}

	return RetVal;
}

} // namespace OculusInput

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
#endif	 // OCULUS_INPUT_SUPPORTED_PLATFORMS
