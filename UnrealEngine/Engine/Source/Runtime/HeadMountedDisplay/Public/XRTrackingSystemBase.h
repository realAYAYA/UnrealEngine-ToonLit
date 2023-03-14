// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRTrackingSystem.h"
#include "IXRCamera.h"
#include "ARSupportInterface.h"

class HEADMOUNTEDDISPLAY_API FXRTrackingSystemDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FXRTrackingOriginChanged, const IXRTrackingSystem* /*TrackingSystem*/);
	static FXRTrackingOriginChanged OnXRTrackingOriginChanged;

	DECLARE_MULTICAST_DELEGATE(FXRPlayAreaChanged);
	static FXRPlayAreaChanged OnXRPlayAreaChanged;
};

/** 
 * Base utility class for implementations of the IXRTrackingSystem interface
 * Contains helpers and default implementation of most abstract methods, so final implementations only need to override features that they support.
 */
class HEADMOUNTEDDISPLAY_API FXRTrackingSystemBase : public IXRTrackingSystem
{
public:
	FXRTrackingSystemBase(IARSystemSupport* InARImplementation);
	virtual ~FXRTrackingSystemBase();

	/**
	 * Whether or not the system supports positional tracking (either via sensor or other means).
	 * The default implementation always returns false, indicating that only rotational tracking is supported.
	 */
	virtual bool DoesSupportPositionalTracking() const override { return false; }

	/**
	 * If the system currently has valid tracking positions. If not supported at all, returns false.
	 * Defaults to calling DoesSupportPositionalTracking();
	 */
	virtual bool HasValidTrackingPosition() override { return DoesSupportPositionalTracking(); }

	/**
	 * Get the count of tracked devices.
	 *
	 * @param Type Optionally limit the count to a certain type
	 * @return the count of matching tracked devices.
	 *
	 * The default implementation calls EnumerateTrackedDevices and returns the number of elements added to the array.
	 */
	virtual uint32 CountTrackedDevices(EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

	/**
	 * Check current tracking status of a device.
	 * @param DeviceId the device to request status for.
	 * @return true if the system currently has valid tracking info for the given device ID.
	 *
	 * The default implementation returns the result of calling GetCurrentPose(DeviceId, ...), ignoring the returned pose.
	 */
	virtual bool IsTracking(int32 DeviceId) override;

	/**
	 * If the device id represents a tracking sensor, reports the frustum properties in game-world space of the sensor.
	 * @param DeviceId the device to request information for.
	 * @param OutOrientation The current orientation of the device.
	 * @param OutPosition The current position of the device.
	 * @param OutSensorProperties A struct containing the tracking sensor properties.
	 * @return true if the device tracking is valid and supports returning tracking sensor properties.
	 *
	 * The default implementation returns false for all device ids.
	 */
	virtual bool GetTrackingSensorProperties(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties) override
	{
		return false;
	}

	/**
	 * If the device id represents a tracking sensor, reports the device type.
	 * @param DeviceId the device to request information for.
	 * @return the device type enum.
	 */
	virtual EXRTrackedDeviceType GetTrackedDeviceType(int32 DeviceId) const override
	{
		return EXRTrackedDeviceType::Invalid;
	}

	/**
	 * If the device id represents a tracking sensor, reports the serial number as a string if the device supports it.
	 * @param DeviceId the device to request information for.
	 * @return the serial number of the device if it's available.
	 */
	virtual FString GetTrackedDevicePropertySerialNumber(int32 DeviceId) override
	{
		return FString(TEXT("NoSerial"));
	}

	/**
	 * Get the IXCamera instance for the given device.
	 * @param DeviceId the device the camera should track.
	 * @return a shared pointer to an IXRCamera.
	 *
	 * The default implementation only supports a single IXRCamera for the HMD Device, returning a FDefaultXRCamera instance.
	 */
	virtual TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > GetXRCamera(int32 DeviceId = HMDDeviceId) override;

	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const { return FString(TEXT("GenericHMD")); }

	virtual bool GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition) override;

	/**
	 * Sets tracking origin (either 'eye'-level or 'floor'-level).
	 *
	 * The default implementations simply ignores the origin value.
	 */
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override { }

