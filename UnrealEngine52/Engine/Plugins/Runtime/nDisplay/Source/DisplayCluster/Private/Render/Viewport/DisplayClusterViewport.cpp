// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "EngineUtils.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "UnrealClient.h"

#include "DisplayClusterSceneViewExtensions.h"

#include "Misc/DisplayClusterLog.h"

int32 GDisplayClusterMultiGPUEnable = 1;
static FAutoConsoleVariableRef CVarDisplayClusterMultiGPUEnable(
	TEXT("DC.MultiGPU"),
	GDisplayClusterMultiGPUEnable,
	TEXT("Enable MultiGPU for Display Cluster rendering.  Useful to disable for debugging.  (Default = 1)"),
	ECVF_Default
);

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewport::FDisplayClusterViewport(FDisplayClusterViewportManager& InOwner, const FString& InClusterNodeId, const FString& InViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
	: UninitializedProjectionPolicy(InProjectionPolicy)
	, ViewportId(InViewportId)
	, ClusterNodeId(InClusterNodeId)
	, Owner(InOwner)
{
	check(!ClusterNodeId.IsEmpty());
	check(!ViewportId.IsEmpty());
	check(UninitializedProjectionPolicy.IsValid());

	// Create scene proxy pair with on game thread. Outside, in ViewportManager added to proxy array on render thread
	ViewportProxy = MakeShared<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>(*this);

	// Add viewport proxy on renderthread
	ENQUEUE_RENDER_COMMAND(CreateDisplayClusterViewportProxy)(
		[ViewportManagerProxy = Owner.GetViewportManagerProxy(), ViewportProxy = ViewportProxy](FRHICommandListImmediate& RHICmdList)
		{
			ViewportManagerProxy->CreateViewport_RenderThread(ViewportProxy);
		}
	);
}

FDisplayClusterViewport::~FDisplayClusterViewport()
{
	// Remove viewport proxy on render_thread
	ENQUEUE_RENDER_COMMAND(DeleteDisplayClusterViewportProxy)(
		[ViewportManagerProxy = Owner.GetViewportManagerProxy(), ViewportProxy = ViewportProxy](FRHICommandListImmediate& RHICmdList)
		{
			ViewportManagerProxy->DeleteViewport_RenderThread(ViewportProxy);
		}
	);

	ViewportProxy.Reset();
	OpenColorIO.Reset();

	// Handle projection policy EndScene event
	HandleEndScene();

	// Handle projection policy event
	ProjectionPolicy.Reset();
	UninitializedProjectionPolicy.Reset();

	// Reset RTT size after viewport delete
	Owner.ResetSceneRenderTargetSize();
}

IDisplayClusterViewportManager& FDisplayClusterViewport::GetOwner() const
{
	return Owner;
}

const FDisplayClusterRenderFrameSettings& FDisplayClusterViewport::GetRenderFrameSettings() const
{
	return Owner.GetRenderFrameSettings();
}

bool FDisplayClusterViewport::IsOpenColorIOEquals(const FDisplayClusterViewport& InViewport) const
{
	bool bEnabledOCIO_1 = OpenColorIO.IsValid();
	bool bEnabledOCIO_2 = InViewport.OpenColorIO.IsValid();

	if (bEnabledOCIO_1 == bEnabledOCIO_2)
	{
		if (!bEnabledOCIO_1)
		{
			// Both OCIO disabled
			return true;
		}

		if (OpenColorIO->IsConversionSettingsEqual(InViewport.OpenColorIO->GetConversionSettings()))
		{
			return true;
		}
	}

	return false;
}

const TArray<FSceneViewExtensionRef> FDisplayClusterViewport::GatherActiveExtensions(FViewport* InViewport) const
{
	// Use VE from engine for default render and MRQ:
	switch (RenderSettings.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Default:
	case EDisplayClusterViewportCaptureMode::MoviePipeline:
		if (InViewport)
		{
			FDisplayClusterSceneViewExtensionContext ViewExtensionContext(InViewport, &Owner, GetId());
			return GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
		}
		else
		{
			UWorld* CurrentWorld = Owner.GetCurrentWorld();
			if (CurrentWorld)
			{
				FDisplayClusterSceneViewExtensionContext ViewExtensionContext(CurrentWorld->Scene, &Owner, GetId());
				return GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
			}
		}

		// No extension found.
		return TArray<FSceneViewExtensionRef>();
		
	default:
		break;
	}

	// Get custom VE:
	TArray<FSceneViewExtensionRef> OutCustomExtensions;

	// Initialize custom extensions:
	switch (RenderSettings.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
	case EDisplayClusterViewportCaptureMode::Lightcard:
	{
		// Chromakey and LightCard use only nDisplay VE for callback purposes (preserve alpha channel, etc).
		TSharedPtr<FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe> ViewportManagerViewExtension = Owner.GetViewportManagerViewExtension();
		if (ViewportManagerViewExtension.IsValid())
		{
			OutCustomExtensions.Add(ViewportManagerViewExtension->AsShared());
		}
	}
	break;

	default:
		break;
	}

	// Sort extensions in order of priority (copied from FSceneViewExtensions::GatherActiveExtensions)
	struct SortPriority
	{
		bool operator () (const FSceneViewExtensionRef& A, const FSceneViewExtensionRef& B) const
		{
			return A->GetPriority() > B->GetPriority();
		}
	};
	Sort(OutCustomExtensions.GetData(), OutCustomExtensions.Num(), SortPriority());

	return OutCustomExtensions;
}

bool FDisplayClusterViewport::HandleStartScene()
{
	bool bResult = true;
	if (Owner.IsSceneOpened())
	{
		if (UninitializedProjectionPolicy.IsValid())
		{
			bResult = UninitializedProjectionPolicy->HandleStartScene(this);
			if (bResult)
			{
				ProjectionPolicy = UninitializedProjectionPolicy;
				UninitializedProjectionPolicy.Reset();
			}
		}
		else 
		{
			// Already Initialized
			if (!ProjectionPolicy.IsValid())
			{
				// No projection policy for this viewport
				UE_LOG(LogDisplayClusterViewport, Error, TEXT("No projection policy assigned for Viewports '%s'."), *GetId());
			}
		}
	}

	return bResult;
}

void FDisplayClusterViewport::HandleEndScene()
{
	if (ProjectionPolicy.IsValid())
	{
		ProjectionPolicy->HandleEndScene(this);
		UninitializedProjectionPolicy = ProjectionPolicy;
		ProjectionPolicy.Reset();
	}

#if WITH_EDITOR
	CleanupViewState();
#endif
}

void FDisplayClusterViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITOR
	// ViewStates released on rendering thread from viewport proxy object
#endif
}

