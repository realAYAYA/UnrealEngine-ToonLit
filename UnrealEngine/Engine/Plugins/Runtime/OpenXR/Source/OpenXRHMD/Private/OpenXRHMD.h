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
#include "IOpenXRHMD.h"
#include "Misc/EnumClassFlags.h"

#include <openxr/openxr.h>

class APlayerController;
class FSceneView;
class FSceneViewFamily;
class FFBFoveationImageGenerator;
class FOpenXRSwapchain;
class UCanvas;
class FOpenXRRenderBridge;
class IOpenXRInputModule;
struct FDefaultStereoLayers_LayerRenderParams;
union FXrCompositionLayerUnion;

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
	, public IOpenXRHMD
{
private:

public:
	class FDeviceSpace
	{
	public:
		FDeviceSpace(XrAction InAction, XrPath InPath);
		FDeviceSpace(XrAction InAction, XrPath InPath, XrPath InSubactionPath);
		~FDeviceSpace();

		bool CreateSpace(XrSession InSession);
		void DestroySpace();

		XrAction Action;
		XrSpace Space;
		XrPath Path;
		XrPath SubactionPath;
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

	// The game and render threads each have a separate copy of these structures so that they don't stomp on each other or cause tearing
	// when the game thread progresses to the next frame while the render thread is still working on the previous frame.
	struct FPipelinedFrameState
	{
		XrFrameState FrameState{XR_TYPE_FRAME_STATE};
		XrViewState ViewState{XR_TYPE_VIEW_STATE};
		TArray<XrView> Views;
		TArray<XrViewConfigurationView> ViewConfigs;
		TArray<XrSpaceLocation> DeviceLocations;
		TSharedPtr<FTrackingSpace> TrackingSpace;
		float WorldToMetersScale = 100.0f;
		float PixelDensity = 1.0f;
		int WaitCount = 0;
		int BeginCount = 0;
		int EndCount = 0;
		bool bXrFrameStateUpdated = false;
	};

	struct FEmulatedLayerState
	{
		// These layers are used as a target to composite all the emulated face locked layers into
		// and be sent to the compositor with VIEW tracking space to avoid reprojection.
		TArray<XrCompositionLayerProjectionView> CompositedProjectionLayers;
		TArray<XrSwapchainSubImage> EmulationImages;
		// This swapchain is where the emulated face locked layers are rendered into.
		FXRSwapChainPtr EmulationSwapchain;
	};

	struct FBasePassLayerBlendParameters
	{
		// Default constructor inverts the alpha for color blending to make up for the fact that UE uses
		// alpha = 0 for opaque and alpha = 1 for transparent while OpenXR does the opposite.
		// Alpha blending passes through the destination alpha instead.
		FBasePassLayerBlendParameters()
		{
			srcFactorColor = XR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA_FB;
			dstFactorColor = XR_BLEND_FACTOR_SRC_ALPHA_FB;
			srcFactorAlpha = XR_BLEND_FACTOR_ZERO_FB;
			dstFactorAlpha = XR_BLEND_FACTOR_ONE_FB;
		}
		
		XrBlendFactorFB	srcFactorColor;
		XrBlendFactorFB	dstFactorColor;
		XrBlendFactorFB	srcFactorAlpha;
		XrBlendFactorFB	dstFactorAlpha;
	};

	struct FLayerColorScaleAndBias
	{
		// Used by XR_KHR_composition_layer_color_scale_bias to apply a color multiplier and offset to the background layer
		// and set via UHeadMountedDisplayFunctionLibrary::SetHMDColorScaleAndBias() --> OpenXRHMD::SetColorScaleAndBias()
		XrColor4f ColorScale;
		XrColor4f ColorBias;
	};

	enum class EOpenXRLayerStateFlags : uint32
	{
		None = 0u,
		BackgroundLayerVisible = (1u << 0),
		SubmitBackgroundLayer = (1u << 1),
		SubmitDepthLayer = (1u << 2),
		SubmitEmulatedFaceLockedLayer = (1u << 3),
	};
	FRIEND_ENUM_CLASS_FLAGS(EOpenXRLayerStateFlags);

	struct FPipelinedLayerState
	{
		TArray<FXrCompositionLayerUnion> NativeOverlays;
		TArray<XrCompositionLayerProjectionView> ProjectionLayers;
		TArray<XrCompositionLayerDepthInfoKHR> DepthLayers;

		TArray<XrSwapchainSubImage> ColorImages;
		TArray<XrSwapchainSubImage> DepthImages;

		FXRSwapChainPtr ColorSwapchain;
		FXRSwapChainPtr DepthSwapchain;
		TArray<FXRSwapChainPtr> NativeOverlaySwapchains;

		FEmulatedLayerState EmulatedLayerState;

		EOpenXRLayerStateFlags LayerStateFlags = EOpenXRLayerStateFlags::None;
		FBasePassLayerBlendParameters BasePassLayerBlendParams;
		FLayerColorScaleAndBias LayerColorScaleAndBias;
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

	virtual bool GetIsTracked(int32 DeviceId) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual bool GetPoseForTime(int32 DeviceId, FTimespan Timespan, bool& OutTimeWasUsed, FQuat& CurrentOrientation, FVector& CurrentPosition, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityAsAxisAndLength, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration, float WorldToMetersScale) override;
	virtual bool GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile) override;
	
	virtual void SetBaseRotation(const FRotator& InBaseRotation) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& InBaseOrientation) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual void SetBasePosition(const FVector& InBasePosition) override;
	virtual FVector GetBasePosition() const override;

	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override;

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
	enum ETextureCopyBlendModifier : uint8;

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

	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, 
								  bool bClearBlack, ERenderTargetActions RTAction, ERHIAccess FinalDstAccess, ETextureCopyBlendModifier SrcTextureCopyModifier) const;
	
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, const FXRSwapChainPtr& DstSwapChain, FIntRect DstRect, bool bClearBlack, ETextureCopyBlendModifier SrcTextureCopyModifier) const;

	void AllocateDepthTextureInternal(uint32 SizeX, uint32 SizeY, uint32 NumSamples, uint32 ArraySize);

	void SetupFrameLayers_RenderThread(FRHICommandListImmediate& RHICmdList);
	void DrawEmulatedLayers_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView);
	void DrawBackgroundCompositedEmulatedLayers_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView);
	void DrawEmulatedFaceLockedLayers_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView);

	/** TStereoLayerManager<FOpenXRLayer> */
	void UpdateLayer(FOpenXRLayer& ManagerLayer, uint32 LayerId, bool bIsValid) override;

