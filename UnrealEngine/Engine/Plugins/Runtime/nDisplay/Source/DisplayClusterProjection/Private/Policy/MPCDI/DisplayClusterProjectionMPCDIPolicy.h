// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"

#include "WarpBlend/IDisplayClusterWarpBlend.h"
#include "WarpBlend/DisplayClusterWarpContext.h"


/**
 * MPCDI projection policy
 * Supported load from 'MPCDI' and 'PFM' files
 */
class FDisplayClusterProjectionMPCDIPolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	enum class EWarpType : uint8
	{
		mpcdi = 0,
		mesh
	};

public:
	FDisplayClusterProjectionMPCDIPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);
	virtual ~FDisplayClusterProjectionMPCDIPolicy();

public:
	virtual EWarpType GetWarpType() const
	{
		return EWarpType::mpcdi;
	}

	// This policy can support ICVFX rendering
	virtual bool ShouldSupportICVFX() const
	{
		return true;
	}

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

	virtual bool GetWarpBlendInterface(TSharedPtr<class IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlendInterface) const override;
	virtual bool GetWarpBlendInterface_RenderThread(TSharedPtr<class IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlendInterfaceProxy) const override;

	virtual bool ShouldUseSourceTextureWithMips() const override
	{
		// Support input texture with mips
		return true;
	}

	virtual bool ShouldUseAdditionalTargetableResource() const override
	{
		// Request additional targetable resources for warp&blend output
		return true;
	}

	virtual void UpdateProxyData(IDisplayClusterViewport* InViewport) override;

protected:
	bool CreateWarpBlendFromConfig(IDisplayClusterViewport* InViewport);
	void ImplRelease();

protected:
	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlendInterface;
	TArray<FDisplayClusterWarpContext> WarpBlendContexts;

	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlendInterface_Proxy;
	TArray<FDisplayClusterWarpContext> WarpBlendContexts_Proxy;

private:
	bool bInvalidConfiguration = false;
	bool bIsPreviewMeshEnabled = false;

#if WITH_EDITOR
protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicyPreview
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool HasPreviewMesh() override
	{
		return true;
	}
	virtual class UMeshComponent* GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) override;

	void ReleasePreviewMeshComponent();

private:
	FDisplayClusterSceneComponentRef PreviewMeshComponentRef;
#endif
};