bool FDisplayClusterViewport::ShouldUseAdditionalTargetableResource() const
{
	check(IsInGameThread());

	// PostRender Blur require additional RTT for shader
	if (PostRenderSettings.PostprocessBlur.IsEnabled())
	{
		return true;
	}

	// Supoport projection policy additional resource
	if (ProjectionPolicy.IsValid() && ProjectionPolicy->ShouldUseAdditionalTargetableResource())
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::ShouldUseAdditionalFrameTargetableResource() const
{
	if (ViewportRemap.IsUsed())
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewport::ShouldUseFullSizeFrameTargetableResource() const
{
	if (ViewportRemap.IsUsed())
	{
		return true;
	}

	return false;
}

void FDisplayClusterViewport::SetupSceneView(uint32 ContextNum, class UWorld* World, FSceneViewFamily& InOutViewFamily, FSceneView& InOutView) const
{
	check(IsInGameThread());
	check(Contexts.IsValidIndex(ContextNum));

	// MRQ only uses viewport visibility settings
	if(RenderSettings.CaptureMode == EDisplayClusterViewportCaptureMode::MoviePipeline)
	{
		// Apply visibility settigns to view
		VisibilitySettings.SetupSceneView(World, InOutView);

		return;
	}

	if (OpenColorIO.IsValid())
	{
		OpenColorIO->SetupSceneView(InOutViewFamily, InOutView);
	}

	if(Contexts[ContextNum].GPUIndex >= 0)
	{
		// Use custom GPUIndex for render
		InOutView.bOverrideGPUMask = true;
		InOutView.GPUMask = FRHIGPUMask::FromIndex((uint32)Contexts[ContextNum].GPUIndex);
	}

	if (Contexts[ContextNum].bOverrideCrossGPUTransfer || !RenderSettings.bEnableCrossGPUTransfer)
	{
		// Disable native cross-GPU transfers inside Renderer.
		InOutView.bAllowCrossGPUTransfer = false;
	}

	// Disable raytracing for lightcard and chromakey
#if RHI_RAYTRACING
	switch (RenderSettings.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
	case EDisplayClusterViewportCaptureMode::Lightcard:
		InOutView.bAllowRayTracing = false;
		break;

	default:
		break;
	}
#endif // RHI_RAYTRACING

	// Apply visibility settigns to view
	VisibilitySettings.SetupSceneView(World, InOutView);

	// Handle Motion blur parameters
	CameraMotionBlur.SetupSceneView(Contexts[ContextNum], InOutView);
}

inline void AdjustRect(FIntRect& InOutRect, const float multX, const float multY)
{
	InOutRect.Min.X *= multX;
	InOutRect.Max.X *= multX;
	InOutRect.Min.Y *= multY;
	InOutRect.Max.Y *= multY;
}

FIntRect FDisplayClusterViewport::GetValidRect(const FIntRect& InRect, const TCHAR* DbgSourceName)
{
	// The target always needs be within GMaxTextureDimensions, larger dimensions are not supported by the engine
	const int32 MaxTextureSize = DisplayClusterViewportHelpers::GetMaxTextureDimension();
	const int32 MinTextureSize = DisplayClusterViewportHelpers::GetMinTextureDimension();

	int32 Width  = FMath::Max(MinTextureSize, InRect.Width());
	int32 Height = FMath::Max(MinTextureSize, InRect.Height());

	FIntRect OutRect(InRect.Min, InRect.Min + FIntPoint(Width, Height));

	float RectScale = 1;

	// Make sure the rect doesn't exceed the maximum resolution, and preserve its aspect ratio if it needs to be clamped
	int32 RectMaxSize = OutRect.Max.GetMax();
	if (RectMaxSize > MaxTextureSize)
	{
		RectScale = float(MaxTextureSize) / RectMaxSize;
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("The viewport '%s' rect '%s' size %dx%d clamped: max texture dimensions is %d"), *GetId(), (DbgSourceName==nullptr) ? TEXT("none") : DbgSourceName, InRect.Max.X, InRect.Max.Y, MaxTextureSize);
	}

	OutRect.Min.X = FMath::Min(OutRect.Min.X, MaxTextureSize);
	OutRect.Min.Y = FMath::Min(OutRect.Min.Y, MaxTextureSize);

	const FIntPoint ScaledRectMax = DisplayClusterViewportHelpers::ScaleTextureSize(OutRect.Max, RectScale);

	OutRect.Max.X = FMath::Clamp(ScaledRectMax.X, OutRect.Min.X, MaxTextureSize);
	OutRect.Max.Y = FMath::Clamp(ScaledRectMax.Y, OutRect.Min.Y, MaxTextureSize);

	return OutRect;
}

float FDisplayClusterViewport::GetClusterRenderTargetRatioMult(const FDisplayClusterRenderFrameSettings& InFrameSettings) const
{
	float ClusterRenderTargetRatioMult = InFrameSettings.ClusterRenderTargetRatioMult;

	// Support Outer viewport cluster rtt multiplier
	if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target))
	{
		ClusterRenderTargetRatioMult *= InFrameSettings.ClusterICVFXOuterViewportRenderTargetRatioMult;
	}
	else if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera))
	{
		ClusterRenderTargetRatioMult *= InFrameSettings.ClusterICVFXInnerViewportRenderTargetRatioMult;
	}

	// Cluster mult downscale in range 0..1
	return FMath::Clamp(ClusterRenderTargetRatioMult, 0.f, 1.f);
}