	/**
	 * Returns current tracking origin.
	 *
	 * The default implementation always reports 'eye'-level tracking.
	 */
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override
	{
		return EHMDTrackingOrigin::Eye;
	}

	/**
	 * Returns the system's latest known tracking-to-world transform.
	 */
	virtual FTransform GetTrackingToWorldTransform() const override;

	/** 
	 * Returns a transform for converting from 'Floor' origin space to 'Eye' origin space.
	 * The default implementation always returns the identity.
	 */
	virtual bool GetFloorToEyeTrackingTransform(FTransform& OutFloorToEye) const override { OutFloorToEye = FTransform::Identity; return false; }

	/**
	 * Refreshes the system's known tracking-to-world transform.
	 */
	virtual void UpdateTrackingToWorldTransform(const FTransform& TrackingToWorldOverride) override;

	/**
	 * Called to calibrate the offset transform between an external tracking source and the internal tracking source
	 * (e.g. mocap tracker to and HMD tracker).  This should be called once per session, or when the physical relationship
	 * between the external tracker and internal tracker changes (e.g. it was bumped or reattached).  After calibration,
	 * calling UpdateExternalTrackingPosition will try to correct the internal tracker to the calibrated offset to prevent
	 * drift between the two systems
	 *
	 * @param ExternalTrackingTransform		The transform in world-coordinates, of the reference marker of the external tracking system
	 */
	virtual void CalibrateExternalTrackingSource(const FTransform& ExternalTrackingTransform) override;

	/**
	* Called after calibration to attempt to pull the internal tracker (e.g. HMD tracking) in line with the external tracker
	* (e.g. mocap tracker).  This will set the internal tracker's base offset and rotation to match and realign the two systems.
	* This can be called every tick, or whenever realignment is desired.  Note that this may cause choppy movement if the two
	* systems diverge relative to each other, or a big jump if called infrequently when there has been significant drift
	*
	* @param ExternalTrackingTransform		The transform in world-coordinates, of the reference marker of the external tracking system
	*/
	virtual void UpdateExternalTrackingPosition(const FTransform& ExternalTrackingTransform) override;
	virtual class IXRLoadingScreen* GetLoadingScreen() override final;

	virtual void GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData) override;
	virtual bool GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile) override;

	virtual bool ConfigureGestures(const FXRGestureConfig& GestureConfig) override { return false; }

	TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> GetARCompositionComponent();
	const TSharedPtr<const FARSupportInterface , ESPMode::ThreadSafe> GetARCompositionComponent() const;

protected:

	/** Override this method if you need to customize the IXRLoadingScreen implementation.
	 *  It will be called from GetLoadingScreen() if LoadingScreen is not valid */
	virtual class IXRLoadingScreen* CreateLoadingScreen();

	/** 
	 * Meant to be called by sub-classes whenever the tracking origin is altered.
	 */
	virtual void OnTrackingOriginChanged()
	{
		FXRTrackingSystemDelegates::OnXRTrackingOriginChanged.Broadcast(this);
	}

	virtual void OnPlayAreaChanged()
	{
		FXRTrackingSystemDelegates::OnXRPlayAreaChanged.Broadcast();
	}

	/**
	 * Computes the project's tracking-to-world transform based off how the user 
	 * has set up their camera system (assumes the camera is parented to the XR 
	 * origin, and in turn uses that transform).
	 *
	 * Intended to be called from OnStartGameFrame()
	 */
	FTransform RefreshTrackingToWorldTransform(FWorldContext& WorldContext);
	FTransform ComputeTrackingToWorldTransform(FWorldContext& WorldContext) const;
	
	TSharedPtr< class FDefaultXRCamera, ESPMode::ThreadSafe > XRCamera;

	FTransform CachedTrackingToWorld;

	/**
	 * If the tracker is trying to lock itself to an external tracking source for drift control,
	 * this stores the calibrated offset between the external tracking system and the internal tracking system
	 * (e.g. a location from a mocap system tracker to the HMD's internal IMU).  UpdateExternalTrackingPostion
	 * will attempt to normalize the internal tracking system to match this calibration when called.
	 */
	FTransform CalibratedOffset;
	mutable class IXRLoadingScreen* LoadingScreen;

private:
	TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> ARCompositionComponent;
};

