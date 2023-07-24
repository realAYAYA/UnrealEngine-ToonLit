// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"
#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOWarper.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Templates/SharedPointer.h"

/**
 * VIOSO projection policy data
 */
struct FDisplayClusterProjectionVIOSOPolicyViewData
	: public TSharedFromThis<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>
{
	enum class ERenderDevice : uint8
	{
		Unsupported = 0,
		D3D11,
		D3D12
	};

	FDisplayClusterProjectionVIOSOPolicyViewData();
	virtual ~FDisplayClusterProjectionVIOSOPolicyViewData();

public:
	bool IsValid();

	bool UpdateVIOSO(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FVector& LocalLocation, const FRotator& LocalRotator, const float WorldToMeters, const float NCP, const float FCP);
	bool RenderVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* ShaderResourceTexture, FRHITexture2D* RenderTargetTexture, const FViosoPolicyConfiguration& InConfigData);

protected:
	bool InitializeVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* RenderTargetTexture, const FViosoPolicyConfiguration& InConfigData);

public:
	FVector  ViewLocation;
	FRotator ViewRotation;
	FMatrix  ProjectionMatrix;

private:
	ERenderDevice RenderDevice = ERenderDevice::Unsupported;
	FViosoWarper Warper;

	bool bInitialized = false;
};

/**
 * VIOSO projection policy
 */
class FDisplayClusterProjectionVIOSOPolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionVIOSOPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);
	virtual ~FDisplayClusterProjectionVIOSOPolicy();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual const FString& GetType() const override;

	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport) override;
	virtual void HandleEndScene(IDisplayClusterViewport* InViewport) override;

	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) override;

	virtual bool IsWarpBlendSupported() override;
	virtual void ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy) override;

	// Request additional targetable resources for domeprojection  external warpblend
	virtual bool ShouldUseAdditionalTargetableResource() const override
	{
		return true;
	}

	virtual bool ShouldUseSourceTextureWithMips() const override
	{
		return true;
	}

protected:
	bool ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy);
	void ImplRelease();

protected:
	FViosoPolicyConfiguration ViosoConfigData;

	TArray<TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>> Views;

	FCriticalSection DllAccessCS;
};