FIntPoint FDisplayClusterViewport::GetDesiredContextSize(const FIntPoint& InSize, const FDisplayClusterRenderFrameSettings& InFrameSettings) const
{
	const float ClusterRenderTargetRatioMult = GetClusterRenderTargetRatioMult(InFrameSettings);

	// Check size multipliers in order bellow:
	const float RenderTargetAdaptRatio = DisplayClusterViewportHelpers::GetValidSizeMultiplier(InSize, RenderSettings.RenderTargetAdaptRatio, ClusterRenderTargetRatioMult * RenderSettings.RenderTargetRatio);
	const float RenderTargetRatio = DisplayClusterViewportHelpers::GetValidSizeMultiplier(InSize, RenderSettings.RenderTargetRatio, ClusterRenderTargetRatioMult * RenderTargetAdaptRatio);
	const float ClusterMult = DisplayClusterViewportHelpers::GetValidSizeMultiplier(InSize, ClusterRenderTargetRatioMult, RenderTargetRatio * RenderTargetAdaptRatio);

	FIntPoint DesiredContextSize = DisplayClusterViewportHelpers::ScaleTextureSize(InSize, FMath::Max(RenderTargetAdaptRatio * RenderTargetRatio * ClusterMult, 0.f));

	const int32 MaxTextureSize = DisplayClusterViewportHelpers::GetMaxTextureDimension();
	DesiredContextSize.X = FMath::Min(DesiredContextSize.X, MaxTextureSize);
	DesiredContextSize.Y = FMath::Min(DesiredContextSize.Y, MaxTextureSize);

	return DesiredContextSize;
}

