// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameManager.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetManager.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessOutputRemap.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationBase.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"

#include "WarpBlend/IDisplayClusterWarpBlend.h"

#include "SceneViewExtension.h"

#include "DisplayClusterRootActor.h"

#include "Misc/DisplayClusterLog.h"
#include "LegacyScreenPercentageDriver.h"

#include "Engine/Console.h"

int32 GDisplayClusterLightcardsAllowNanite = 0;
static FAutoConsoleVariableRef CVarDisplayClusterLightcardsAllowNanite(
	TEXT("DC.Lightcards.AllowNanite"),
	GDisplayClusterLightcardsAllowNanite,
	TEXT("0 disables Nanite when rendering lightcards. Otherwise uses default showflag."),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterChromaKeyAllowNanite = 0;
static FAutoConsoleVariableRef CVarDisplayClusterChromaKeyAllowNanite(
	TEXT("DC.ChromaKey.AllowNanite"),
	GDisplayClusterChromaKeyAllowNanite,
	TEXT("0 disables Nanite when rendering custom chroma keys. Otherwise uses default showflag."),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewportManager
///////////////////////////////////////////////////////////////////////////////////////

FDisplayClusterViewportManager::FDisplayClusterViewportManager()
{
	Configuration      = MakeUnique<FDisplayClusterViewportConfiguration>(*this);
	RenderFrameManager = MakeUnique<FDisplayClusterRenderFrameManager>();
 
	ViewportManagerProxy = MakeShared<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe>();
 
	RenderTargetManager = MakeShared<FDisplayClusterRenderTargetManager, ESPMode::ThreadSafe>(ViewportManagerProxy.Get());
	PostProcessManager  = MakeShared<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe>(*this);
	LightCardManager = MakeShared<FDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe>(*this);
 
	// initialize proxy
	ViewportManagerProxy->Initialize(*this);
 
	// Always reset RTT when root actor re-created
	ResetSceneRenderTargetSize();

	RegisterCallbacks();
}

FDisplayClusterViewportManager::~FDisplayClusterViewportManager()
{
	UnregisterCallbacks();
	
	// Remove viewports
	for (FDisplayClusterViewport* ViewportIt : Viewports)
	{
		if (ViewportIt)
		{
			delete ViewportIt;
		}
	}
	Viewports.Reset();
	ClusterNodeViewports.Reset();

	RenderTargetManager.Reset();
	PostProcessManager.Reset();

	if (LightCardManager.IsValid())
	{
		LightCardManager->Release();
		LightCardManager.Reset();
	}

	if (ViewportManagerViewExtension.IsValid())
	{
		// Force release of VE data since this TSharedPtr<> may also be held by other resources at this time.
		// So Reset() doesn't actually release it right away in some situations.
		ViewportManagerViewExtension->Release();
		ViewportManagerViewExtension.Reset();
	}

	Configuration.Reset();

	if (ViewportManagerProxy.IsValid())
	{
		// Remove viewport manager proxy on render_thread
		ENQUEUE_RENDER_COMMAND(DeleteDisplayClusterViewportManagerProxy)(
			[ViewportManagerProxy = ViewportManagerProxy](FRHICommandListImmediate& RHICmdList)
	{
				ViewportManagerProxy->Release_RenderThread();
			});

		ViewportManagerProxy.Reset();
	}
}

const IDisplayClusterViewportManagerProxy* FDisplayClusterViewportManager::GetProxy() const
{
	return ViewportManagerProxy.Get();
}

IDisplayClusterViewportManagerProxy* FDisplayClusterViewportManager::GetProxy()
{
	return ViewportManagerProxy.Get();
}

UWorld* FDisplayClusterViewportManager::GetCurrentWorld() const
{
	check(IsInGameThread());

	if (!CurrentWorldRef.IsValid() || CurrentWorldRef.IsStale())
	{
		return nullptr;
	}

	return CurrentWorldRef.Get();
}

ADisplayClusterRootActor* FDisplayClusterViewportManager::GetRootActor() const
{
	return Configuration->GetRootActor();
}

bool FDisplayClusterViewportManager::IsSceneOpened() const
{
	check(IsInGameThread());

	return CurrentWorldRef.IsValid() && !CurrentWorldRef.IsStale();
}

void FDisplayClusterViewportManager::StartScene(UWorld* InWorld)
{
	check(IsInGameThread());

	CurrentWorldRef = TWeakObjectPtr<UWorld>(InWorld);

	for (FDisplayClusterViewport* Viewport : Viewports)
	{
		if (Viewport)
		{
			Viewport->HandleStartScene();
		}
	}

	if (PostProcessManager.IsValid())
	{
		PostProcessManager->HandleStartScene();
	}

	if (LightCardManager.IsValid())
	{
		LightCardManager->HandleStartScene();
	}
}

void FDisplayClusterViewportManager::EndScene()
{
	check(IsInGameThread());

	for (FDisplayClusterViewport* Viewport : Viewports)
	{
		if (Viewport)
		{
			Viewport->HandleEndScene();
		}
	}

	if (PostProcessManager.IsValid())
	{
		PostProcessManager->HandleEndScene();
	}

	if (LightCardManager.IsValid())
	{
		LightCardManager->HandleEndScene();
	}

	CurrentWorldRef.Reset();
}

void FDisplayClusterViewportManager::ResetScene()
{
	check(IsInGameThread());

	if (LightCardManager.IsValid())
	{
		LightCardManager->HandleEndScene();
		LightCardManager->HandleStartScene();
	}

	for (FDisplayClusterViewport* Viewport : Viewports)
	{
		if (Viewport)
		{
			Viewport->HandleEndScene();
			Viewport->HandleStartScene();
		}
	}
}

void FDisplayClusterViewportManager::SetViewportBufferRatio(FDisplayClusterViewport& DstViewport, float InBufferRatio)
{
	const float BufferRatio = FLegacyScreenPercentageDriver::GetCVarResolutionFraction() * InBufferRatio;
	if (DstViewport.RenderSettings.BufferRatio > BufferRatio)
	{
		// Reset scene RTT when buffer ratio changed down
		ResetSceneRenderTargetSize();
	}

	DstViewport.RenderSettings.BufferRatio = BufferRatio;
}

void FDisplayClusterViewportManager::HandleViewportRTTChanges(const TArray<FDisplayClusterViewport_Context>& PrevContexts, const TArray<FDisplayClusterViewport_Context>& Contexts)
{
	// Support for resetting RTT size when viewport size is changed
	if (PrevContexts.Num() != Contexts.Num())
	{
		// Reset scene RTT size when viewport disabled
		ResetSceneRenderTargetSize();
	}
	else
	{
		for (int32 ContextIt = 0; ContextIt < Contexts.Num(); ContextIt++)
		{
			if (Contexts[ContextIt].RenderTargetRect.Size() != PrevContexts[ContextIt].RenderTargetRect.Size())
			{
				ResetSceneRenderTargetSize();
				break;
			}
		}
	}
}

void FDisplayClusterViewportManager::ResetSceneRenderTargetSize()
{
	SceneRenderTargetResizeMethod = ESceneRenderTargetResizeMethod::Reset;
}

void FDisplayClusterViewportManager::UpdateSceneRenderTargetSize()
{
	if (SceneRenderTargetResizeMethod != ESceneRenderTargetResizeMethod::None)
	{
		IConsoleVariable* const RTResizeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneRenderTargetResizeMethod"));
		if (RTResizeCVar)
		{
			switch (SceneRenderTargetResizeMethod)
			{
			case ESceneRenderTargetResizeMethod::Reset:
				// Resize to match requested render size (Default) (Least memory use, can cause stalls when size changes e.g. ScreenPercentage)
				RTResizeCVar->Set(0);

				// Wait for frame history is done
				// static const uint32 FSceneRenderTargets::FrameSizeHistoryCount = 3;
				FrameHistoryCounter = 3;
				SceneRenderTargetResizeMethod = ESceneRenderTargetResizeMethod::WaitFrameSizeHistory;
				break;

			case ESceneRenderTargetResizeMethod::WaitFrameSizeHistory:
				if (FrameHistoryCounter-- < 0)
				{
					SceneRenderTargetResizeMethod = ESceneRenderTargetResizeMethod::Restore;
				}
				break;

			case ESceneRenderTargetResizeMethod::Restore:
				// Expands to encompass the largest requested render dimension. (Most memory use, least prone to allocation stalls.)
				RTResizeCVar->Set(2);
				SceneRenderTargetResizeMethod = ESceneRenderTargetResizeMethod::None;
				break;

			default:
				break;
			}
		}
	}
}

bool FDisplayClusterViewportManager::ShouldUseFullSizeFrameTargetableResource() const
{
	check(IsInGameThread());

	if (PostProcessManager.IsValid() && PostProcessManager->ShouldUseFullSizeFrameTargetableResource())
	{
		return true;
	}

	for (const FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport && Viewport->ShouldUseFullSizeFrameTargetableResource())
		{
			return true;
		}
	}

	return false;
}

bool FDisplayClusterViewportManager::ShouldUseAdditionalFrameTargetableResource() const
{
	check(IsInGameThread());

	if (PostProcessManager.IsValid() && PostProcessManager->ShouldUseAdditionalFrameTargetableResource())
	{
		return true;
	}

	for (const FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport && Viewport->ShouldUseAdditionalFrameTargetableResource())
		{
			return true;
		}
	}

	return false;
}

