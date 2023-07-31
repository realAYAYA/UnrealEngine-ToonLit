// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHeadMountedDisplay.h"
#include "XRTrackingSystemBase.h"
#include "DefaultSpectatorScreenController.h"

/**
 * Default implementation for various IHeadMountedDisplay methods.
 * You can extend this class instead of IHeadMountedDisplay directly when implementing support for new HMD devices.
 */

class HEADMOUNTEDDISPLAY_API FHeadMountedDisplayBase : public FXRTrackingSystemBase, public IHeadMountedDisplay, public IStereoRendering
{

public:
	FHeadMountedDisplayBase(IARSystemSupport* InARImplementation);
	virtual ~FHeadMountedDisplayBase() {}

	/**
	 * Retrieves the HMD name, by default this is the same as the system name.
	 */
	virtual FName GetHMDName() const override { return GetSystemName(); }

	/**
	 * Record analytics - To add custom information logged with the analytics, override PopulateAnalyticsAttributes
	 */
	virtual void RecordAnalytics() override;

	/** 
	 * Default IXRTrackingSystem implementation
	 */
	virtual bool IsHeadTrackingAllowed() const override;

	/** Optional IXRTrackingSystem methods.
	  */
	virtual bool IsHeadTrackingEnforced() const override;
	virtual void SetHeadTrackingEnforced(bool bEnabled) override;

	/** 
	 * Default stereo layer implementation
	 */
	virtual IStereoLayers* GetStereoLayers() override;

	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override;
	virtual void OnLateUpdateApplied_RenderThread(FRHICommandListImmediate& RHICmdList, const FTransform& NewRelativeTransform) override;

	virtual void CalculateStereoViewOffset(const int32 ViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual bool IsSpectatorScreenActive() const override;

	virtual class ISpectatorScreenController* GetSpectatorScreenController() override;
	virtual class ISpectatorScreenController const* GetSpectatorScreenController() const override;

	// Spectator Screen Hooks into specific implementations
	// Get the point on the left eye render target which the viewers eye is aimed directly at when looking straight forward. 0,0 is top left.
	virtual FVector2D GetEyeCenterPoint_RenderThread(const int32 ViewIndex) const;
	// Get the rectangle of the HMD rendertarget for the left eye which seems undistorted enough to be cropped and displayed on the spectator screen.
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const { return FIntRect(0, 0, 1, 1); }
	// Helper to copy one render target into another for spectator screen display
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const {}

protected:
	/**
	 * Called by RecordAnalytics when creating the analytics event sent during HMD initialization.
	 *
	 * Return false to disable recording the analytics event	
	 */
	virtual bool PopulateAnalyticsAttributes(TArray<struct FAnalyticsEventAttribute>& EventAttributes);

	/**
	 * Implement this method to provide an alternate render target for head locked stereo layer rendering, when using the default Stereo Layers implementation.
	 * 
	 * Return a FTexture2DRHIRef pointing to a texture that can be composed on top of each eye without applying reprojection to it.
	 * Return nullptr to render head locked stereo layers into the same render target as other layer types, in which case InOutViewport must not be modified.
	 */
	virtual FTexture2DRHIRef GetOverlayLayerTarget_RenderThread(int32 ViewIndex, FIntRect& InOutViewport) { return nullptr; }

	/**
	 * Implement this method to override the render target for scene based stereo layers.
	 * Return nullptr to render stereo layers into the normal render target passed to the stereo layers scene view extension, in which case OutViewport must not be modified.
	 */
	virtual FTexture2DRHIRef GetSceneLayerTarget_RenderThread(int32 ViewIndex, FIntRect& InOutViewport) { return nullptr; }

	mutable TSharedPtr<class FDefaultStereoLayers, ESPMode::ThreadSafe> DefaultStereoLayers;
	
	friend class FDefaultStereoLayers;

	TUniquePtr<FDefaultSpectatorScreenController> SpectatorScreenController;

	// Sane pixel density values
	static constexpr float PixelDensityMin = 0.1f;
	static constexpr float PixelDensityMax = 2.0f;

	/**
	 * CVar sink for pixel density
	 */
	static void CVarSinkHandler();
	static FAutoConsoleVariableSink CVarSink;

private:
	bool bHeadTrackingEnforced;
};
