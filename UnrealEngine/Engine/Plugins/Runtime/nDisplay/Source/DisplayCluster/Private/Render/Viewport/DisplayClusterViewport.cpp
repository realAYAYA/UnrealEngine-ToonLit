// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "EngineUtils.h"
#include "SceneView.h"

#include "DisplayClusterSceneViewExtensions.h"

#include "Misc/DisplayClusterLog.h"

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

const TArray<FSceneViewExtensionRef> FDisplayClusterViewport::GatherActiveExtensions(FViewport* InViewport) const
{
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

	return TArray<FSceneViewExtensionRef>();
}

FSceneViewExtensionIsActiveFunctor FDisplayClusterViewport::GetSceneViewExtensionIsActiveFunctor() const
{
	FSceneViewExtensionIsActiveFunctor IsActiveFunction;
	IsActiveFunction.IsActiveFunction = [this](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
	{
		if (Context.IsA(FDisplayClusterSceneViewExtensionContext()))
		{
			const FDisplayClusterSceneViewExtensionContext& DisplayContext = static_cast<const FDisplayClusterSceneViewExtensionContext&>(Context);

			// Find exist viewport by name
			IDisplayClusterViewport* PublicViewport = DisplayContext.ViewportManager->FindViewport(DisplayContext.ViewportId);
			if (PublicViewport)
			{
				FDisplayClusterViewport* Viewport = static_cast<FDisplayClusterViewport*>(PublicViewport);

				if (Viewport->OpenColorIODisplayExtension.IsValid() && Viewport->OpenColorIODisplayExtension.Get() == SceneViewExtension)
				{
					// This viewport use this OCIO extension
					return  TOptional<bool>(true);
				}
			}
		}

		return TOptional<bool>(false);
	};

	return IsActiveFunction;
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
	for (FSceneViewStateReference& ViewState : ViewStates)
	{
		FSceneViewStateInterface* Ref = ViewState.GetReference();
		if (Ref)
		{
			Ref->AddReferencedObjects(Collector);
		}
	}
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

	// Setup MGPU features:
	if(Contexts[ContextNum].GPUIndex >= 0)
	{
		InOutView.bOverrideGPUMask = true;
		InOutView.GPUMask = FRHIGPUMask::FromIndex(FMath::Min((uint32)Contexts[ContextNum].GPUIndex, GNumExplicitGPUsForRendering - 1));
		InOutView.bAllowCrossGPUTransfer = (Contexts[ContextNum].bAllowGPUTransferOptimization == false);
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
	if ((RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_ICVFXTarget) != 0)
	{
		ClusterRenderTargetRatioMult *= InFrameSettings.ClusterICVFXOuterViewportRenderTargetRatioMult;
	}
	else if ((RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_ICVFXIncamera) != 0)
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

	if ((RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_ICVFXTarget) != 0)
	{
		// Outer viewport
		CustomBufferRatio *= InFrameSettings.ClusterICVFXOuterViewportBufferRatioMult;
	}
	else if ((RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_ICVFXIncamera) != 0)
	{
		// Inner Frustum
		CustomBufferRatio *= InFrameSettings.ClusterICVFXInnerFrustumBufferRatioMult;
	}

	return CustomBufferRatio;
}

bool FDisplayClusterViewport::UpdateFrameContexts(const uint32 InStereoViewIndex, const FDisplayClusterRenderFrameSettings& InFrameSettings)
{
	check(IsInGameThread());

	uint32 FrameTargetsAmount = 2;
	FIntRect     DesiredFrameTargetRect = RenderSettings.Rect;
	{
		switch (InFrameSettings.RenderMode)
		{
		case EDisplayClusterRenderFrameMode::Mono:
			FrameTargetsAmount = 1;
			break;
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

			// Mono
			FrameTargetsAmount = 1;
			break;
		}
		default:
			break;
		}
	}

	// Special case mono->stereo
	uint32 ViewportContextAmount = RenderSettings.bForceMono ? 1 : FrameTargetsAmount;

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
	if (RenderSettings.bSkipRendering || RenderSettings.OverrideViewportId.IsEmpty() == false)
	{
		bDisableInternalResources = true;
		bDisableRender = true;
	}

	//Add new contexts
	for (uint32 ContextIt = 0; ContextIt < ViewportContextAmount; ++ContextIt)
	{
		const EStereoscopicPass StereoscopicPass = FDisplayClusterViewportStereoscopicPass::EncodeStereoscopicPass(ContextIt, ViewportContextAmount, InFrameSettings);
		const int32 StereoViewIndex = (int32)(InStereoViewIndex + ContextIt);

		FDisplayClusterViewport_Context Context(ContextIt, StereoscopicPass, StereoViewIndex);

		int32 ContextGPUIndex = (ContextIt > 0 && RenderSettings.StereoGPUIndex >= 0) ? RenderSettings.StereoGPUIndex : RenderSettings.GPUIndex;
		Context.GPUIndex = ContextGPUIndex;


		if (InFrameSettings.bIsRenderingInEditor)
		{
			// Disable MultiGPU feature
			Context.GPUIndex = INDEX_NONE;

			if (InFrameSettings.bAllowMultiGPURenderingInEditor)
			{
				// Experimental: allow mGPU for preview rendering:
				if (GNumExplicitGPUsForRendering > 1 && bDisableRender == false)
				{
					int32 MaxExplicitGPUIndex = GNumExplicitGPUsForRendering - 1;

					int32 MinGPUIndex = FMath::Min(InFrameSettings.PreviewMinGPUIndex, MaxExplicitGPUIndex);
					int32 MaxGPUIndex = FMath::Min(InFrameSettings.PreviewMaxGPUIndex, MaxExplicitGPUIndex);

					static int32 PreviewGPUIndex = MinGPUIndex;
					Context.GPUIndex = PreviewGPUIndex++;

					if (PreviewGPUIndex > MaxGPUIndex)
					{
						PreviewGPUIndex = MinGPUIndex;
					}

					Context.bAllowGPUTransferOptimization = true;
					Context.bEnabledGPUTransferLockSteps = false;
				}
			}
		}
		else
		{
			// Control mGPU:
			switch (InFrameSettings.MultiGPUMode)
			{
			case EDisplayClusterMultiGPUMode::None:
				Context.bAllowGPUTransferOptimization = false;
				Context.GPUIndex = INDEX_NONE;
				break;

			case EDisplayClusterMultiGPUMode::Optimized_EnabledLockSteps:
				Context.bAllowGPUTransferOptimization = true;
				Context.bEnabledGPUTransferLockSteps = true;
				break;

			case EDisplayClusterMultiGPUMode::Optimized_DisabledLockSteps:
				Context.bAllowGPUTransferOptimization = true;
				Context.bEnabledGPUTransferLockSteps = false;
				break;

			default:
				Context.bAllowGPUTransferOptimization = false;
				break;
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