bool FDisplayClusterViewportManager::UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames, class ADisplayClusterRootActor* InRootActorPtr)
{
	ImplUpdateClusterNodeViewports(InRenderMode, TEXT(""));

	if (InRootActorPtr)
	{
		const bool bIsRootActorChanged = Configuration->SetRootActor(InRootActorPtr);

		// When the root actor changes, we have to ResetScene() to reinitialize the internal references of the projection policy.
		if (bIsRootActorChanged)
		{
			ResetScene();
		}

		return Configuration->UpdateCustomConfiguration(InRenderMode, InViewportNames);
	}

	return false;
}

bool FDisplayClusterViewportManager::UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, ADisplayClusterRootActor* InRootActorPtr, const FDisplayClusterPreviewSettings* InPreviewSettings)
{
	ImplUpdateClusterNodeViewports(InRenderMode, InClusterNodeId);

	if (InRootActorPtr)
	{
		const bool bIsRootActorChanged = Configuration->SetRootActor(InRootActorPtr);

		// When the root actor changes, we have to ResetScene() to reinitialize the internal references of the projection policy.
		if (bIsRootActorChanged)
		{
			ResetScene();
		}

		if (LightCardManager.IsValid())
		{
			LightCardManager->UpdateConfiguration();
		}

		if (InPreviewSettings == nullptr)
		{
			if (Configuration->UpdateConfiguration(InRenderMode, InClusterNodeId))
			{
				return true;
			}
		}
#if WITH_EDITOR
		else
		{
			if (Configuration->UpdatePreviewConfiguration(InRenderMode, InClusterNodeId, *InPreviewSettings))
			{
				return true;
			}
		}
#endif
	}

	return false;
}

