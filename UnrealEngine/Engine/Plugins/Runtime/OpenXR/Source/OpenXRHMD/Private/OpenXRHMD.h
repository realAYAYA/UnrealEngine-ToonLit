// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OpenXRHMD_Layer.h"
#include "OpenXRAssetManager.h"
#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRTrackingSystemBase.h"
#include "XRRenderTargetManager.h"
#include "XRRenderBridge.h"
#include "XRSwapChain.h"
#include "SceneViewExtension.h"
#include "StereoLayerManager.h"
#include "DefaultSpectatorScreenController.h"
#include "IHeadMountedDisplayVulkanExtensions.h"
#include "IOpenXRExtensionPluginDelegates.h"

#include <openxr/openxr.h>

class APlayerController;
class FSceneView;
class FSceneViewFamily;
class UCanvas;
class FOpenXRRenderBridge;
class IOpenXRInputModule;

/**
 * Simple Head Mounted Display
 */
class FOpenXRHMD
	: public FHeadMountedDisplayBase
	, public FXRRenderTargetManager
	, public FHMDSceneViewExtension
	, public FOpenXRAssetManager
	, public TStereoLayerManager<FOpenXRLayer>
	, public IOpenXRExtensionPluginDelegates
{
public:
	class FDeviceSpace
	{
	public:
		FDeviceSpace(XrAction InAction, XrPath InPath);
		~FDeviceSpace();

		bool CreateSpace(XrSession InSession);
		void DestroySpace();

		XrAction Action;
		XrSpace Space;
		XrPath Path;
	};

	class FTrackingSpace
	{
	public:
		FTrackingSpace(XrReferenceSpaceType InType);
		FTrackingSpace(XrReferenceSpaceType InType, XrPosef InBasePose);
		~FTrackingSpace();

		bool CreateSpace(XrSession InSession);
		void DestroySpace();

		XrReferenceSpaceType Type;
		XrSpace Handle;
		XrPosef BasePose;
	};

	struct FPluginViewInfo
	{
		class IOpenXRExtensionPlugin* Plugin = nullptr;
		EStereoscopicPass PassType = EStereoscopicPass::eSSP_PRIMARY;
		bool bIsPluginManaged = false;
	};

	// The game and render threads each have a separate copy of these structures so that they don't stomp on each other or cause tearing
	// when the game thread progresses to the next frame while the render thread is still working on the previous frame.
	struct FPipelinedFrameState
	{
		XrFrameState FrameState{XR_TYPE_FRAME_STATE};
		XrViewState ViewState{XR_TYPE_VIEW_STATE};
		TArray<XrView> Views;
		TArray<XrSpaceLocation> DeviceLocations;
		TSharedPtr<FTrackingSpace> TrackingSpace;
		float WorldToMetersScale = 100.0f;
		float PixelDensity = 1.0f;

		TArray<XrViewConfigurationView> ViewConfigs;
		TArray<FPluginViewInfo> PluginViewInfos;

		bool bXrFrameStateUpdated = false;
	};

	struct FPipelinedLayerState
	{
		TArray<XrCompositionLayerQuad> QuadLayers;
		TArray<XrCompositionLayerProjectionView> ProjectionLayers;
		TArray<XrCompositionLayerDepthInfoKHR> DepthLayers;

		TArray<XrSwapchainSubImage> ColorImages;
		TArray<XrSwapchainSubImage> DepthImages;

		FXRSwapChainPtr ColorSwapchain;
		FXRSwapChainPtr DepthSwapchain;
		TArray<FXRSwapChainPtr> QuadSwapchains;

		bool bBackgroundLayerVisible = true;
		bool bSubmitBackgroundLayer = true;
	};

	class FVulkanExtensions : public IHeadMountedDisplayVulkanExtensions
	{
	public:
		FVulkanExtensions(XrInstance InInstance, XrSystemId InSystem) : Instance(InInstance), System(InSystem) {}
		virtual ~FVulkanExtensions() {}

		/** IHeadMountedDisplayVulkanExtensions */
		virtual bool GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out) override;
		virtual bool GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out) override;

	private:
		XrInstance Instance;
		XrSystemId System;

		TArray<char> Extensions;
		TArray<char> DeviceExtensions;
	};

	/** IXRTrackingSystem interface */
	virtual FName GetSystemName() const override
	{
		// This identifier is relied upon for plugin identification,
		// see GetHMDName() to query the true XR system name.
		static FName DefaultName(TEXT("OpenXR"));
		return DefaultName;
	}

	int32 GetXRSystemFlags() const override
	{
		int32 flags = EXRSystemFlags::IsHeadMounted;

		if (SelectedEnvironmentBlendMode != XR_ENVIRONMENT_BLEND_MODE_OPAQUE)
		{
			flags |= EXRSystemFlags::IsAR;
		}

		if (bSupportsHandTracking)
		{
			flags |= EXRSystemFlags::SupportsHandTracking;
		}

		return flags;
	}

	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual bool GetRelativeEyePose(int32 InDeviceId, int32 InViewIndex, FQuat& OutOrientation, FVector& OutPosition) override;

	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;
	virtual void Recenter(EOrientPositionSelector::Type Selector, float Yaw = 0.f);

	virtual bool GetIsTracked(int32 DeviceId);
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual bool GetPoseForTime(int32 DeviceId, FTimespan Timespan, bool& OutTimeWasUsed, FQuat& CurrentOrientation, FVector& CurrentPosition, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityRadPerSec, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration, float WorldToMetersScale);
	virtual bool GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile) override;
	
	virtual void SetBaseRotation(const FRotator& InBaseRotation) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& InBaseOrientation) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual void SetBasePosition(const FVector& InBasePosition) override;
	virtual FVector GetBasePosition() const override;

	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override
	{
		if (!bUseCustomReferenceSpace)
		{
			TrackingSpaceType = (NewOrigin == EHMDTrackingOrigin::Eye || StageSpace == XR_NULL_HANDLE) ? XR_REFERENCE_SPACE_TYPE_LOCAL : XR_REFERENCE_SPACE_TYPE_STAGE;
			bTrackingSpaceInvalid = true;
		}
	}
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override
	{
		return (TrackingSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) ? EHMDTrackingOrigin::Eye : EHMDTrackingOrigin::Stage;
	}

	virtual class IHeadMountedDisplay* GetHMDDevice() override
	{
		return this;
	}
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override
	{
		return SharedThis(this);
	}
