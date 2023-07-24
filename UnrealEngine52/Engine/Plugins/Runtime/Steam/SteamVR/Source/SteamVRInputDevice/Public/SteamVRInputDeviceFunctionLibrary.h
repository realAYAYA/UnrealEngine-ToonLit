/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "openvr.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Features/IModularFeatures.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "IMotionController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SteamVRInputDeviceFunctionLibrary.generated.h"

using namespace vr;

#define ACTION_PATH_VIBRATE_LEFT		"/actions/main/out/vibrateleft"
#define ACTION_PATH_VIBRATE_RIGHT		"/actions/main/out/vibrateright"
#define MAX_BINDINGINFO_COUNT	5

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/** UE4 Bone definition of the SteamVR Skeleton */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRSkeletonTransform
{
	GENERATED_BODY()

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Root;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Wrist;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Thumb_0;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Thumb_1;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Thumb_2;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Thumb_3;
	
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Index_0;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Index_1;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Index_2;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Index_3;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Index_4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Middle_0;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Middle_1;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Middle_2;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Middle_3;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Middle_4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Ring_0;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Ring_1;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Ring_2;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Ring_3;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Ring_4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Pinky_0;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Pinky_1;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Pinky_2;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Pinky_3;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Pinky_4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Aux_Thumb;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Aux_Index;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Aux_Middle;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Aux_Ring;
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FTransform Aux_Pinky;
};

/** SteamVR finger curls */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRFingerCurls
{
	GENERATED_BODY()

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Thumb = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Index = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Middle = 0.0f;
	
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Ring = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Pinky = 0.0f;
};

/** SteamVR finger splays */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRFingerSplays
{
	GENERATED_BODY()

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Thumb_Index = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Index_Middle = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Middle_Ring = 0.0f;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float	Ring_Pinky = 0.0f;
};

/** SteamVR actions as defined by the developer */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRAction
{
	GENERATED_BODY()

	// The SteamVR name of the action (e.g. Teleport, OpenConsole)
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FName		Name;

	// The path defined for the action (e.g. main/in/{ActionName})
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FString		Path;

	VRActionHandle_t Handle;				// The handle to the SteamVR Action 
	VRInputValueHandle_t ActiveOrigin;		// The input value handle of the origin of the latest input event

	FSteamVRAction(const FName ActionName, const FString ActionPath, const VRActionHandle_t ActionHandle, const VRInputValueHandle_t ActionOrigin)
		: Name(ActionName)
		, Path(ActionPath)
		, Handle(ActionHandle)
		, ActiveOrigin(ActionOrigin)
	{}

	FSteamVRAction()
	{}
};


/** SteamVR action set as defined by the developer */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRActionSet
{
	GENERATED_BODY()

	// The path defined for this action set (e.g. /actions/main)
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FString		Path;			
	
	VRActionSetHandle_t Handle;	// The handle to the SteamVR Action Set

	FSteamVRActionSet(const FString ActionSetPath, const VRActionHandle_t ActionSetHandle)
		: Path(ActionSetPath)
		, Handle(ActionSetHandle)
	{}

	FSteamVRActionSet()
	{}
};

/** Information about the tracked device associated from the input source */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRInputOriginInfo
{
	GENERATED_BODY()

	// The tracked device index for the device or k_unTrackedDeviceInvalid (0xFFFFFFFF)
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	int32	TrackedDeviceIndex;
	
	//  The name of the component of the tracked device's render model that represents this input source, or an empty string if there is no associated render model component.
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FString	RenderModelComponentName;

	//  The tracked device's model info
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FString	TrackedDeviceModel;

	FSteamVRInputOriginInfo(const int32 InDeviceIndex, const FString InRenderModelComponentName)
		: TrackedDeviceIndex(InDeviceIndex)
		, RenderModelComponentName(InRenderModelComponentName)
	{}

	FSteamVRInputOriginInfo()
	{
		TrackedDeviceIndex = 0;
	}
};