const FDisplayClusterRenderFrameSettings& FDisplayClusterViewportManager::GetRenderFrameSettings() const
{
	check(IsInGameThread());

	return Configuration->GetRenderFrameSettings();
}

void FDisplayClusterViewportManager::ImplUpdateClusterNodeViewports(const EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId)
{
	ClusterNodeViewports.Empty();

	switch (InRenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		{
			// Per-viewport preview render required special render order for viewports
			// Sort viewports: move linked and overridden viewports to last
			TArray<FDisplayClusterViewport*> LinkedViewports;
			TArray<FDisplayClusterViewport*> OverriddenViewports;
			for (FDisplayClusterViewport* Viewport : Viewports)
			{
				if (Viewport && (InClusterNodeId.IsEmpty() || Viewport->GetClusterNodeId() == InClusterNodeId))
				{
					if (Viewport->RenderSettings.IsViewportOverrided())
					{
						OverriddenViewports.Add(Viewport);
					}
					else if (Viewport->RenderSettings.IsViewportHasParent())
					{
						LinkedViewports.Add(Viewport);
					}
					else
					{
						ClusterNodeViewports.Add(Viewport);
					}
				}
			}

			ClusterNodeViewports.Append(LinkedViewports);
			ClusterNodeViewports.Append(OverriddenViewports);
		}
		break;
	default:
		// Use all exist viewports for render
		ClusterNodeViewports.Append(Viewports);
		break;
	}
}

