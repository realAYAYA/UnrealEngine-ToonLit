// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"
#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOPolicyViewData.h"

/**
 * VIOSO projection policy
 */
class FDisplayClusterProjectionVIOSOPolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionVIOSOPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy, const TSharedRef<FDisplayClusterProjectionVIOSOLibrary, ESPMode::ThreadSafe>& InVIOSOLibrary);
	virtual ~FDisplayClusterProjectionVIOSOPolicy();

public:
	//~~BEGIN IDisplayClusterProjectionPolicy
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

	virtual bool HasPreviewMesh(IDisplayClusterViewport* InViewport) override;
	virtual class UMeshComponent* GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) override;

	//~~END IDisplayClusterProjectionPolicy

protected:
	bool ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy);
	void ImplRelease();

protected:
	// VIOSO DLL api
	const TSharedRef<FDisplayClusterProjectionVIOSOLibrary, ESPMode::ThreadSafe> VIOSOLibrary;

	FViosoPolicyConfiguration ViosoConfigData;

	TArray<TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>> Views;

	FDisplayClusterSceneComponentRef PreviewMeshComponentRef;

	FCriticalSection DllAccessCS;
};
