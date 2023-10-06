// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOWarper.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

#include "Templates/SharedPointer.h"

class FDisplayClusterProjectionVIOSOLibrary;
struct FViosoPolicyConfiguration;

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

public:
	FDisplayClusterProjectionVIOSOPolicyViewData(const TSharedRef<FDisplayClusterProjectionVIOSOLibrary, ESPMode::ThreadSafe>& InVIOSOLibrary, const FViosoPolicyConfiguration& InConfigData, IDisplayClusterViewport* InViewport, const int32 InContextNum);
	virtual ~FDisplayClusterProjectionVIOSOPolicyViewData() = default;

public:
	bool IsWarperInterfaceValid();

	bool IsViewDataValid()
	{
		return bInitialized && IsWarperInterfaceValid();
	}

	bool UpdateVIOSO(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FVector& LocalLocation, const FRotator& LocalRotator, const float WorldToMeters, const float NCP, const float FCP);
	bool RenderVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* ShaderResourceTexture, FRHITexture2D* RenderTargetTexture);

protected:
	bool InitializeVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* RenderTargetTexture);

public:
	FVector  ViewLocation;
	FRotator ViewRotation;
	FMatrix  ProjectionMatrix;

private:
#if WITH_VIOSO_LIBRARY
	// VIOSO warper interface
	const TSharedRef<FDisplayClusterProjectionVIOSOWarper, ESPMode::ThreadSafe> WarperInterface;
#endif

	ERenderDevice RenderDevice = ERenderDevice::Unsupported;

	bool bInitialized = false;

	// This RTT is used for VIOSO. If we change this resource, we need to re-initialize VIOSO
	FRHITexture2D* UsedRenderTargetTexture = nullptr;

	FVector2D ClippingPlanes;
};