public:
	/** IXRTrackingSystem interface */
	virtual bool DoesSupportLateProjectionUpdate() const override { return true; }
	virtual FString GetVersionString() const override;
	virtual bool HasValidTrackingPosition() override { return IsTracking(HMDDeviceId); }
	virtual IOpenXRHMD* GetIOpenXRHMD() { return this; }

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override;
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
	virtual bool SetColorScaleAndBias(FLinearColor ColorScale, FLinearColor ColorBias);

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
	virtual bool AllocateRenderTargetTextures(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumLayers, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, TArray<FTexture2DRHIRef>& OutTargetableTextures, TArray<FTexture2DRHIRef>& OutShaderResourceTextures, uint32 NumSamples = 1) override;
	virtual int32 AcquireColorTexture() override final;
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override final;
	virtual bool ReconfigureForShaderPlatform(EShaderPlatform NewShaderPlatform) override;
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
	FOpenXRHMD(const FAutoRegister&, XrInstance InInstance, TRefCountPtr<FOpenXRRenderBridge>& InRenderBridge, TArray<const char*> InEnabledExtensions, TArray<class IOpenXRExtensionPlugin*> InExtensionPlugins, IARSystemSupport* ARSystemSupport);


	/** Destructor */
	virtual ~FOpenXRHMD();

	void OnBeginSimulation_GameThread();
	void OnBeginRendering_RHIThread(const FPipelinedFrameState& InFrameState, FXRSwapChainPtr ColorSwapchain, FXRSwapChainPtr DepthSwapchain, FXRSwapChainPtr EmulationSwapchain);
	void OnFinishRendering_RHIThread();

	/** IOpenXRHMD */
	void SetInputModule(IOpenXRInputModule* InInputModule) override
	{
		InputModule = InInputModule;
	}
	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const override;
	bool IsRunning() const override;
	bool IsFocused() const override;
	int32 AddTrackedDevice(XrAction Action, XrPath Path) override;
	int32 AddTrackedDevice(XrAction Action, XrPath Path, XrPath SubActionPath) override;
	void ResetTrackedDevices() override;
	XrPath GetTrackedDevicePath(const int32 DeviceId) override;
	XrSpace GetTrackedDeviceSpace(const int32 DeviceId) override;
	bool IsExtensionEnabled(const FString& Name) const override { return EnabledExtensions.Contains(Name); }
	XrInstance GetInstance() override { return Instance; }
	XrSystemId GetSystem() override { return System; }
	XrSession GetSession() override { return Session; }
	XrTime GetDisplayTime() const override;
	XrSpace GetTrackingSpace() const override;
	IOpenXRExtensionPluginDelegates& GetIOpenXRExtensionPluginDelegates() override { return *this; }
	TArray<IOpenXRExtensionPlugin*>& GetExtensionPlugins() override { return ExtensionPlugins; }
	
	OPENXRHMD_API void SetEnvironmentBlendMode(XrEnvironmentBlendMode NewBlendMode);

	/** Returns shader platform the plugin is currently configured for, in the editor it can change due to preview platforms. */
	EShaderPlatform GetConfiguredShaderPlatform() const { check(ConfiguredShaderPlatform != EShaderPlatform::SP_NumPlatforms); return ConfiguredShaderPlatform; }
	FOpenXRSwapchain* GetColorSwapchain_RenderThread();