/** Retargetting information for the SteamVR skeleton to UE4 stock skeleton */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FUE4RetargettingRefs
{
	GENERATED_BODY()

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	bool bIsInitialized;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	bool bIsRightHanded;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FVector KnuckleAverageMS_UE4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FVector WristSideDirectionLS_UE4;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FVector WristForwardLS_UE4;

	FUE4RetargettingRefs()
	{
		bIsInitialized = false;
		bIsRightHanded = false;
		KnuckleAverageMS_UE4 = FVector::ZeroVector;
		WristSideDirectionLS_UE4 = FVector::RightVector;
		WristForwardLS_UE4 = FVector::ForwardVector;
	}
};

/** Information about the input bindings for an action on currently active controller (i.e device path, input path, mode, slot)  */
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FSteamVRInputBindingInfo
{
	GENERATED_BODY()

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FName	DevicePathName;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FName	InputPathName;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FName	ModeName;

	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FName	SlotName;

	FSteamVRInputBindingInfo()
	{
		DevicePathName = NAME_None;
		InputPathName = NAME_None;
		ModeName = NAME_None;
		SlotName = NAME_None;
	}

	FSteamVRInputBindingInfo(FName InDevicePathName, FName InInputPathName, FName InModeName, FName InSlotName) 
		: DevicePathName(InDevicePathName)
		, InputPathName(InInputPathName)
		, ModeName(InModeName)
		, SlotName(InSlotName)
	{
	}
};

/** Convenience type for SteamVR Hand designation (Left/Right) */
UENUM(BlueprintType)	
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") ESteamVRHand : uint8
{
	VR_Left		UMETA(DisplayName = "Left"),
	VR_Right	UMETA(DisplayName = "Right")
};

/** Valid range of motion for a skeletal animation */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") EMotionRange : uint8
{
	VR_WithoutController 	UMETA(DisplayName = "Without Controller"),
	VR_WithController 		UMETA(DisplayName = "With Controller")
};

/** Valid values for hands thats used for the Skeletal Input System calls */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") EHand : uint8
{
	VR_LeftHand 	UMETA(DisplayName = "Left Hand"),
	VR_RightHand 	UMETA(DisplayName = "Right Hand")
};

/** Types of known skeletons that this animation node can handle */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") EHandSkeleton : uint8
{
	VR_SteamVRHandSkeleton 	UMETA(DisplayName = "SteamVR Hand Skeleton"),
	VR_UE4HandSkeleton 		UMETA(DisplayName = "UE4 Hand Skeleton")
};

/** Skeletal Tracking Level of a controller */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") EControllerFidelity : uint8
{
	VR_ControllerFidelity_Estimated 	UMETA(DisplayName = "Controller Fidelity Estimated"),
	VR_ControllerFidelity_Full 			UMETA(DisplayName = "Controller Fidelity Full"),
	VR_ControllerFidelity_Partial 		UMETA(DisplayName = "Controller Fidelity Partial")
};

/** Input String Values for querying user hardware */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") ESteamVRInputStringBits : uint8
{
	// Which hand the origin is in (e.g. "Left Hand")
	VR_InputString_Hand					UMETA(DisplayName = "Hand"),

	// What kind of controller the user has in that hand (e.g. "Index Controller")
	VR_InputString_ControllerType		UMETA(DisplayName = "Controller Type"),

	// What part of that controller is the origin (e.g. "Trackpad")
	VR_InputString_InputSource			UMETA(DisplayName = "Input Source"),

	// All of the above. (e.g. "Left Hand Index Controller Trackpad")
	VR_InputString_All					UMETA(DisplayName = "All")
};

/** Input String Values for querying user hardware */
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.") ESkeletalSummaryDataType : uint8
{
	// The data should match the animated transforms in the skeleton transforms. This data will probably be smoothed and may be more latent
	VR_SummaryType_FromAnimation		UMETA(DisplayName = "From Animation"),

	// The data should be the unprocessed values from the device when available. This data may include more jitter but may be provided with less latency
	VR_SummaryType_FromDevice			UMETA(DisplayName = "From Device"),
};