void FDisplayClusterViewportManager::RegisterCallbacks()
{
#if WITH_EDITOR
	PreGarbageCollectHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FDisplayClusterViewportManager::OnPreGarbageCollect);
#endif
}

void FDisplayClusterViewportManager::UnregisterCallbacks()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGarbageCollectHandle);
#endif
}

int32 FDisplayClusterViewportManager::GetViewPerViewportAmount() const
{
	switch (GetRenderFrameSettings().RenderMode)
	{
	case EDisplayClusterRenderFrameMode::Mono:
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		return 1;
		break;
	default:
		break;
	}

	return 2;
}

int32 FDisplayClusterViewportManager::FindFirstViewportStereoViewIndex(FDisplayClusterViewport* InViewport) const
{
	const int32 ViewportIndex = Viewports.Find(InViewport);
	if (ViewportIndex != INDEX_NONE)
	{
		const int32 ViewPerViewportAmount = GetViewPerViewportAmount();
		// Begin from 1, because INDEX_NONE use ViewState[0] in LocalPlayer.cpp:786
		const int32 FirstViewportStereoViewIndex = (ViewportIndex * ViewPerViewportAmount) + 1;

		return FirstViewportStereoViewIndex;
	}

	return INDEX_NONE;
}

bool FDisplayClusterViewportManager::BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame)
{
	check(IsInGameThread());

	OutRenderFrame.ViewportManager = this;

	// Handle world runtime update
	UWorld* CurrentWorld = GetCurrentWorld();
	if (CurrentWorld != InWorld)
	{
		// Handle end current scene
		if (CurrentWorld)
		{
			EndScene();
		}

		// Handle begin new scene
		if (InWorld)
		{
			StartScene(InWorld);
		}
		else
		{
			// no world for render
			return false;
		}
	}

	// Create DC ViewExtension to handle special features
	if (!ViewportManagerViewExtension.IsValid())
	{
		ViewportManagerViewExtension = FSceneViewExtensions::NewExtension<FDisplayClusterViewportManagerViewExtension>(this);
	}

	// Before new frame
	if (PostProcessManager.IsValid())
	{
		PostProcessManager->HandleSetupNewFrame();
	}

	const FDisplayClusterRenderFrameSettings& RenderFrameSettings = GetRenderFrameSettings();

	// Initialize viewports from new render settings, and create new contexts, reset prev frame resources
	for (FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		const int32 FirstViewportStereoViewIndex = FindFirstViewportStereoViewIndex(Viewport);
		if (FirstViewportStereoViewIndex != INDEX_NONE)
		{
			// Save orig viewport contexts
			TArray<FDisplayClusterViewport_Context> PrevContexts;
			PrevContexts.Append(Viewport->GetContexts());
			Viewport->UpdateFrameContexts(FirstViewportStereoViewIndex, RenderFrameSettings);

			HandleViewportRTTChanges(PrevContexts, Viewport->GetContexts());
		}
		else
		{
			// do not render this viewport
			Viewport->ResetFrameContexts();
		}
	}

	/* Some media dependent data can (and needs to) be initialized in advance */
	for (FDisplayClusterViewport* const Viewport : ClusterNodeViewports)
	{
		Viewport->UpdateMediaDependencies(InViewport);
	}

	// Handle scene RTT resize
	UpdateSceneRenderTargetSize();

	// Build new frame structure
	if (!RenderFrameManager->BuildRenderFrame(InViewport, RenderFrameSettings, ClusterNodeViewports, OutRenderFrame))
	{
		return false;
	}

	// Allocate resources for frame
	if (!RenderTargetManager->AllocateRenderFrameResources(InViewport, RenderFrameSettings, ClusterNodeViewports, OutRenderFrame))
	{
		return false;
	}

	const FIntPoint RenderFrameSize = OutRenderFrame.FrameRect.Size();

	for (FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport)
		{
			// Update ViewportRemap geometry
			Viewport->ViewportRemap.Update(*Viewport, RenderFrameSize);
		}
	}

	// Update desired views number
	UpdateDesiredNumberOfViews(OutRenderFrame);

#if WITH_EDITOR
	// Get preview resources from root actor
	ImplUpdatePreviewRTTResources();