#if !PLATFORM_HOLOLENS
	// Native stereo layers severely impact performance on Hololens
	virtual class IStereoLayers* GetStereoLayers() override
	{
		return this;
	}
#endif

	virtual void GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData) override;

	virtual float GetWorldToMetersScale() const override;

	virtual FVector2D GetPlayAreaBounds(EHMDTrackingOrigin::Type Origin) const override;
	virtual bool GetPlayAreaRect(FTransform& OutTransform, FVector2D& OutExtent) const override;
	virtual bool GetTrackingOriginTransform(TEnumAsByte<EHMDTrackingOrigin::Type> Origin, FTransform& OutTransform)  const override;

	virtual bool HDRGetMetaDataForStereo(EDisplayOutputFormat& OutDisplayOutputFormat, EDisplayColorGamut& OutDisplayColorGamut, bool& OutbHDRSupported) override;

protected:

	bool StartSession();
	bool StopSession();
	bool OnStereoStartup();
	bool OnStereoTeardown();
	bool ReadNextEvent(XrEventDataBuffer* buffer);
	void DestroySession();

	void RequestExitApp();

	void BuildOcclusionMeshes();
	bool BuildOcclusionMesh(XrVisibilityMaskTypeKHR Type, int View, FHMDViewMesh& Mesh);

	const FPipelinedFrameState& GetPipelinedFrameStateForThread() const;
	FPipelinedFrameState& GetPipelinedFrameStateForThread();

	void UpdateDeviceLocations(bool bUpdateOpenXRExtensionPlugins);
	void EnumerateViews(FPipelinedFrameState& PipelineState);
	void LocateViews(FPipelinedFrameState& PipelinedState, bool ResizeViewsArray = false);
	bool IsViewManagedByPlugin(int32 ViewIndex) const;

	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, 
								  bool bClearBlack, bool bNoAlpha, ERenderTargetActions RTAction, ERHIAccess FinalDstAccess) const;
	
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, const FXRSwapChainPtr& DstSwapChain, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const;

	void AllocateDepthTextureInternal(uint32 Index, uint32 SizeX, uint32 SizeY, uint32 NumSamples);

	// Used with FCoreDelegates
	void VRHeadsetRecenterDelegate();

	void SetupFrameQuadLayers_RenderThread(FRHICommandListImmediate& RHICmdList);
	void DrawEmulatedQuadLayers_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView);

	/** TStereoLayerManager<FOpenXRLayer> */
	void UpdateLayer(FOpenXRLayer& ManagerLayer, uint32 LayerId, bool bIsValid) override;

