// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRCamera.h"
#include "IXRTrackingSystem.h"
#include "SceneViewExtension.h"
#include "LateUpdateManager.h"

/** 
 * Default base implementation of IXRCamera.
 * Can either be used directly by implementations or extended with platform-specific features.
 */
class HEADMOUNTEDDISPLAY_API  FDefaultXRCamera : public IXRCamera, public FHMDSceneViewExtension
{
public:
	FDefaultXRCamera(const FAutoRegister&, IXRTrackingSystem* InTrackingSystem, int32 InDeviceId);

	virtual ~FDefaultXRCamera()
	{}

	// IXRSystemIdentifier interface
public:
	virtual FName GetSystemName() const override
	{
		return TrackingSystem->GetSystemName();
	}

	// IIdentifiableXRDevice interface:
public:
	virtual int32 GetSystemDeviceId() const override
	{
		return DeviceId;
	}

	// IXRCamera interface
public:

	/**
	 * Set the view offset mode to assume an implied HMD position
	 */
	virtual void UseImplicitHMDPosition(bool bInImplicitHMDPosition) override
	{ 
		bUseImplicitHMDPosition = bInImplicitHMDPosition;
	}

	/**
	* Returns current setting controlling whether to assume an implied hmd position
	*/
	virtual bool GetUseImplicitHMDPosition() const override
	{
		return bUseImplicitHMDPosition;
	}

	/**
	 * Optionally called by APlayerController to apply the orientation of the
	 * headset to the PC's rotation. If this is not done then the PC will face
	 * differently than the camera, which might be good (depending on the game).
	 */
	virtual void ApplyHMDRotation(APlayerController* PC, FRotator& ViewRotation) override;

	/**
	 * Apply the orientation and position of the headset to the Camera.
	 */
	virtual bool UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	
	virtual void OverrideFOV(float& InOutFOV) override;

	/** Setup state for applying the render thread late update */
	virtual void SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component, bool bSkipLateUpdate) override;

	virtual void CalculateStereoCameraOffset(const int32 ViewIndex, FRotator& ViewRotation, FVector& ViewLocation) override;

	// ISceneViewExtension interface:
public:
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	// FWorldSceneViewExtension interface:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	

protected:
	IXRTrackingSystem* TrackingSystem;
	const int32 DeviceId;

	FRotator DeltaControlRotation;
	FQuat DeltaControlOrientation;
private:
	FLateUpdateManager LateUpdate;
	bool bUseImplicitHMDPosition;
	mutable bool bCurrentFrameIsStereoRendering;
};
