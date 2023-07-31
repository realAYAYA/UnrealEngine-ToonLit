// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"
#include "Policy/EasyBlend/DisplayClusterProjectionEasyBlendViewAdapterBase.h"


/**
 * EasyBlend projection policy base class
 */
class FDisplayClusterProjectionEasyBlendPolicyBase
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionEasyBlendPolicyBase(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual const FString& GetType() const override;

	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport) override;
	virtual void HandleEndScene(IDisplayClusterViewport* InViewport) override;

	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) override;

	virtual bool IsWarpBlendSupported() override
	{
		return true;
	}

	// Request additional targetable resources for easyblend external warpblend
	virtual bool ShouldUseAdditionalTargetableResource() const override
	{
		return true;
	}

	virtual void ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy) override;

public:
	virtual bool IsEasyBlendRenderingEnabled() = 0;

protected:
	// Delegate view adapter instantiation to the RHI specific children
	virtual TUniquePtr<FDisplayClusterProjectionEasyBlendViewAdapterBase> CreateViewAdapter(const FDisplayClusterProjectionEasyBlendViewAdapterBase::FInitParams& InitParams) = 0;

private:
	// Parse EasyBlend related data from the nDisplay config file
	bool ReadConfigData(IDisplayClusterViewport* InViewport, FString& OutFile, FString& OutOrigin, float& OutGeometryScale);

private:
	FString OriginCompId;
	float EasyBlendScale = 1.f;
	bool bInitializeOnce = false;

	// RHI depended view adapter (different RHI require different DLL/API etc.)
	TUniquePtr<FDisplayClusterProjectionEasyBlendViewAdapterBase> ViewAdapter;
};