public:
	/** IXRTrackingSystem interface */
	virtual bool DoesSupportLateProjectionUpdate() const override { return true; }
	virtual FString GetVersionString() const override;
	virtual bool HasValidTrackingPosition() override { return IsTracking(HMDDeviceId); }

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override { return true; }
	virtual bool DoesSupportPositionalTracking() const override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual FName GetHMDName() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual float GetPixelDenity() const override;
	virtual void SetPixelDensity(const float NewDensity) override;
	virtual FIntPoint GetIdealRenderTargetSize() const override;
	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override { return false; }
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const override;
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const override;
	virtual bool HasHiddenAreaMesh() const override final;
	virtual bool HasVisibleAreaMesh() const override final;
	virtual void DrawHiddenAreaMesh(class FRHICommandList& RHICmdList, int32 ViewIndex) const override final;
	virtual void DrawVisibleAreaMesh(class FRHICommandList& RHICmdList, int32 ViewIndex) const override final;
	virtual void OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual void OnBeginRendering_GameThread() override;
	virtual void OnLateUpdateApplied_RenderThread(FRHICommandListImmediate& RHICmdList, const FTransform& NewRelativeTransform) override;
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	virtual EHMDWornState::Type GetHMDWornState() override { return bIsReady ? EHMDWornState::Worn : EHMDWornState::NotWorn; }

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void SetFinalViewRect(FRHICommandListImmediate& RHICmdList, const int32 StereoViewIndex, const FIntRect& FinalViewRect) override;
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override;
	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const override;
	virtual uint32 GetLODViewIndex() const override;
	virtual bool IsStandaloneStereoOnlyDevice() const override { return bIsStandaloneStereoOnlyDevice; }

	virtual FMatrix GetStereoProjectionMatrix(const int32 StereoViewIndex) const override;
	virtual void GetEyeRenderParams_RenderThread(const struct FHeadMountedDisplayPassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override;
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture* BackBuffer, class FRHITexture* SrcTexture, FVector2D WindowSize) const override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override; // for non-face locked compositing
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	/** FHMDSceneViewExtension interface */
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	/** IStereoRenderTargetManager */
	virtual bool ShouldUseSeparateRenderTarget() const override { return IsStereoEnabled() && RenderBridge.IsValid(); }
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<IPooledRenderTarget>& DepthTarget) override final { return bNeedReAllocatedDepth; }
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override final;
	virtual EPixelFormat GetActualColorSwapchainFormat() const override { return static_cast<EPixelFormat>(LastActualColorSwapchainFormat); }

	/** FXRRenderTargetManager */
	virtual FXRRenderBridge* GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget) override;

	/** IXRTrackingSystem */
	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;

	/** IStereoLayers */
	virtual bool ShouldCopyDebugLayersToSpectatorScreen() const override { return true; }

	/** IOpenXRExtensionPluginDelegates */
public:
	virtual FApplyHapticFeedbackAddChainStructsDelegate& GetApplyHapticFeedbackAddChainStructsDelegate() override { return ApplyHapticFeedbackAddChainStructsDelegate; }
private:
	FApplyHapticFeedbackAddChainStructsDelegate ApplyHapticFeedbackAddChainStructsDelegate;