float FDisplayClusterViewport::GetCustomBufferRatio(const FDisplayClusterRenderFrameSettings& InFrameSettings) const
{
	float CustomBufferRatio = RenderSettings.BufferRatio;

	// Global multiplier
	CustomBufferRatio *= InFrameSettings.ClusterBufferRatioMult;

	if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target))
	{
		// Outer viewport
		CustomBufferRatio *= InFrameSettings.ClusterICVFXOuterViewportBufferRatioMult;
	}
	else if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera))
	{
		// Inner Frustum
		CustomBufferRatio *= InFrameSettings.ClusterICVFXInnerFrustumBufferRatioMult;
	}

	return CustomBufferRatio;
}

void FDisplayClusterViewport::ResetFrameContexts()
{

#if WITH_EDITOR
	OutputPreviewTargetableResource.SafeRelease();
#endif

	// Discard resources that are not used in frame composition
	RenderTargets.Empty();
	OutputFrameTargetableResources.Empty();
	AdditionalFrameTargetableResources.Empty();

	// Release old contexts
	Contexts.Empty();

	// Free internal resources
	InputShaderResources.Empty();
	AdditionalTargetableResources.Empty();
	MipsShaderResources.Empty();
}

bool FDisplayClusterViewport::UpdateFrameContexts(const uint32 InStereoViewIndex, const FDisplayClusterRenderFrameSettings& InFrameSettings)
{
	check(IsInGameThread());

	const uint32 FrameTargetsAmount = Owner.GetViewPerViewportAmount();

	FIntRect     DesiredFrameTargetRect = RenderSettings.Rect;
	{
		switch (InFrameSettings.RenderMode)
		{
		case EDisplayClusterRenderFrameMode::SideBySide:
			AdjustRect(DesiredFrameTargetRect, 0.5f, 1.f);
			break;
		case EDisplayClusterRenderFrameMode::TopBottom:
			AdjustRect(DesiredFrameTargetRect, 1.f, 0.5f);
			break;
		case EDisplayClusterRenderFrameMode::PreviewInScene:
		{
			// Preview downscale in range 0..1
			float MultXY = FMath::Clamp(InFrameSettings.PreviewRenderTargetRatioMult, 0.f, 1.f);
			AdjustRect(DesiredFrameTargetRect, MultXY, MultXY);

			// Align each frame to zero
			DesiredFrameTargetRect = FIntRect(FIntPoint(0, 0), DesiredFrameTargetRect.Size());

			break;
		}
		default:
			break;
		}
	}

	// Special case mono->stereo
	const uint32 ViewportContextAmount = RenderSettings.bForceMono ? 1 : FrameTargetsAmount;

#if WITH_EDITOR
	OutputPreviewTargetableResource.SafeRelease();
#endif

	// Discard resources that are not used in frame composition
	RenderTargets.Empty();
	OutputFrameTargetableResources.Empty();
	AdditionalFrameTargetableResources.Empty();

	// Freeze the image in the viewport only after the frame has been rendered
	if (RenderSettings.bFreezeRendering && RenderSettings.bEnable)
	{
		// Freeze only when all resources valid
		if (Contexts.Num() > 0 && Contexts.Num() == InputShaderResources.Num())
		{
			DisplayClusterViewportHelpers::FreezeRenderingOfViewportTextureResources(InputShaderResources);
			DisplayClusterViewportHelpers::FreezeRenderingOfViewportTextureResources(AdditionalTargetableResources);
			DisplayClusterViewportHelpers::FreezeRenderingOfViewportTextureResources(MipsShaderResources);

			// Update context links for freezed viewport
			for (FDisplayClusterViewport_Context& ContextIt : Contexts)
			{
				ContextIt.StereoscopicPass = FDisplayClusterViewportStereoscopicPass::EncodeStereoscopicPass(ContextIt.ContextNum, ViewportContextAmount, InFrameSettings);
				ContextIt.StereoViewIndex = (int32)(InStereoViewIndex + ContextIt.ContextNum);
				ContextIt.bDisableRender = true;
				ContextIt.FrameTargetRect = GetValidRect(DesiredFrameTargetRect, TEXT("Context Frame"));
			}

			return true;
		}
	}

	// Release old contexts
	Contexts.Empty();

	// Free internal resources
	InputShaderResources.Empty();
	AdditionalTargetableResources.Empty();
	MipsShaderResources.Empty();

	if (RenderSettings.bEnable == false)
	{
		// Exclude this viewport from render and logic, but object still exist
		return false;
	}

	if (PostRenderSettings.GenerateMips.IsEnabled())
	{
		//Check if current projection policy supports this feature
		if (!ProjectionPolicy.IsValid() || !ProjectionPolicy->ShouldUseSourceTextureWithMips())
		{
			// Don't create unused mips texture
			PostRenderSettings.GenerateMips.Reset();
		}
	}

	// Make sure the frame target rect doesn't exceed the maximum resolution, and preserve its aspect ratio if it needs to be clamped
	FIntRect FrameTargetRect = GetValidRect(DesiredFrameTargetRect, TEXT("Context Frame"));

	// Exclude zero-size viewports from render
	if (FrameTargetRect.Size().GetMin() <= 0)
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("The viewport '%s' FrameTarget rect has zero size %dx%d: Disabled"), *GetId(), FrameTargetRect.Size().X, FrameTargetRect.Size().Y);
		return false;
	}

	// Scale context for rendering
	FIntPoint DesiredContextSize = GetDesiredContextSize(FrameTargetRect.Size(), InFrameSettings);

