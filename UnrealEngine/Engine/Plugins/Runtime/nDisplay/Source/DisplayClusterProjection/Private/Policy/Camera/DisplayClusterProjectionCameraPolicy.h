// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"
#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "Misc/DisplayClusterObjectRef.h"

class UCameraComponent;


/**
 * Camera projection policy implementation
 */
class FDisplayClusterProjectionCameraPolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionCameraPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual const FString& GetType() const override;

	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport) override;
	virtual void HandleEndScene(IDisplayClusterViewport* InViewport) override;

	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) override;

	virtual bool ShouldUseSourceTextureWithMips() const override
	{
		return true;
	}

public:
	void SetCamera(UCameraComponent* const NewCamera, const FDisplayClusterProjectionCameraPolicySettings& InCameraSettings);

private:
	bool ImplGetProjectionMatrix(const float CameraFOV, const float CameraAspectRatio, IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix);

protected:
	UCameraComponent* GetCameraComponent();

private:
	// Camera to use for rendering
	FDisplayClusterSceneComponentRef CameraRef;
	FDisplayClusterProjectionCameraPolicySettings CameraSettings;
	float ZNear = 1.f;
	float ZFar = 1.f;
};
