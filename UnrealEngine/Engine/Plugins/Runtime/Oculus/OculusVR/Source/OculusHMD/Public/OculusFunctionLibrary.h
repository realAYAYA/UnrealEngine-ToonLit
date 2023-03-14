// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IOculusHMDModule.h"
#include "OculusFunctionLibrary.generated.h"

namespace OculusHMD
{
	class FOculusHMD;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/* Tracked device types corresponding to ovrTrackedDeviceType enum*/
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") ETrackedDeviceType : uint8
{
	None UMETA(DisplayName = "No Devices"),
	HMD	UMETA(DisplayName = "HMD"),
	LTouch	UMETA(DisplayName = "Left Hand"),
	RTouch	UMETA(DisplayName = "Right Hand"),
	Touch		UMETA(DisplayName = "All Hands"),
    DeviceObjectZero    UMETA(DisplayName = "DeviceObject Zero"),
	All	UMETA(DisplayName = "All Devices")
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data Field", Deprecated = "5.1"))
struct FHmdUserProfileField
{
	GENERATED_USTRUCT_BODY()

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	FString FieldName;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	FString FieldValue;

	FHmdUserProfileField() {}
	FHmdUserProfileField(const FString& Name, const FString& Value) :
		FieldName(Name), FieldValue(Value) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data", Deprecated = "5.1"))
struct FHmdUserProfile
{
	GENERATED_USTRUCT_BODY()

	/** Name of the user's profile. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	FString Name;

	/** Gender of the user ("male", "female", etc). */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	FString Gender;

	/** Height of the player, in meters */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	float PlayerHeight;

	/** Height of the player, in meters */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	float EyeHeight;

	/** Interpupillary distance of the player, in meters */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	float IPD;

	/** Neck-to-eye distance, in meters. X - horizontal, Y - vertical. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	FVector2D NeckToEyeDistance;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay", meta = (DeprecatedProperty))
	TArray<FHmdUserProfileField> ExtraFields;

	FHmdUserProfile() :
		PlayerHeight(0.f), EyeHeight(0.f), IPD(0.f), NeckToEyeDistance(FVector2D::ZeroVector) {}
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EFixedFoveatedRenderingLevel : uint8
{
	FFR_Off = 0,
	FFR_Low = 1,
	FFR_Medium = 2,
	FFR_High = 3,
	// High foveation setting with more detail toward the bottom of the view and more foveation near the top
	FFR_HighTop = 4
};

/* Guardian boundary types*/
UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EBoundaryType : uint8
{
	Boundary_Outer	UMETA(DisplayName = "Outer Boundary"),
	Boundary_PlayArea	UMETA(DisplayName = "Play Area"),
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusColorSpace : uint8
{
	/// The default value from GetHmdColorSpace until SetClientColorDesc is called. Only valid on PC, and will be remapped to Quest on Mobile
	Unknown = 0,
	/// No color correction, not recommended for production use. See documentation for more info
	Unmanaged = 1,
	/// Preferred color space for standardized color across all Oculus HMDs with D65 white point
	Rec_2020 = 2,
	/// Rec. 709 is used on Oculus Go and shares the same primary color coordinates as sRGB
	Rec_709 = 3,
	/// Oculus Rift CV1 uses a unique color space, see documentation for more info
	Rift_CV1 = 4,
	/// Oculus Rift S uses a unique color space, see documentation for more info
	Rift_S = 5,
	/// Oculus Quest's native color space is slightly different than Rift CV1
	Quest = 6,
	/// Similar to DCI-P3. See documentation for more details on P3
	P3 = 7,
	/// Similar to sRGB but with deeper greens using D65 white point
	Adobe_RGB = 8,
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EHandTrackingSupport : uint8
{
	ControllersOnly,
	ControllersAndHands,
	HandsOnly,
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EHandTrackingFrequency : uint8
{
	LOW,
	HIGH,
	MAX,
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusDeviceType : uint8
{
	//mobile HMDs 
	OculusMobile_Deprecated0 = 0,
	OculusQuest,
	OculusQuest2,
	//OculusMobile_Placeholder10,

	//PC HMDs
	Rift = 100,
	Rift_S,
	Quest_Link,
	Quest2_Link,
	//OculusPC_Placeholder4103,

	//default
	OculusUnknown = 200,
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusXrApi : uint8
{
	LegacyOVRPlugin = 0 UMETA(DisplayName = "Legacy Oculus SDK (no longer developed by Epic)", ToolTip = "Legacy Oculus SDK. Epic is no longer developing for this SDK, and it should only be used if there are features required for a project that are not yet supported through OpenXR, but it's hard to guarantee potential bugs will be fixed as it's not in active development at Epic. Epic recommends Native OpenXR instead, as that will be the main development focus going forward."),
	
	OVRPluginOpenXR = 1 UMETA(DisplayName = "Legacy Oculus SDK + OpenXR (temporary experimental solution)", ToolTip = "Legacy Oculus SDK using an OpenXR backend. Experimental. Epic recommends Native OpenXR instead, as that will be the main development focus going forward."),
	
	NativeOpenXR = 2 UMETA(DisplayName = "Native OpenXR with Oculus vendor extensions (Epic's development focus)", ToolTip = "Disable Legacy Oculus in favor of the native OpenXR implementation, with Oculus vendor extensions. Must enable the OpenXR plugin. This will be where Epic focuses XR development going forward. Oculus OpenXR extensions may be moved into a separate plugin (or plugins) in the future to improve modularity. The features supported by OpenXR are listed in the OpenXR specification on khronos.org, and the features supported by a given runtime can be verified with the \"OpenXR Explorer\" application on GitHub."),
};

/*
* Information about relationships between a triggered boundary (EBoundaryType::Boundary_Outer or
* EBoundaryType::Boundary_PlayArea) and a device or point in the world.
* All dimensions, points, and vectors are returned in Unreal world coordinate space.
*/
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct FGuardianTestResult
{
	GENERATED_BODY()

	/** Is there a triggering interaction between the device/point and specified boundary? */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result", meta = (DeprecatedProperty))
	bool IsTriggering = false;

	/** Device type triggering boundary (ETrackedDeviceType::None if BoundaryTestResult corresponds to a point rather than a device) */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result", meta = (DeprecatedProperty))
	ETrackedDeviceType DeviceType = ETrackedDeviceType::None;

	/** Distance of device/point to surface of boundary specified by BoundaryType */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result", meta = (DeprecatedProperty))
	float ClosestDistance = 0.0f;

	/** Closest point on surface corresponding to specified boundary */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result", meta = (DeprecatedProperty))
	FVector ClosestPoint = FVector(0.0f);

	/** Normal of closest point */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result", meta = (DeprecatedProperty))
	FVector ClosestPointNormal = FVector(0.0f, 0.0f, 1.0f);
};

UCLASS(deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSHMD_API UDEPRECATED_UOculusFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 * @param NeckPosition		(out) The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as DevicePosition.
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale		(in) The 3D scale that will be applied to position.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector);

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	*
	* @param AngularAcceleration	(out) Angular acceleration in radians per second per second.
	* @param LinearAcceleration		(out) Acceleration in meters per second per second.
	* @param AngularVelocity		(out) Angular velocity in radians per second.
	* @param LinearVelocity			(out) Velocity in meters per second.
	* @param TimeInSeconds			(out) Time when the reported IMU reading took place, in seconds.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds, ETrackedDeviceType DeviceType = ETrackedDeviceType::HMD);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsDeviceTracked(ETrackedDeviceType DeviceType);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary")
	static void SetCPUAndGPULevels(int CPULevel, int GPULevel);

	/**
	* Returns current user profile.
	*
	* @param Profile		(out) Structure to hold current user profile.
	* @return (boolean)	True, if user profile was acquired.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool GetUserProfile(FHmdUserProfile& Profile);

	/**
	* Sets 'base rotation' - the rotation that will be subtracted from
	* the actual HMD orientation.
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param Rotation			(in) Rotator object with base rotation
	* @param BaseOffsetInMeters (in) the vector to be set as base offset, in meters.
	* @param Options			(in) specifies either position, orientation or both should be set.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options);

	/**
	* Returns current base rotation and base offset.
	* The base offset is currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param OutRotation			(out) Rotator object with base rotation
	* @param OutBaseOffsetInMeters	(out) base position offset, vector, in meters.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters);

	/**
	 * Scales the HMD position that gets added to the virtual camera position.
	 *
	 * @param PosScale3D	(in) the scale to apply to the HMD position.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "This feature is no longer supported."))
	static void SetPositionScale3D(FVector PosScale3D) { }

	/**
	 * Sets 'base rotation' - the rotation that will be subtracted from
	 * the actual HMD orientation.
	 * The position offset might be added to current HMD position,
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs after the HMD orientation and position are applied.
	 *
	 * @param BaseRot			(in) Rotator object with base rotation
	 * @param PosOffset			(in) the vector to be added to HMD position.
	 * @param Options			(in) specifies either position, orientation or both should be set.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options);

	/**
	 * Returns current base rotation and position offset.
	 *
	 * @param OutRot			(out) Rotator object with base rotation
	 * @param OutPosOffset		(out) the vector with previously set position offset.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset);

	/**
	 * Adds loading splash screen with parameters
	 *
	 * @param Texture			(in) A texture asset to be used for the splash.
	 * @param TranslationInMeters (in) Initial translation of the center of the splash screen (in meters).
	 * @param Rotation			(in) Initial rotation of the splash screen, with the origin at the center of the splash screen.
	 * @param SizeInMeters		(in) Size, in meters, of the quad with the splash screen.
	 * @param DeltaRotation		(in) Incremental rotation, that is added each 2nd frame to the quad transform. The quad is rotated around the center of the quad.
	 * @param bClearBeforeAdd	(in) If true, clears splashes before adding a new one.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "Use Add Loading Screen Splash from the Head Mounted Display Loading Screen functions instead."))
	static void AddLoadingSplashScreen(class UTexture2D* Texture, FVector TranslationInMeters, FRotator Rotation, FVector2D SizeInMeters = FVector2D(1.0f, 1.0f), FRotator DeltaRotation = FRotator::ZeroRotator, bool bClearBeforeAdd = false);

	/**
	 * Removes all the splash screens.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "Use Clear Loading Screen Splashes from the Head Mounted Display Loading Screen functions instead."))
	static void ClearLoadingSplashScreens();

	/**
	* Returns true, if the app has input focus.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool HasInputFocus();

	/**
	* Returns true, if the system overlay is present.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool HasSystemOverlayPresent();

	/**
	* Returns the GPU utilization availability and value
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void GetGPUUtilization(bool& IsGPUAvailable, float& GPUUtilization);

	/**
	* Returns the GPU frame time on supported mobile platforms (Go for now)
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static float GetGPUFrameTime();

	/**
	* Returns the current multiresolution level
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static EFixedFoveatedRenderingLevel GetFixedFoveatedRenderingLevel();

	/**
	* Set the requested multiresolution level for the next frame, and whether FFR's level is now dynamic or not.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetFixedFoveatedRenderingLevel(EFixedFoveatedRenderingLevel level, bool isDynamic);

	/**
	* Returns the current device's name
	*/
	UE_DEPRECATED(4.22, "UOculusFunctionLibrary::GetDeviceName has been deprecated and no longer functions as before. Please use the enum-based GetDeviceType instead.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "UOculusFunctionLibrary::GetDeviceName has been deprecated and no longer functions as before. Please use the enum-based GetDeviceType instead."))
	static FString GetDeviceName();

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static EOculusDeviceType GetDeviceType();

	/**
	* Returns the current available frequencies
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static TArray<float> GetAvailableDisplayFrequencies();

	/**
	* Returns the current display frequency
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static float GetCurrentDisplayFrequency();

	/**
	* Sets the requested display frequency
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetDisplayFrequency(float RequestedFrequency);

	/**
	* Enables/disables positional tracking on devices that support it.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void EnablePositionTracking(bool bPositionTracking);

	/**
	* Enables/disables orientation tracking on devices that support it.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void EnableOrientationTracking(bool bOrientationTracking);

	/**
	* Set the Color Scale/Offset
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetColorScaleAndOffset(FLinearColor ColorScale, FLinearColor ColorOffset, bool bApplyToAllLayers = false);

	/**
	* Returns true if system headset is in 3dof mode 
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool GetSystemHmd3DofModeEnabled();

	/**
	* Returns the color space of the target HMD
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category="OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static EOculusColorSpace GetHmdColorDesc();

	/**
	* Sets the target HMD to do color space correction to a specific color space
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetClientColorDesc(EOculusColorSpace ColorSpace);

	/**
	 * Returns IStereoLayers interface to work with overlays.
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	static class IStereoLayers* GetStereoLayers();

	/* GUARDIAN API */
	/**
	* Returns true if the Guardian Outer Boundary is being displayed
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsGuardianDisplayed();

	/* GUARDIAN API */
	/**
	* Returns true if the Guardian has been set up by the user, false if the user is in "seated" mode and has not set up a play space.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsGuardianConfigured();

	/**
	* Returns the list of points in UE world space of the requested Boundary Type 
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	* @param UsePawnSpace			(in) Boolean indicating to return the points in world space or pawn space
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static TArray<FVector> GetGuardianPoints(EBoundaryType BoundaryType, bool UsePawnSpace = false);

	/**
	* Returns the dimensions in UE world space of the requested Boundary Type
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FVector GetGuardianDimensions(EBoundaryType BoundaryType);

	/**
	* Returns the transform of the play area rectangle, defining its position, rotation and scale to apply to a unit cube to match it with the play area.
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FTransform GetPlayAreaTransform();

	/**
	* Get the intersection result between a UE coordinate and a guardian boundary
	* @param Point					(in) Point in UE space to test against guardian boundaries
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FGuardianTestResult GetPointGuardianIntersection(const FVector Point, EBoundaryType BoundaryType);

	/**
	* Get the intersection result between a tracked device (HMD or controllers) and a guardian boundary
	* @param DeviceType             (in) Tracked Device type to test against guardian boundaries
	* @param BoundaryType			(in) An enum representing the boundary type requested, either Outer Boundary (exact guardian bounds) or PlayArea (rectangle inside the Outer Boundary)
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FGuardianTestResult GetNodeGuardianIntersection(ETrackedDeviceType DeviceType, EBoundaryType BoundaryType);

	/**
	* Forces the runtime to render guardian at all times or not
	* @param GuardianVisible			(in) True will display guardian, False will hide it
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|Guardian", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static void SetGuardianVisibility(bool GuardianVisible);

	/** When player triggers the Guardian boundary */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOculusGuardianTriggeredEvent, FGuardianTestResult);

	/** When player returns within outer bounds */
	DECLARE_MULTICAST_DELEGATE(FOculusGuardianReturnedEvent);

	/**
	* For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryTriggered.AddDynamic(this, &UCameraActor::PauseGameForBoundarySystem) where
	* PauseGameForBoundarySystem() takes a TArray<FBoundaryTestResult> parameter.
	*/
	//UPROPERTY(BlueprintAssignable, Category = "Input|OculusLibrary|Guardian")
	//static FOculusGuardianTriggeredEvent OnGuardianTriggered;

	/** For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryReturned.AddDynamic(this, &UCameraActor::ResumeGameForBoundarySystem) */
	//UPROPERTY(BlueprintAssignable, Category = "OculusLibrary|Guardian")
	//FOculusGuardianReturnedEvent OnGuardianReturned;

protected:
	static class OculusHMD::FOculusHMD* GetOculusHMD();
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