private:

	TArray<XrEnvironmentBlendMode> RetrieveEnvironmentBlendModes() const;
	FDefaultStereoLayers_LayerRenderParams CalculateEmulatedLayerRenderParams(const FSceneView& InView);
	FRHIRenderPassInfo SetupEmulatedLayersRenderPass(FRHICommandListImmediate& RHICmdList, const FSceneView& InView, TArray<IStereoLayers::FLayerDesc>& Layers, FTexture2DRHIRef RenderTarget, FDefaultStereoLayers_LayerRenderParams& OutRenderParams);
	bool IsEmulatingStereoLayers();
	
	void UpdateLayerSwapchainTexture(const FOpenXRLayer& Layer, FRHICommandListImmediate& RHICmdList);
	void ConfigureLayerSwapchain(FOpenXRLayer& Layer, TArray<FOpenXRLayer>& BackupLayers);
	void AddLayersToHeaders(TArray<const XrCompositionLayerBaseHeader*>& Headers);

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
	bool					bNeedReBuildOcclusionMesh;
	bool					bIsMobileMultiViewEnabled;
	bool					bSupportsHandTracking;
	bool					bSpaceAccelerationSupported;
	bool					bProjectionLayerAlphaEnabled;
	bool					bIsStandaloneStereoOnlyDevice;
	bool					bIsTrackingOnlySession;
	bool					bIsAcquireOnAnyThreadSupported;
	bool					bUseWaitCountToAvoidExtraXrBeginFrameCalls;
	float					WorldToMetersScale = 100.0f;
	float					RuntimePixelDensityMax = FHeadMountedDisplayBase::PixelDensityMax;
	EShaderPlatform			ConfiguredShaderPlatform = EShaderPlatform::SP_NumPlatforms;

	XrSessionState			CurrentSessionState;
	FRWLock					SessionHandleMutex;

	TArray<const char*>		EnabledExtensions;
	IOpenXRInputModule*		InputModule;
	TArray<class IOpenXRExtensionPlugin*> ExtensionPlugins;
	XrInstance				Instance;
	XrSystemId				System;
	XrSession				Session;
	XrSpace					LocalSpace;
	XrSpace					LocalFloorSpace;
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

	bool					bLayerSupportOpenXRCompliant;
	bool					bOpenXRInvertAlphaCvarCachedValue;
	bool					bOpenXRForceStereoLayersEmulationCVarCachedValue;
	TArray<IStereoLayers::FLayerDesc> BackgroundCompositedEmulatedLayers;
	TArray<IStereoLayers::FLayerDesc> EmulatedFaceLockedLayers;
	TArray<FOpenXRLayer>			  NativeLayers;

	TUniquePtr<FFBFoveationImageGenerator> FBFoveationImageGenerator;
	bool					bFoveationExtensionSupported;
	bool					bRuntimeFoveationSupported;
	bool					bLocalFloorExtensionSupported;

	XrColor4f				LayerColorScale;
	XrColor4f				LayerColorBias;
	bool					bCompositionLayerColorScaleBiasSupported;
};

ENUM_CLASS_FLAGS(FOpenXRHMD::EOpenXRLayerStateFlags);