#endif /*WITH_EDITOR*/

	if (PostProcessManager.IsValid())
	{
		PostProcessManager->HandleBeginNewFrame(OutRenderFrame);
	}

	return true;
}

void FDisplayClusterViewportManager::UpdateDesiredNumberOfViews(FDisplayClusterRenderFrame& InOutRenderFrame)
{
	// Total views ammount
	const int32 ViewPerViewportAmount = GetViewPerViewportAmount();
	const int32 DesiredNumberOfViews = (Viewports.Num() * ViewPerViewportAmount) + 1;

	InOutRenderFrame.DesiredNumberOfViews = DesiredNumberOfViews;

	// Total view on current cluster node
	InOutRenderFrame.ViewportsAmount = 0;

	for (FDisplayClusterRenderFrame::FFrameRenderTarget& RenderTargetIt : InOutRenderFrame.RenderTargets)
	{
		for (FDisplayClusterRenderFrame::FFrameViewFamily& ViewFamilyIt : RenderTargetIt.ViewFamilies)
		{
			ViewFamilyIt.NumViewsForRender = 0;

			for (FDisplayClusterRenderFrame::FFrameView& ViewIt : ViewFamilyIt.Views)
			{
				InOutRenderFrame.ViewportsAmount++;

				if (ViewIt.bDisableRender == false && ViewIt.Viewport != nullptr)
				{
					ViewFamilyIt.NumViewsForRender++;

					// Get StereoViewIndex for this viewport
					const int32 StereoViewIndex = ViewIt.Viewport->GetContexts()[ViewIt.ContextNum].StereoViewIndex;
				}
			}
		}
	}
}

void FDisplayClusterViewportManager::InitializeNewFrame()
{
	check(IsInGameThread());

	// Before render
	for (FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport)
		{
			// Initialize OCIO resources
			if (Viewport->OpenColorIO.IsValid())
			{
				Viewport->OpenColorIO->UpdateOpenColorIORenderPassResources();
			}
		}
	}

	// Send render frame settings to rendering thread
	ViewportManagerProxy->ImplUpdateRenderFrameSettings(GetRenderFrameSettings(), ViewportManagerViewExtension);

	/**
	 * [1] Send new viewport render resources to proxy:
	 * At the moment, there are new resources in the viewport, but the math is not initialized.The math will be updated later in the game thread.
	 * But these new resources were needed for future rendering on the render thread.
	 * So we are sending viewports from the game stream data to the proxy.
	 * Lastly, after the viewport math data has been updated, we need to send it again.
	 */
	ViewportManagerProxy->ImplUpdateViewports(ClusterNodeViewports);

	// Send postprocess data to render thread
	if (PostProcessManager.IsValid())
	{
		// Update postprocess data from game thread
		PostProcessManager->Tick();

		// Send updated postprocess data to rendering thread
		PostProcessManager->FinalizeNewFrame();
	}
}

void FDisplayClusterViewportManager::FinalizeNewFrame()
{
	check(IsInGameThread());

	// [2] The viewport math has been updated, we need to send it to the proxy again.
	ViewportManagerProxy->ImplUpdateViewports(ClusterNodeViewports);

	for (FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport)
		{
			// When all viewports processed, we remove all single frame custom postprocess
			Viewport->CustomPostProcessSettings.FinalizeFrame();

			// Update projection policy proxy data
			if (Viewport->ProjectionPolicy.IsValid())
			{
				Viewport->ProjectionPolicy->UpdateProxyData(Viewport);
			}

		}
	}

	// Finish update settings
	for (FDisplayClusterViewport* Viewport : ClusterNodeViewports)
	{
		if (Viewport)
		{
			Viewport->RenderSettings.FinishUpdateSettings();
		}
	}
}

