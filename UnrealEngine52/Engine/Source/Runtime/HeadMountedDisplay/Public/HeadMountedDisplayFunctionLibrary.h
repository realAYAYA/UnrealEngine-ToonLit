// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "IIdentifiableXRDevice.h" // for FXRDeviceId
#include "XRGestureConfig.h"
#include "HeadMountedDisplayFunctionLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FXRDeviceOnDisconnectDelegate, const FString, OutReason);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FXRTimedInputActionDelegate, const float, Value, const FTimespan, Time);

UCLASS()
class HEADMOUNTEDDISPLAY_API UHeadMountedDisplayFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Returns whether or not we are currently using the head mounted display.
	 *
	 * @return (Boolean)  status of HMD
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta = (KeyWords = "HMD"))
	static bool IsHeadMountedDisplayEnabled();

	/**
	* Returns whether or not the HMD hardware is connected and ready to use.  It may or may not actually be in use.
	*
	* @return (Boolean)  status whether the HMD hardware is connected and ready to use.  It may or may not actually be in use. 
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay", meta = (KeyWords = "HMD"))
	static bool IsHeadMountedDisplayConnected();

	/**
	 * Switches to/from using HMD and stereo rendering.
	 *
	 * @param bEnable			(in) 'true' to enable HMD / stereo; 'false' otherwise
	 * @return (Boolean)		True, if the request was successful.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static bool EnableHMD(bool bEnable);

	/**
	 * Returns the name of the device, so scripts can modify their behaviour appropriately
	 *
	 * @return	FName specific to the currently active HMD device type.  "None" implies no device, "Unknown" implies a device with no description.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static FName GetHMDDeviceName();

	/**
	 * Returns the flags for the device, so scripts can modify their behaviour appropriately
	 *
	 * @return	IsAR, IsTablet, IsHeadMounted.  Returns false
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static int32 GetXRSystemFlags();

	/**
	 * Returns name of tracking system specific version string.
	 */
	UFUNCTION(BlueprintPure, Category = "HeadMountedDisplay")
	static FString GetVersionString();

	/**
	* Returns the worn state of the device.
	*
	* @return	Unknown, Worn, NotWorn.  If the platform does not detect this it will always return Unknown.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static EHMDWornState::Type GetHMDWornState();

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static void GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition);

	/**
	* If the HMD supports positional tracking, whether or not we are currently being tracked
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static bool HasValidTrackingPosition();

	/**
	* If the HMD has multiple positional tracking sensors, return a total number of them currently connected.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static int32 GetNumOfTrackingSensors();

	/**
	 * If the HMD has a positional sensor, this will return the game-world location of it, as well as the parameters for the bounding region of tracking.
	 * This allows an in-game representation of the legal positional tracking range.  All values will be zeroed if the sensor is not available or the HMD does not support it.
	 *
	 * @param Index				(in) Index of the tracking sensor to query
	 * @param Origin			(out) Origin, in world-space, of the sensor
	 * @param Rotation			(out) Rotation, in world-space, of the sensor
	 * @param LeftFOV			(out) Field-of-view, left from center, in degrees, of the valid tracking zone of the sensor
	 * @param RightFOV			(out) Field-of-view, right from center, in degrees, of the valid tracking zone of the sensor
	 * @param TopFOV			(out) Field-of-view, top from center, in degrees, of the valid tracking zone of the sensor
	 * @param BottomFOV			(out) Field-of-view, bottom from center, in degrees, of the valid tracking zone of the sensor
	 * @param Distance			(out) Nominal distance to sensor, in world-space
	 * @param NearPlane			(out) Near plane distance of the tracking volume, in world-space
	 * @param FarPlane			(out) Far plane distance of the tracking volume, in world-space
	 * @param IsActive			(out) True, if the query for the specified sensor succeeded.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay", meta = (AdvancedDisplay = "LeftFOV,RightFOV,TopFOV,BottomFOV,Distance,NearPlane,FarPlane"))
	static void GetTrackingSensorParameters(FVector& Origin, FRotator& Rotation, float& LeftFOV, float& RightFOV, float& TopFOV, float& BottomFOV, float& Distance, float& NearPlane, float& FarPlane, bool& IsActive, int32 Index = 0);

	/**
	 * If the HMD has a positional sensor, this will return the game-world location of it, as well as the parameters for the bounding region of tracking.
	 * This allows an in-game representation of the legal positional tracking range.  All values will be zeroed if the sensor is not available or the HMD does not support it.
	 *
	 * @param Origin			(out) Origin, in world-space, of the sensor
	 * @param Rotation			(out) Rotation, in world-space, of the sensor
	 * @param HFOV				(out) Field-of-view, horizontal, in degrees, of the valid tracking zone of the sensor
	 * @param VFOV				(out) Field-of-view, vertical, in degrees, of the valid tracking zone of the sensor
	 * @param CameraDistance	(out) Nominal distance to sensor, in world-space
	 * @param NearPlane			(out) Near plane distance of the tracking volume, in world-space
	 * @param FarPlane			(out) Far plane distance of the tracking volume, in world-space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta=(DeprecatedFunction, DeprecationMessage = "Use new GetTrackingSensorParameters / GetNumOfTrackingSensors"))
	static void GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float& FarPlane);

	/**
	 * Returns true, if HMD is in low persistence mode. 'false' otherwise.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta = (DeprecatedFunction, DeprecationMessage = "This functionality is no longer available. HMD platforms that support low persistence will always enable it."))
	static bool IsInLowPersistenceMode() { return false; }

	/**
	 * Switches between low and full persistence modes.
	 *
	 * @param bEnable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay", meta = (DeprecatedFunction, DeprecationMessage = "This functionality is no longer available. HMD platforms that support low persistence will always enable it."))
	static void EnableLowPersistenceMode(bool bEnable) { }

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking). 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 * @param Options			(in) specifies either position, orientation or both should be reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetOrientationAndPosition(float Yaw = 0.f, EOrientPositionSelector::Type Options = EOrientPositionSelector::OrientationAndPosition);

	/** 
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param Near				(in) Near clipping plane, in centimeters
	 * @param Far				(in) Far clipping plane, in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void SetClippingPlanes(float Near, float Far);

	/** 
	 * Returns the current VR pixel density. Pixel density sets the VR render 
	 * target texture size as a factor of recommended texture size. The recommended 
	 * texture size is the size that will result in no under sampling in most 
	 * distorted area of the view when computing the final image to be displayed 
	 * on the device by the runtime compositor.
	 *
	 * @return (float)	The pixel density to be used in VR mode.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static float GetPixelDensity();

	/**
	* Sets the World to Meters scale, which changes the scale of the world as perceived by the player
	*
	* @param NewScale	Specifies how many Unreal units correspond to one meter in the real world
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay", meta = (WorldContext = "WorldContext"))
	static void SetWorldToMetersScale(UObject* WorldContext, float NewScale = 100.f);

	/**
	* Returns the World to Meters scale, which corresponds to the scale of the world as perceived by the player
	*
	* @return	How many Unreal units correspond to one meter in the real world
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay", meta = (WorldContext = "WorldContext"))
	static float GetWorldToMetersScale(UObject* WorldContext);

	/**
	 * Sets current tracking origin type (eye level or floor level).
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetTrackingOrigin(TEnumAsByte<EHMDTrackingOrigin::Type> Origin);

	/**
	 * Returns current tracking origin type (eye level or floor level).
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static TEnumAsByte<EHMDTrackingOrigin::Type> GetTrackingOrigin();

	/**
	 * Returns a transform that can be used to convert points from tracking space to world space.
	 * Does NOT include the set WorldToMeters scale, as that is added in by the backing XR system to their tracking space poses.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay",  meta=(WorldContext="WorldContext"))
	static FTransform GetTrackingToWorldTransform(UObject* WorldContext);

	/**
	* Called to calibrate the offset transform between an external tracking source and the internal tracking source
	* (e.g. mocap tracker to and HMD tracker).  This should be called once per session, or when the physical relationship
	* between the external tracker and internal tracker changes (e.g. it was bumped or reattached).  After calibration,
	* calling UpdateExternalTrackingPosition will try to correct the internal tracker to the calibrated offset to prevent
	* drift between the two systems
	*
	* @param ExternalTrackingTransform		The transform in world-coordinates, of the reference marker of the external tracking system
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay|ExternalTracking")
	static void CalibrateExternalTrackingToHMD(const FTransform& ExternalTrackingTransform);

	/**
	* Called after calibration to attempt to pull the internal tracker (e.g. HMD tracking) in line with the external tracker
	* (e.g. mocap tracker).  This will set the internal tracker's base offset and rotation to match and realign the two systems.
	* This can be called every tick, or whenever realignment is desired.  Note that this may cause choppy movement if the two
	* systems diverge relative to each other, or a big jump if called infrequently when there has been significant drift
	*
	* @param ExternalTrackingTransform		The transform in world-coordinates, of the reference marker of the external tracking system
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay|ExternalTracking")
	static void UpdateExternalTrackingHMDPosition(const FTransform& ExternalTrackingTransform);

	/**
	 * Returns current state of VR focus.
	 *
	 * @param bUseFocus		(out) if set to true, then this App does use VR focus.
	 * @param bHasFocus		(out) if set to true, then this App currently has VR focus.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta=(DisplayName="Get VR Focus State"))
	static void GetVRFocusState(bool& bUseFocus, bool& bHasFocus);

	/**
	* Return true if spectator screen mode control is available.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay|SpectatorScreen")
	static bool IsSpectatorScreenModeControllable();

	/**
	* Sets the social screen mode.
	* @param Mode				(in) The social screen Mode.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay|SpectatorScreen")
	static void SetSpectatorScreenMode(ESpectatorScreenMode Mode);

	/**
	* Change the texture displayed on the social screen
	* @param	InTexture: new Texture2D
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay|SpectatorScreen")
	static void SetSpectatorScreenTexture(UTexture* InTexture);

	/**
	* Setup the layout for ESpectatorScreenMode::TexturePlusEye.
	* @param	EyeRectMin: min of screen rectangle the eye will be drawn in.  0-1 normalized.
	* @param	EyeRectMax: max of screen rectangle the eye will be drawn in.  0-1 normalized.
	* @param	TextureRectMin: min of screen rectangle the texture will be drawn in.  0-1 normalized.
	* @param	TextureRectMax: max of screen rectangle the texture will be drawn in.  0-1 normalized.
	* @param	bDrawEyeFirst: if true the eye is drawn before the texture, if false the reverse.
	* @param	bClearBlack: if true the render target will be drawn black before either rect is drawn.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay|SpectatorScreen")
	static void SetSpectatorScreenModeTexturePlusEyeLayout(FVector2D EyeRectMin, FVector2D EyeRectMax, FVector2D TextureRectMin, FVector2D TextureRectMax, bool bDrawEyeFirst = true, bool bClearBlack = false, bool bUseAlpha = false);

	/**
	 * Cross XR-System query that will list all XR devices currently being tracked.
	 *
	 * @param  SystemId		(Optional) Specifies an explicit system to poll devices from (use if you want only devices belonging to one explicit XR ecosystem, e.g. 'OculusHMD', or 'SteamVR')
	 * @param  DeviceType	Specifies the type of device to query for - defaults to 'Any' (meaning 'All').
	 *
	 * @return A list of device identifiers matching the query. Use these to query and operate on the device (e.g. through GetDevicePose, AddDeviceVisualizationComponent, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category="Input|XRTracking")
	static TArray<FXRDeviceId> EnumerateTrackedDevices(const FName SystemId = NAME_None, EXRTrackedDeviceType DeviceType = EXRTrackedDeviceType::Any);

	/**
	 * Cross XR-System query that returns a specific device's tracked position and orientation (in tracking space).
	 *
	 * @param  XRDeviceId				Specifies the device you're querying for.
	 * @param  bIsTracked				[out] Details if the specified device is tracked (i.e. should the rest of the outputs be used)
	 * @param  Orientation				[out] Represents the device's current rotation - NOTE: this value is not late updated and will be behind the render thread
	 * @param  bHasPositionalTracking	[out] Details if the specified device has positional tracking (i.e. if the position output should be used)
	 * @param  Position					[out] Represents the device's current position - NOTE: this value is not late updated and will be behind the render thread
	 */
	UFUNCTION(BlueprintCallable, Category="Input|XRTracking")
	static void GetDevicePose(const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position);

	/**
	 * Cross XR-System query that returns a specific device's position and orientation in world space.
	 *
	 * @param  XRDeviceId				Specifies the device you're querying for.
	 * @param  bIsTracked				[out] Details if the specified device is tracked (i.e. should the rest of the outputs be used)
	 * @param  Orientation				[out] Represents the device's current rotation - NOTE: this value is not late updated and will be behind the render thread
	 * @param  bHasPositionalTracking	[out] Details if the specified device has positional tracking (i.e. if the position output should be used)
	 * @param  Position					[out] Represents the device's current position - NOTE: this value is not late updated and will be behind the render thread
	 */
	UFUNCTION(BlueprintCallable, Category="Input|XRTracking",  meta=(WorldContext="WorldContext"))
	static void GetDeviceWorldPose(UObject* WorldContext, const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position);

	/**
	 * Cross XR-System query that returns whether the specified device is tracked or not.
	 *
	 * @param  XRDeviceId	Specifies the device you're querying for.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|XRTracking")
	static bool IsDeviceTracking(const FXRDeviceId& XRDeviceId);

	/**
	 * Cross XR-System query that returns critical information about the HMD display (position, orientation, device name)
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void GetHMDData(UObject* WorldContext, FXRHMDData& HMDData);

	/**
	 * Cross XR-System query that returns critical information about the motion controller (position, orientation, hand/finger position)
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData);

	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking", meta = (ToolTip = "Specify which gestures to capture."))
	static bool ConfigureGestures(const FXRGestureConfig& GestureConfig);

	/**
	 * Get the openXR interaction profile name for the given controller. Returns true if the openxr call is successfully made.  The string may be empty
	 * if there is no interaction profile associated with the controller.
	 */	
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking|OpenXR")
	static bool GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile);
	
	/** Connect to a remote device */
	UFUNCTION(BlueprintCallable, Category = "XR|HeadMountedDisplay")
	static EXRDeviceConnectionResult::Type ConnectRemoteXRDevice(const FString& IpAddress, const int32 BitRate);
	/** Disconnect remote AR Device */
	UFUNCTION(BlueprintCallable, Category = "XR|HeadMountedDisplay")
	static void DisconnectRemoteXRDevice();
	UFUNCTION(BlueprintCallable, Category = "XR|HeadMountedDisplay")
	static void SetXRDisconnectDelegate(const FXRDeviceOnDisconnectDelegate& InDisconnectedDelegate);
	static FXRDeviceOnDisconnectDelegate OnXRDeviceOnDisconnectDelegate;

	/** 
	* Hook up a delegate to get an OpenXR action event with action time.  
	* For a boolean input the the 'value' parameter of the delegate will be 1.0 for a press and 0.0 for a release.  For an analog input the value's range is action and platform specific.
	* Use in combination with GetControllerTransformForTime for potentially improved temporal transform precision and velocity data. 
	* "Left Grip" is an example of a valid ActionName.
	* Note: this is likely to be replaced by native support for event times in the core input system at some time in the future.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void SetXRTimedInputActionDelegate(const FName& ActionName, const FXRTimedInputActionDelegate& InDelegate);
	//** Clear a delegate to get an OpenXR action event with action time.*/
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static void ClearXRTimedInputActionDelegate(const FName& ActionPath);
	static TMap<FName, FXRTimedInputActionDelegate> OnXRTimedInputActionDelegateMap;

	/**
	* Get the transform and potentially velocity data at a specified time near the current frame in unreal world space.
	* This is intended for use with sub-frame input action timing data from SetXRTimedInputActionDelegate, or future support for timestamps in the core input system.
	* The valid time window is platform dependent, but the intention per OpenXR is to fetch transforms for times from, at most, the previous few frames in the past or future.  
	* The OpenXR spec suggests that 50ms in the past should return an accurate result.  There is no guarantee for the future, but the underlying system is likely to have been
	* designed to predict out to about 50ms as well.
	* On some platforms this  will always just return a cached position and rotation, ignoring time.  bTimeWasUsed will be false in that case.
	* AngularVelocityRadPerSec is a vector whose direction is the axis of rotation and whoes length is the speed of rotation in radians per second.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static bool GetControllerTransformForTime(UObject* WorldContext, const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& bTimeWasUsed, FRotator& Orientation, FVector& Position, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityRadPerSec, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration);

	/**
	 * Get the bounds of the area where the user can freely move while remaining tracked centered around the specified origin
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static FVector2D GetPlayAreaBounds(TEnumAsByte<EHMDTrackingOrigin::Type> Origin = EHMDTrackingOrigin::Stage);

	/**
	 * Get the transform of the specified tracking origin, if available.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static bool GetTrackingOriginTransform(TEnumAsByte<EHMDTrackingOrigin::Type> Origin, FTransform& OutTransform);

	/**
	 * Get the transform and dimensions of the playable area rectangle.  Returns false if none currently specified/available.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|XRTracking")
	static bool GetPlayAreaRect(FTransform& OutTransform, FVector2D& OutRect);

	/** Breaks an XR key apart into the interaction profile, handedness, motion source, indentifier and component. */
	UFUNCTION(BlueprintPure, Category = "Input|XRTracking", meta = (NativeBreakFunc))
	static void BreakKey(FKey InKey, FString& InteractionProfile, EControllerHand& Hand, FName& MotionSource, FString& Indentifier, FString& Component);
};
