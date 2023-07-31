// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIResources.h"
#include "Engine/Scene.h"
#include "StereoRenderTargetManager.h"

#include "Render/Device/IDisplayClusterRenderDevice.h"
#include "Render/IDisplayClusterRenderManager.h"

#include "Containers/Queue.h"

class IDisplayClusterPostProcess;
class FDisplayClusterPresentationBase;
class IDisplayClusterViewportManager;
class IDisplayClusterViewportManagerProxy;
class FSceneView;


/**
 * Abstract render device
 */
class FDisplayClusterDeviceBase
	: public IStereoRenderTargetManager
	, public IDisplayClusterRenderDevice
{
public:
	FDisplayClusterDeviceBase() = delete;
	FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode InRenderFrameMode);
	virtual ~FDisplayClusterDeviceBase();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterRenderDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Initialize() override;
	virtual void StartScene(UWorld* InWorld) override;
	virtual void EndScene() override;
	virtual void PreTick(float DeltaSeconds) override;

	virtual IDisplayClusterPresentation* GetPresentation() const override;

	virtual bool BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame) override;
	virtual void FinalizeNewFrame() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsStereoEnabled() const override;
	virtual bool IsStereoEnabledOnNextFrame() const override;
	virtual bool EnableStereo(bool bStereoEnabled = true) override;
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) override;
	virtual void AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const int32 ViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const int32 ViewIndex) const override;
	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* BackBuffer, FRHITexture* SrcTexture, FVector2D WindowSize) const override;
	
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override
	{
		return FMath::Max(1, DesiredNumberOfViews);
	}

	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const override;

	virtual IStereoRenderTargetManager* GetRenderTargetManager() override
	{ return this; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRenderTargetManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return true; }

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;

	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<struct IPooledRenderTarget>& DepthTarget) override
	{ return false; }

	virtual uint32 GetNumberOfBufferedFrames() const override
	{ return 1; }

	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override
	{ return false; }

	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1)
	{ return false; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterDeviceBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	enum EDisplayClusterEyeType
	{
		StereoLeft  = 0,
		Mono        = 1,
		StereoRight = 2,
		COUNT
	};

protected:
	// Factory method to instantiate an output presentation class implementation
	virtual FDisplayClusterPresentationBase* CreatePresentationObject(FViewport* const Viewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& SyncPolicy) = 0;

	// Checks if custom post processing settings is assigned for specific viewport and assign them to be used
	virtual void StartFinalPostprocessSettings(struct FPostProcessSettings* StartPostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex) override;
	virtual bool OverrideFinalPostprocessSettings(struct FPostProcessSettings* OverridePostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex, float& BlendWeight) override;
	virtual void EndFinalPostprocessSettings(struct FPostProcessSettings* FinalPostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex) override;

private:
	// Runtime viewport manager api for game and render threads. Internal usage only
	IDisplayClusterViewportManager*      ViewportManagerPtr = nullptr;
	IDisplayClusterViewportManagerProxy* ViewportManagerProxyPtr = nullptr;

	EDisplayClusterRenderFrameMode RenderFrameMode = EDisplayClusterRenderFrameMode::Mono;
	int32 DesiredNumberOfViews = 0;

	// UE main viewport
	FViewport* MainViewport = nullptr;

	bool bIsCustomPresentSet = false;

	// Data access synchronization
	mutable FCriticalSection InternalsSyncScope;

	// Pointer to the current presentation handler
	FDisplayClusterPresentationBase* CustomPresentHandler = nullptr;
};