FSceneViewFamily::ConstructionValues FDisplayClusterViewportManager::CreateViewFamilyConstructionValues(
	const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget,
	FSceneInterface* InScene,
	FEngineShowFlags InEngineShowFlags,
	const bool bInAdditionalViewFamily) const
{

	bool bResolveScene = true;

	// Control NaniteMeshes for ChromaKey and Lightcards:
	switch (InFrameTarget.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
		if (!GDisplayClusterChromaKeyAllowNanite)
		{
			InEngineShowFlags.SetNaniteMeshes(0);
		}

		break;
	case EDisplayClusterViewportCaptureMode::Lightcard:
		if (!GDisplayClusterLightcardsAllowNanite)
		{
			InEngineShowFlags.SetNaniteMeshes(0);
		}
		break;
	default:
		break;
	}

	const FDisplayClusterRenderFrameSettings& RenderFrameSettings = GetRenderFrameSettings();
	switch (RenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:

		if (RenderFrameSettings.bPreviewEnablePostProcess == false)
		{
			// Disable postprocess for preview
			InEngineShowFlags.PostProcessing = 0;
		}
		break;
	default:
		break;
	}

	switch (InFrameTarget.CaptureMode)
	{
	case EDisplayClusterViewportCaptureMode::Chromakey:
	case EDisplayClusterViewportCaptureMode::Lightcard:
		switch (GetRenderFrameSettings().AlphaChannelCaptureMode)
		{
			case EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy:
				// Disable AA
				InEngineShowFlags.SetAntiAliasing(0);
				InEngineShowFlags.SetTemporalAA(0);
				break;

			case EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA:
				// Use AA
				InEngineShowFlags.SetAntiAliasing(1);
				InEngineShowFlags.SetTemporalAA(0);
				break;

			case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
				// Alpha captured without AA, own FXAA used
				InEngineShowFlags.SetAntiAliasing(0);
				InEngineShowFlags.SetTemporalAA(0);
				break;

			default:
				break;
		}

		// Disable postprocess for LC\CK
		InEngineShowFlags.SetPostProcessing(0);

		InEngineShowFlags.SetAtmosphere(0);
		InEngineShowFlags.SetFog(0);
		InEngineShowFlags.SetVolumetricFog(0);
		InEngineShowFlags.SetMotionBlur(0); // motion blur doesn't work correctly with scene captures.
		InEngineShowFlags.SetSeparateTranslucency(0);
		InEngineShowFlags.SetHMDDistortion(0);
		InEngineShowFlags.SetOnScreenDebug(0);

		InEngineShowFlags.SetLumenReflections(0);
		InEngineShowFlags.SetLumenGlobalIllumination(0);
		InEngineShowFlags.SetGlobalIllumination(0);

		InEngineShowFlags.SetScreenSpaceAO(0);
		InEngineShowFlags.SetAmbientOcclusion(0);
		InEngineShowFlags.SetDeferredLighting(0);
		InEngineShowFlags.SetVirtualTexturePrimitives(0);
		InEngineShowFlags.SetRectLights(0);
		break;

	default:
		break;
	}

	return FSceneViewFamily::ConstructionValues(InFrameTarget.RenderTargetPtr, InScene, InEngineShowFlags)
		.SetResolveScene(bResolveScene)
		.SetRealtimeUpdate(true)
		.SetGammaCorrection(1.0f)
		.SetAdditionalViewFamily(bInAdditionalViewFamily);
}

bool FDisplayClusterViewportManager::ShouldRenderFinalColor() const
{
#if WITH_EDITOR
	const FDisplayClusterRenderFrameSettings& RenderFrameSettings = GetRenderFrameSettings();
	switch (RenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Disable postprocess for preview
		return RenderFrameSettings.bPreviewEnablePostProcess;

	default:
		break;
	}
#endif

	return true;
}

void FDisplayClusterViewportManager::ConfigureViewFamily(const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget, const FDisplayClusterRenderFrame::FFrameViewFamily& InFrameViewFamily, FSceneViewFamilyContext& ViewFamily)
{
	// Note: EngineShowFlags should have already been configured in CreateViewFamilyConstructionValues.
	ViewFamily.SceneCaptureCompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
	if (!ShouldRenderFinalColor())
	{
		ViewFamily.SceneCaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	}

	// Gather Scene View Extensions
	// Note: Chromakey and Light maps use their own view extensions, collected in FDisplayClusterViewport::GatherActiveExtensions().
	ViewFamily.ViewExtensions = InFrameViewFamily.ViewExtensions;

	// Scene View Extension activation with ViewportId granularity only works if you have one ViewFamily per ViewportId
	for (FSceneViewExtensionRef& ViewExt : ViewFamily.ViewExtensions)
	{
		ViewExt->SetupViewFamily(ViewFamily);
	}
}