/*
 * SteamVR Input Extended Functions
 * Functions and properties defined here are safe for developer use
 */
UCLASS(deprecated, meta = (DeprecationMessage = "SteamVR plugin is deprecated; please use the OpenXR plugin."))
class STEAMVRINPUTDEVICE_API UDEPRECATED_USteamVRInputDeviceFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Get the finger curl and splay for a give hand in the current frame
	* @param Hand - Which hand to get the finger curls and splay values for
	* @param FingerCurls - Curl values for each finger pair this frame
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetFingerCurlsAndSplays(EHand Hand, FSteamVRFingerCurls& FingerCurls, FSteamVRFingerSplays& FingerSplays, ESkeletalSummaryDataType SummaryDataType = ESkeletalSummaryDataType::VR_SummaryType_FromAnimation);

	/**
	* Generate haptic feedback in the requested controller
	* @param Hand - Which hand to send the controller feedback to
	* @param StartSecondsFromNow - When to start the haptic feedback
	* @param DurationSeconds - How long to have the haptic feedback active
	* @param Frequency - Frequency used in the haptic feedback
	* @param Amplitude - Amplitude used in the haptic feedback
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category="SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void PlaySteamVR_HapticFeedback(ESteamVRHand Hand, float StartSecondsFromNow, float DurationSeconds = 1.f,
			float Frequency = 1.f, float Amplitude = 0.5f);

	/**
	* Check Whether or not Curls and Splay values are being retrieved per frame from the SteamVR Input System
	* @return LeftHandState - Whether or not curls and splay values are being retrieved from the left hand
	* @return RightHandState -  Whether or not curls and splay values are being retrieved from the right hand
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category="SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetCurlsAndSplaysState(bool& LeftHandState, bool& RightHandState);

	/**
	* Check Whether or not controllers attached to either hand have Skeletal Input support
	* @return LeftHandState - Whether or not the controller attached to the player's left hand have skeletal input support
	* @return RightHandState -  Whether or not the controller attached to the player's right hand have skeletal input support
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetSkeletalState(bool& LeftHandState, bool& RightHandState);

	/** 
	* Retrieve skeletal tracking level for all controllers 
	* @return LeftControllerFidelity - The skeletal tracking level of the left controller
	* @return RightControllerFidelity -  The skeletal tracking level of the right controller
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetControllerFidelity(EControllerFidelity& LeftControllerFidelity, EControllerFidelity& RightControllerFidelity);

	/**
	* Tell SteamVR Whether or not to retrieve Curls and Splay values per frame
	* @param NewLeftHandState - Whether or not curls and splay values will be retrieved for the left hand
	* @param NewRightHandState -  Whether or not curls and splay values will be retrieved for the right hand
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void SetCurlsAndSplaysState(bool NewLeftHandState, bool NewRightHandState);

	/**
	* Check whether we are using a skeleton pose or the raw controller pose for the orientation and position of the motion controller
	* @param bUseSkeletonPose - Whether or not we are using the skeleton pose instead of the controller raw pose
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetPoseSource(bool& bUsingSkeletonPose);

	/**
	* Set whether to use skeleton pose or the raw controller pose for the orientation and position of the motion controller
	* @param bUseSkeletonPose - Whether or not to use the skeleton pose instead of the controller raw pose
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void SetPoseSource(bool bUseSkeletonPose);

	/**
	* Retrieve the live skeletal input bone values from SteamVR
	* @return LeftHand - Per bone transform values for the left hand skeleton
	* @return RightHand - Per bone transform values for the right hand skeleton
	* @param bWithController - Whether or not retrieve skeletal input values with controller
	* @param bXAxisForward - Whether or not the Skeleton has the X axis facing forward
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetSkeletalTransform(FSteamVRSkeletonTransform& LeftHand, FSteamVRSkeletonTransform& RightHand, bool bWithController=false);
	
	/**
	* Retrieve the left hand pose information - position, orientation and velocities
	* @return Position - Translation from the pose data matrix in UE coordinates
	* @return Orientation - Orientation derived from the pose data matrix in UE coordinates
	* @return AngularVelocity - The angular velocity of the hand this frame
	* @return Velocity - The velocity of the hand this frame
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetLeftHandPoseData(FVector& Position, FRotator& Orientation, FVector& AngularVelocity, FVector& Velocity);

	/**
	* Retrieve the right hand pose information - position, orientation and velocities
	* @return Position - Translation from the pose data matrix in UE coordinates
	* @return Orientation - Orientation derived from the pose data matrix in UE coordinates
	* @return AngularVelocity - The angular velocity of the hand this frame
	* @return Velocity - The velocity of the hand this frame
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetRightHandPoseData(FVector& Position, FRotator& Orientation, FVector& AngularVelocity, FVector& Velocity);

	/**
	* Retrieve the input actions for this project
	* @return SteamVRActions - Input actions defined in this project
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetSteamVR_ActionArray(TArray<FSteamVRAction>& SteamVRActions);

	/**
	* Search for a valid action matching the given action name and action set
	* @param ActionName - The name of the action to look for (e.g. TeleportLeft)
	* @param ActionSet - The name of the action set that the action belongs to (e.g. main). Default is "main"
	* @return bresult - The result of the search
	* @return FoundAction - The action if found
	* @return FoundActionSet - The action set if found
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void FindSteamVR_Action(FName ActionName, bool& bResult, FSteamVRAction& FoundAction, FSteamVRActionSet& FoundActionSet, FName ActionSet = FName("main"));

	/**
	* Retrieve the input action sets for this project
	* @return SteamVRActionSets - Input action sets defined in this project
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetSteamVR_ActionSetArray(TArray<FSteamVRActionSet>& SteamVRActionSets);

	/**
	* Returns information about the tracked device associated from the input source.
	* @param SteamVRAction - The action that's the source of the input
	* @return InputOriginInfo - The origin info of the action
	* @return bool - whether the operation is successful or not
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static bool GetSteamVR_OriginTrackedDeviceInfo(FSteamVRAction SteamVRAction, FSteamVRInputOriginInfo& InputOriginInfo);

	/**
	* Find and return information about the tracked device associated from the input source.
	* @param SteamVRAction - The action that's the source of the input
	* @return InputOriginInfo - The origin info of the action
	* @return bool - whether the operation is successful or not
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void FindSteamVR_OriginTrackedDeviceInfo(FName ActionName, bool& bResult, FSteamVRInputOriginInfo& InputOriginInfo, FName ActionSet = FName("main"));

	/**
	* Retrieve the localized name of the origin of a given action (e.g. "Left Hand Index Controller Trackpad")
	* @param SteamVRAction - The action that we will lookup the last active origin for
	* @param LocalizedParts - Bitfields to specify which origin parts to return
	* @return OriginLocalizedName -  The localized name of the origin of a given action (e.g. "Left Hand Index Controller Trackpad")
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void GetSteamVR_OriginLocalizedName(FSteamVRAction SteamVRAction, TArray<ESteamVRInputStringBits> LocalizedParts, FString& OriginLocalizedName);

	/**
	* Show the current binding of a given action in the user's HMD
	* @param SteamVRAction - The action that we will lookup the current binding for
	* @param SteamVRActionSet - The action set that the action belongs to
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void ShowSteamVR_ActionOrigin(FSteamVRAction SteamVRAction, FSteamVRActionSet SteamVRActionSet);

	/**
	* Search and show the current binding of a provided action name and action set in the user's HMD
	* @param SteamVRAction - The action that we will lookup the current binding for
	* @param SteamVRActionSet - The action set that the action belongs to. Defaults to "main"
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static bool FindSteamVR_ActionOrigin(FName ActionName, FName ActionSet = FName("main"));

	/**
	* Returns the data for the hand transform at any point in time from current time, given a relative number of seconds
	* @param Hand - The hand that we're going to retrieve the transform for
	* @return Position - The position of the hand at the point in time, given a relative number of seconds, from the current time
	* @return Orientation - The rotation of the hand at the point in time, given a relative number of seconds, from the current time
	* @return bool - Whether or not the call was succesful
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static bool GetSteamVR_HandPoseRelativeToNow(FVector& Position, FRotator& Orientation, ESteamVRHand Hand = ESteamVRHand::VR_Left, float PredictedSecondsFromNow = 0.f);

	/**
	* Returns the the current value of the global PredictedSecondsFromNow use in any Get Pose Action Data calls (i.e. Getting controller transform)
	* A value of -9999.f triggers a GetPoseActionDataForNextFrame, otherwise GetPoseActionRelativeToNow is called with this value
	* @return float - The current Predicted Seconds From Now from the SteamVRInput device
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static float GetSteamVR_GlobalPredictedSecondsFromNow();

	/**
	* Sets the the current value of the global PredictedSecondsFromNow to use in any Get Pose Action Data calls (i.e. Getting controller transform)
	* A value of -9999.f will trigger a GetPoseActionDataForNextFrame, otherwise GetPoseActionRelativeToNow will be called with this value
	* @param NewValue - The value for PredictedSecondsFromNow that will be used by the SteamVRInput device for Get Action Pose Data calls 
	* @return float - The current Predicted Seconds From Now from the SteamVRInput device
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static float SetSteamVR_GlobalPredictedSecondsFromNow(float NewValue);

	/**
	* Shows all current bindings for the current controller in the user's headset
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static void ShowAllSteamVR_ActionOrigins();

	/**
	* Retrieves useful information about the SteamVR input bindings for an action.
	* @param SteamVRActionHandle - The action handle of the action that binding info will be retrieved for the currently active controller. Use Find SteamVRAction node to get a handle
	* @return SteamVRInputBindingInfo - Array of binding info for an action with the currently active controller
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static TArray<FSteamVRInputBindingInfo> GetSteamVR_InputBindingInfo(FSteamVRAction SteamVRActionHandle);

	/**
	* Retrieves useful information about the SteamVR input bindings with a given action name and action set.
	* @param ActionName - The name of the action that binding info will be retrieved for the currently active controller
	* @param ActionSet - The name of the action set that the action belongs in
	* @return SteamVRInputBindingInfo - Array of binding info for an action with the currently active controller
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static TArray<FSteamVRInputBindingInfo> FindSteamVR_InputBindingInfo(FName ActionName, FName ActionSet = FName("main"));

	/**
	* Sets the zero pose for the seated tracker coordinate system to the current position and yaw of the HMD. 
	* After this call, calls that pass TrackingUniverseSeated as the origin will be relative to this new zero pose.
	*
	* NOTE: This function overrides the user's previously saved seated zero pose and should only be called as the result of a user action.
	* Users are also able to set their seated zero pose via the SteamVR Dashboard.
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static bool ResetSeatedPosition();

	/**
	* Returns the user's HMD's current IPD (interpupillary distance) setting in millimetres.
	* @return float - The current IPD setting of the user's headset in millimetres
	*/
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	static float GetUserIPD();

	/**
	* Get the SteamVR Bone Transform value in UE coordinates
	* @param SteamBoneTransform - The SteamVR Bone Transform value to get the UE coordinates for
	*/
	static FTransform GetUETransform(VRBoneTransform_t SteamBoneTransform);

	/** Regenerate the action manifest used by SteamVR Input System */
	static void RegenActionManifest();
	
	/** Regenerate Controller Bindings for supported SteamVR Controller types */
	static void RegenControllerBindings();
	
	/** Live reload the action manifest and register it to the SteamVR Input System */
	static void ReloadActionManifest();
	
	/** Open the SteamVR Controller Input Dashboard in the user#s default browser */
	static void LaunchBindingsURL();

private:
	/** Retrieve the first available SteamVR Input device currently active in a game */
	static class FSteamVRInputDevice* GetSteamVRInputDevice();
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