public:
	/** Constructor */
	FOpenXRHMD(const FAutoRegister&, XrInstance InInstance, XrSystemId InSystem, TRefCountPtr<FOpenXRRenderBridge>& InRenderBridge, TArray<const char*> InEnabledExtensions, TArray<class IOpenXRExtensionPlugin*> InExtensionPlugins, IARSystemSupport* ARSystemSupport);

	void SetInputModule(IOpenXRInputModule* InInputModule)
	{
		InputModule = InInputModule;
	}

	/** Destructor */
	virtual ~FOpenXRHMD();

	void OnBeginSimulation_GameThread();
	void OnBeginRendering_RHIThread(const FPipelinedFrameState& InFrameState, FXRSwapChainPtr ColorSwapchain, FXRSwapChainPtr DepthSwapchain);
	void OnFinishRendering_RHIThread();

	/** @return	True if the HMD was initialized OK */
	OPENXRHMD_API bool IsInitialized() const;
	OPENXRHMD_API bool IsRunning() const;
	OPENXRHMD_API bool IsFocused() const;

	OPENXRHMD_API int32 AddActionDevice(XrAction Action, XrPath Path);
	OPENXRHMD_API void ResetActionDevices();
	OPENXRHMD_API XrPath GetTrackedDevicePath(const int32 DeviceId);
	OPENXRHMD_API XrSpace GetTrackedDeviceSpace(const int32 DeviceId);

	OPENXRHMD_API bool IsExtensionEnabled(const FString& Name) const { return EnabledExtensions.Contains(Name); }
	OPENXRHMD_API XrInstance GetInstance() { return Instance; }
	OPENXRHMD_API XrSystemId GetSystem() { return System; }
	OPENXRHMD_API XrSession GetSession() { return Session; }
	OPENXRHMD_API XrTime GetDisplayTime() const;
	OPENXRHMD_API XrSpace GetTrackingSpace() const;
	OPENXRHMD_API TArray<IOpenXRExtensionPlugin*>& GetExtensionPlugins() { return ExtensionPlugins; }

private:
	bool					bStereoEnabled;
	TAtomic<bool>			bIsRunning;
	TAtomic<bool>			bIsReady;
	TAtomic<bool>			bIsRendering;
	TAtomic<bool>			bIsSynchronized;
	bool					bShouldWait;
	bool					bIsExitingSessionByxrRequestExitSession;
	bool					bDepthExtensionSupported;
	bool					bHiddenAreaMaskSupported;
	bool					bViewConfigurationFovSupported;
	bool					bNeedReAllocatedDepth;
	bool					bNeedReBuildOcclusionMesh;
	bool					bIsMobileMultiViewEnabled;
	bool					bSupportsHandTracking;
	bool					bSpaceAccellerationSupported;
	bool					bProjectionLayerAlphaEnabled;
	bool					bIsStandaloneStereoOnlyDevice;
	float					WorldToMetersScale = 100.0f;
	float					RuntimePixelDensityMax = FHeadMountedDisplayBase::PixelDensityMax;

	XrSessionState			CurrentSessionState;
	FRWLock					SessionHandleMutex;

	TArray<const char*>		EnabledExtensions;
	IOpenXRInputModule*		InputModule;
	TArray<class IOpenXRExtensionPlugin*> ExtensionPlugins;
	XrInstance				Instance;
	XrSystemId				System;
	XrSession				Session;
	XrSpace					LocalSpace;
	XrSpace					StageSpace;
	XrSpace					CustomSpace;
	XrReferenceSpaceType	TrackingSpaceType;
	XrViewConfigurationType SelectedViewConfigurationType;
	XrEnvironmentBlendMode  SelectedEnvironmentBlendMode;
	XrInstanceProperties    InstanceProperties;
	XrSystemProperties      SystemProperties;

	FPipelinedFrameState	PipelinedFrameStateGame;
	FPipelinedFrameState	PipelinedFrameStateRendering;
	FPipelinedFrameState	PipelinedFrameStateRHI;

	FPipelinedLayerState	PipelinedLayerStateRendering;
	FPipelinedLayerState	PipelinedLayerStateRHI;

	mutable FRWLock			DeviceMutex;
	TArray<FDeviceSpace>	DeviceSpaces;

	TRefCountPtr<FOpenXRRenderBridge> RenderBridge;
	IRendererModule*		RendererModule;

	uint8					LastRequestedColorSwapchainFormat;
	uint8					LastActualColorSwapchainFormat;
	uint8					LastRequestedDepthSwapchainFormat;

	TArray<FHMDViewMesh>	HiddenAreaMeshes;
	TArray<FHMDViewMesh>	VisibleAreaMeshes;

	bool					bTrackingSpaceInvalid;
	bool					bUseCustomReferenceSpace;
	FQuat					BaseOrientation;
	FVector					BasePosition;

	bool					bNativeWorldQuadLayerSupport;
	TArray<IStereoLayers::FLayerDesc> EmulatedSceneLayers;
	TArray<FOpenXRLayer>			  NativeQuadLayers;
};