void FDisplayClusterViewportManager::RenderFrame(FViewport* InViewport)
{
	if (LightCardManager.IsValid())
	{
		LightCardManager->RenderFrame();
	}

	ViewportManagerProxy->ImplRenderFrame(InViewport);
}

bool FDisplayClusterViewportManager::CreateViewport(const FString& InViewportId, const class UDisplayClusterConfigurationViewport* ConfigurationViewport)
{
	check(IsInGameThread());
	check(ConfigurationViewport != nullptr);

	// Check viewport ID
	if (InViewportId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterViewport, Warning, TEXT("Wrong viewport ID"));
		return false;
	}

	// ID must be unique
	if (FindViewport(InViewportId) != nullptr)
	{
		UE_LOG(LogDisplayClusterViewport, Warning, TEXT("Viewport '%s' already exists"), *InViewportId);
		return false;
	}

	// Create projection policy for viewport
	const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> NewProjectionPolicy = CreateProjectionPolicy(InViewportId, &ConfigurationViewport->ProjectionPolicy);
	if (NewProjectionPolicy.IsValid())
	{
		// Create viewport for new projection policy
		FDisplayClusterViewport* NewViewport = ImplCreateViewport(InViewportId, NewProjectionPolicy);
		if (NewViewport != nullptr)
		{
			ADisplayClusterRootActor* RootActorPtr = GetRootActor();
			if (RootActorPtr)
			{
				FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(*this, *RootActorPtr, NewViewport, ConfigurationViewport);
				return true;
			}
		}
	}

	UE_LOG(LogDisplayClusterViewport, Error, TEXT("Viewports '%s' not created."), *InViewportId);
	return false;
}

FDisplayClusterViewport* FDisplayClusterViewportManager::ImplFindViewport(const FString& ViewportId) const
{
	check(IsInGameThread());

	// Ok, we have a request for a particular viewport. Let's find it.
	FDisplayClusterViewport* const* DesiredViewport = Viewports.FindByPredicate([ViewportId](const FDisplayClusterViewport* ItemViewport)
	{
		return ViewportId.Equals(ItemViewport->ViewportId, ESearchCase::IgnoreCase);
	});

	return (DesiredViewport != nullptr) ? *DesiredViewport : nullptr;
}

IDisplayClusterViewport* FDisplayClusterViewportManager::CreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
{
	check(IsInGameThread());

	FDisplayClusterViewport* ExistViewport = ImplFindViewport(ViewportId);
	if (ExistViewport != nullptr)
	{
		//add error log: Viewport with name '%s' already exist
		return nullptr;
	}

	return ImplCreateViewport(ViewportId, InProjectionPolicy);
}

bool FDisplayClusterViewportManager::DeleteViewport(const FString& ViewportId)
{
	check(IsInGameThread());

	FDisplayClusterViewport* ExistViewport = ImplFindViewport(ViewportId);
	if (ExistViewport != nullptr)
	{
		ImplDeleteViewport(ExistViewport);

		return true;
	}

	return false;
}

FDisplayClusterViewport* FDisplayClusterViewportManager::ImplCreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
{
	check(IsInGameThread());

	check(InProjectionPolicy.IsValid());

	// Create viewport for cluster node used for rendering
	const FString& ClusterNodeId = GetRenderFrameSettings().ClusterNodeId;
	check(!ClusterNodeId.IsEmpty());

	// Create viewport
	FDisplayClusterViewport* NewViewport = new FDisplayClusterViewport(*this, ClusterNodeId, ViewportId, InProjectionPolicy);

	// Add viewport on gamethread
	Viewports.Add(NewViewport);
	ClusterNodeViewports.Add(NewViewport);

	// Handle start scene for viewport
	NewViewport->HandleStartScene();

	return NewViewport;
}