#if WITH_EDITOR
	switch (InFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		{
			DesiredContextSize = DisplayClusterViewportHelpers::GetTextureSizeLessThanMax(DesiredContextSize, InFrameSettings.PreviewMaxTextureDimension);
		}
		break;
	default:
		break;
	}
#endif

	// Exclude zero-size viewports from render
	if (DesiredContextSize.GetMin() <= 0)
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("The viewport '%s' RenderTarget rect has zero size %dx%d: Disabled"), *GetId(), DesiredContextSize.X, DesiredContextSize.Y);
		return false;
	}

	// Build RTT rect
	FIntRect RenderTargetRect = FIntRect(FIntPoint(0, 0), DesiredContextSize);

	// Support custom frustum rendering feature
	CustomFrustumRendering.Update(*this, RenderTargetRect);

	FIntPoint ContextSize = RenderTargetRect.Size();

	// Support overscan rendering feature
	OverscanRendering.Update(*this, RenderTargetRect);

	const float BaseCustomBufferRatio = GetCustomBufferRatio(InFrameSettings);

	// Fix buffer ratio value vs MaxTextureSize:
	const float CustomBufferRatio = DisplayClusterViewportHelpers::GetValidSizeMultiplier(RenderTargetRect.Size(), BaseCustomBufferRatio, 1.f);

	// Setup resource usage logic:
	bool bDisableRender = false;
	if (PostRenderSettings.Replace.IsEnabled())
	{
		bDisableRender = true;
	}

	bool bDisableInternalResources = false;
	if (RenderSettings.bSkipRendering)
	{
		bDisableInternalResources = true;
		bDisableRender = true;
	}

	if (RenderSettings.IsViewportOverrided())
	{
		switch (RenderSettings.ViewportOverrideMode)
		{
		case EDisplayClusterViewportOverrideMode::InernalRTT:
			bDisableRender = true;
			break;

		case EDisplayClusterViewportOverrideMode::All:
			bDisableRender = true;
			bDisableInternalResources = true;
			break;

		default:
			break;
		}
	}

	// UV LightCard viewport use unique whole-cluster texture from LC manager
	if (EnumHasAllFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Use external texture from LightCardManager instead of rendering
		bDisableRender = true;

		// Use the UVLightCard viewport only when this type of lightcards has been defined
		bool bUseUVLightCardViewport = false;

		TSharedPtr<FDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe> LightCardManager = Owner.GetLightCardManager();
		if (LightCardManager.IsValid() && LightCardManager->IsUVLightCardEnabled())
		{
			// Custom viewport size from LC Manager
			ContextSize = LightCardManager->GetUVLightCardResourceSize();

			// Size must be not null
			if (ContextSize.GetMin() > 1)
			{
				FrameTargetRect = RenderTargetRect = FIntRect(FIntPoint(0, 0), ContextSize);

				// Allow to use this viewport
				bUseUVLightCardViewport = true;
			}
		}

		if(!bUseUVLightCardViewport)
		{
			// do not use UV LightCard viewport
			return false;
		}
	}

	//Add new contexts
	for (uint32 ContextIt = 0; ContextIt < ViewportContextAmount; ++ContextIt)
	{
		const EStereoscopicPass StereoscopicPass = FDisplayClusterViewportStereoscopicPass::EncodeStereoscopicPass(ContextIt, ViewportContextAmount, InFrameSettings);
		const int32 StereoViewIndex = (int32)(InStereoViewIndex + ContextIt);

		FDisplayClusterViewport_Context Context(ContextIt, StereoscopicPass, StereoViewIndex);

		Context.GPUIndex = INDEX_NONE;

		// The nDisplay can use its own cross-GPU transfer
		if (InFrameSettings.CrossGPUTransfer.bEnable)
		{
			Context.bOverrideCrossGPUTransfer = true;
		}

		const int32 MaxExplicitGPUIndex = GDisplayClusterMultiGPUEnable ? GNumExplicitGPUsForRendering - 1 : 0;

		if (MaxExplicitGPUIndex > 0)
		{
			if (InFrameSettings.bIsRenderingInEditor)
			{
				// Experimental: allow mGPU for preview rendering:
				if (InFrameSettings.bAllowMultiGPURenderingInEditor && !bDisableRender)
				{
					int32 MinGPUIndex = FMath::Min(InFrameSettings.PreviewMinGPUIndex, MaxExplicitGPUIndex);
					int32 MaxGPUIndex = FMath::Min(InFrameSettings.PreviewMaxGPUIndex, MaxExplicitGPUIndex);

					static int32 PreviewGPUIndex = MinGPUIndex;
					Context.GPUIndex = PreviewGPUIndex++;

					if (PreviewGPUIndex > MaxGPUIndex)
					{
						PreviewGPUIndex = MinGPUIndex;
					}
				}
			}
			else
			{
				// Set custom GPU index for this view
				const int32 CustomMultiGPUIndex = (ContextIt > 0 && RenderSettings.StereoGPUIndex >= 0) ? RenderSettings.StereoGPUIndex : RenderSettings.GPUIndex;
				Context.GPUIndex = FMath::Min(CustomMultiGPUIndex, MaxExplicitGPUIndex);
			}
		}

		Context.FrameTargetRect = FrameTargetRect;
		Context.RenderTargetRect = RenderTargetRect;
		Context.ContextSize = ContextSize;

		// r.ScreenPercentage
		switch (RenderSettings.CaptureMode)
		{
		case EDisplayClusterViewportCaptureMode::Chromakey:
		case EDisplayClusterViewportCaptureMode::Lightcard:
			// we should not change the size of the Chromakey\Lighcards due to the way copy\resolve works for RT's
			// if the viewfamily resolves to RenderTarget it will remove alpha channel
			// if the viewfamily copying to RenderTarget, the texture would not match the size of RTT (when ScreenPercentage applied)
			break;
		default:
			Context.CustomBufferRatio = CustomBufferRatio;
		}

		Context.bDisableRender = bDisableRender;

		Contexts.Add(Context);
	}

	// Reserve for resources
	if (!bDisableRender)
	{
		RenderTargets.AddZeroed(FrameTargetsAmount);
	}

	if (!bDisableInternalResources)
	{
		InputShaderResources.AddZeroed(FrameTargetsAmount);

		if (ShouldUseAdditionalTargetableResource())
		{
			AdditionalTargetableResources.AddZeroed(FrameTargetsAmount);
		}

		// Setup Mips resources:
		for (FDisplayClusterViewport_Context& ContextIt : Contexts)
		{
			ContextIt.NumMips = DisplayClusterViewportHelpers::GetMaxTextureNumMips(InFrameSettings, PostRenderSettings.GenerateMips.GetRequiredNumMips(ContextIt.ContextSize));
			if (ContextIt.NumMips > 1)
			{
				MipsShaderResources.AddZeroed(1);
			}
		}
	}

	return true;
}

// Currently, some data like EngineShowFlags and EngineGamma get updated on PostRenderViewFamily.
// Since the viewports that have media input assigned never gets rendered in normal way, the PostRenderViewFamily
// callback never gets called, therefore the data mentioned above never gets updated. This workaround initializes
// those settings for all viewports. The viewports with no media input assigned will override the data
// in PostRenderViewFamily like it was previously so nothing should be broken.
void FDisplayClusterViewport::UpdateMediaDependencies(FViewport* InViewport)
{
	if (InViewport && InViewport->GetClient())
	{
		if (const FEngineShowFlags* const EngineShowFlags = InViewport->GetClient()->GetEngineShowFlags())
		{
			const float DisplayGamma = GEngine ? GEngine->DisplayGamma : 2.2f;

			for (FDisplayClusterViewport_Context& Context : Contexts)
			{
				Context.RenderThreadData.EngineDisplayGamma = DisplayGamma;
				Context.RenderThreadData.EngineShowFlags = *EngineShowFlags;
			}
		}
	}
}