void FDisplayClusterViewportManager::ImplDeleteViewport(FDisplayClusterViewport* ExistViewport)
{
	// Handle projection policy event
	ExistViewport->ProjectionPolicy.Reset();
	ExistViewport->UninitializedProjectionPolicy.Reset();

	{
		// Remove viewport from the whole viewports list
		int32 ViewportIndex = Viewports.Find(ExistViewport);
		if (ViewportIndex != INDEX_NONE)
		{
			Viewports[ViewportIndex] = nullptr;
			Viewports.RemoveAt(ViewportIndex);
		}
	}

	{
		// Remove viewport from the cluster viewports list
		int32 ViewportIndex = ClusterNodeViewports.Find(ExistViewport);
		if (ViewportIndex != INDEX_NONE)
		{
			ClusterNodeViewports[ViewportIndex] = nullptr;
			ClusterNodeViewports.RemoveAt(ViewportIndex);
		}
	}

	delete ExistViewport;

	// Reset RTT size after viewport delete
	ResetSceneRenderTargetSize();
}

IDisplayClusterViewport* FDisplayClusterViewportManager::FindViewport(const int32 ViewIndex, uint32* OutContextNum) const
{
	check(IsInGameThread());

	for (FDisplayClusterViewport* Viewport : Viewports)
	{
		if (Viewport && Viewport->FindContext(ViewIndex, OutContextNum))
		{
			return Viewport;
		}
	}

	// Viewport not found
	return nullptr;
}

TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterViewportManager::CreateProjectionPolicy(const FString& InViewportId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	check(IsInGameThread());
	check(InConfigurationProjectionPolicy != nullptr);

	// Generate unique projection policy id from viewport name
	const FString ProjectionPolicyId = FString::Printf(TEXT("%s_%s"), DisplayClusterViewportStrings::prefix::projection, *InViewportId);

	IDisplayClusterRenderManager* const DCRenderManager = IDisplayCluster::Get().GetRenderMgr();
	check(DCRenderManager);

	TSharedPtr<IDisplayClusterProjectionPolicyFactory> ProjPolicyFactory = DCRenderManager->GetProjectionPolicyFactory(InConfigurationProjectionPolicy->Type);
	if (ProjPolicyFactory.IsValid())
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjPolicy = ProjPolicyFactory->Create(ProjectionPolicyId, InConfigurationProjectionPolicy);
		if (ProjPolicy.IsValid())
		{
			return ProjPolicy;
		}
		else
		{
			UE_LOG(LogDisplayClusterViewport, Warning, TEXT("Invalid projection policy: type '%s', RHI '%s', viewport '%s'"), *InConfigurationProjectionPolicy->Type, GDynamicRHI->GetName(), *ProjectionPolicyId);
		}
	}
	else
	{
		UE_LOG(LogDisplayClusterViewport, Warning, TEXT("No projection factory found for projection type '%s'"), *InConfigurationProjectionPolicy->Type);
	}

	return nullptr;
}

void FDisplayClusterViewportManager::MarkComponentGeometryDirty(const FName InComponentName)
{
	check(IsInGameThread());

	// 1. Update all ProceduralMeshComponent references for projection policies
	for (IDisplayClusterViewport* ViewportIt : GetViewports())
	{
		if (ViewportIt != nullptr)
		{
			const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& ProjectionPolicy = ViewportIt->GetProjectionPolicy();
			if (ProjectionPolicy.IsValid())
			{
				TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlendInterface;
				if (ProjectionPolicy->GetWarpBlendInterface(WarpBlendInterface) && WarpBlendInterface.IsValid())
				{
					// Update only interfaces with ProceduralMesh as geometry source
					if (WarpBlendInterface->GetWarpGeometryType() == EDisplayClusterWarpGeometryType::WarpProceduralMesh)
					{
						// Set the ProceduralMeshComponent geometry dirty for all valid WarpBlendInterface
						WarpBlendInterface->MarkWarpGeometryComponentDirty(InComponentName);
					}
				}
			}
		}
	}

	// 2. Update all ProceduralMeshComponent references for OutputRemap
	if (PostProcessManager.IsValid())
	{
		PostProcessManager->GetOutputRemap()->MarkProceduralMeshComponentGeometryDirty(InComponentName);
	}
}

void FDisplayClusterViewportManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FDisplayClusterViewport* ViewportIt : Viewports)
	{
		if (ViewportIt != nullptr)
		{
			ViewportIt->AddReferencedObjects(Collector);
		}
	}
}
