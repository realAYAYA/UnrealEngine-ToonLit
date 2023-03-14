// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#if WITH_EDITOR

#include "EngineModule.h"
#include "CanvasTypes.h"
#include "LegacyScreenPercentageDriver.h"

#include "SceneView.h"
#include "SceneViewExtension.h"

#include "Engine/Scene.h"
#include "GameFramework/WorldSettings.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterRootActor.h" 

#include "ClearQuad.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportManager
///////////////////////////////////////////////////////////////////////////////////////

void FDisplayClusterViewportManager::ImplUpdatePreviewRTTResources()
{
	check(IsInGameThread());

	switch (GetRenderFrameSettings().RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Only for preview modes:
		break;
	default:
		return;
	}

	// The preview RTT created inside root actor
	ADisplayClusterRootActor* RootActor = GetRootActor();
	if (RootActor == nullptr)
	{
		return;
	}

	const FString& ClusterNodeId = GetRenderFrameSettings().ClusterNodeId;

	TArray<FString>        PreviewViewportNames;
	TArray<FTextureRHIRef> PreviewRenderTargetableTextures;

	PreviewViewportNames.Reserve(Viewports.Num());
	PreviewRenderTargetableTextures.Reserve(Viewports.Num());

	// Collect visible and enabled viewports for preview rendering:
	for (FDisplayClusterViewport* const ViewportIt : Viewports)
	{
		// update only current cluster node
		if ((ClusterNodeId.IsEmpty() || ViewportIt->GetClusterNodeId() == ClusterNodeId))
		{
			if (ViewportIt->RenderSettings.bEnable && ViewportIt->RenderSettings.bVisible)
			{
				PreviewViewportNames.Add(ViewportIt->GetId());
				PreviewRenderTargetableTextures.AddDefaulted();
			}
		}
	}

	// Get all supported preview rtt resources from root actor
	RootActor->GetPreviewRenderTargetableTextures(PreviewViewportNames, PreviewRenderTargetableTextures);

	// Configure preview RTT to viwports:
	for (int32 ViewportIndex = 0; ViewportIndex < PreviewViewportNames.Num(); ViewportIndex++)
	{
		FDisplayClusterViewport* DesiredViewport = ImplFindViewport(PreviewViewportNames[ViewportIndex]);
		if (DesiredViewport != nullptr)
		{
			FTextureRHIRef& PreviewRTT = PreviewRenderTargetableTextures[ViewportIndex];
			if (PreviewRTT.IsValid())
			{
				// Use mapped preview viewport
				DesiredViewport->OutputPreviewTargetableResource = PreviewRTT;
			}
			else
			{
				// disable visible, but unused by root actor viewports
				DesiredViewport->RenderSettings.bEnable = false;
			}
		}
	}
}

bool FDisplayClusterViewportManager::RenderInEditor(class FDisplayClusterRenderFrame& InRenderFrame, FViewport* InViewport, const uint32 InFirstViewportNum, const int32 InViewportsAmount, int32& OutViewportsAmount, bool& bOutFrameRendered)
{
	bOutFrameRendered = false;
	OutViewportsAmount = 0;

	UWorld* CurrentWorld = GetCurrentWorld();
	if (CurrentWorld == nullptr)
	{
		return false;
	}

	const ADisplayClusterRootActor* RootActor = GetRootActor();
	if (RootActor && !RootActor->IsEditorRenderEnabled())
	{
		bOutFrameRendered = true;
		return true;
	}
	
	FSceneInterface* PreviewScene = CurrentWorld->Scene;
	FEngineShowFlags EngineShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);

	int32 ViewportIndex = 0;
	bool bViewportsRenderPassDone = false;

	for (FDisplayClusterRenderFrame::FFrameRenderTarget& RenderTargetIt : InRenderFrame.RenderTargets)
	{
		if (bViewportsRenderPassDone)
		{
			break;
		}

		// Special flag, allow clear RTT surface only for first family
		bool bAdditionalViewFamily = false;

		for (FDisplayClusterRenderFrame::FFrameViewFamily& ViewFamiliesIt : RenderTargetIt.ViewFamilies)
		{
			if (bViewportsRenderPassDone)
			{
				break;
			}

			if ((ViewportIndex + ViewFamiliesIt.Views.Num()) < (int32)(InFirstViewportNum + 1))
			{
				// Skip already rendered viewports
				ViewportIndex += ViewFamiliesIt.Views.Num();
			}
			else
			{
				// Create the view family for rendering the world scene to the viewport's render target
				FSceneViewFamilyContext ViewFamily(CreateViewFamilyConstructionValues(
					RenderTargetIt,
					PreviewScene,
					EngineShowFlags,
					bAdditionalViewFamily
				));

				ConfigureViewFamily(RenderTargetIt, ViewFamiliesIt, ViewFamily);

				if (RenderTargetIt.CaptureMode == EDisplayClusterViewportCaptureMode::Default
					&& RootActor
					&& !RootActor->bPreviewEnablePostProcess
					&& RootActor->DoObserversNeedPostProcessRenderTarget())
				{
					if (ViewFamily.EngineShowFlags.TemporalAA)
					{
						ViewFamily.EngineShowFlags.SetTemporalAA(false);
						ViewFamily.EngineShowFlags.SetAntiAliasing(true);
					}
				}
				
				for (FDisplayClusterRenderFrame::FFrameView& ViewIt : ViewFamiliesIt.Views)
				{
					bool bViewportAlreadyRendered = ViewportIndex < (int32)InFirstViewportNum;
					ViewportIndex++;

					if (!bViewportAlreadyRendered)
					{
						FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport);
						check(ViewportPtr != nullptr);
						check(ViewIt.ContextNum < (uint32)ViewportPtr->Contexts.Num());

						// Calculate the player's view information.
						FVector  ViewLocation;
						FRotator ViewRotation;
						FSceneView* View = ViewportPtr->ImplCalcScenePreview(ViewFamily, ViewIt.ContextNum);

						if (View != nullptr && ViewIt.ShouldRenderSceneView() == false)
						{
							ViewFamily.Views.Remove(View);

							delete View;
							View = nullptr;
						}

						if (View != nullptr)
						{
							// Count all rendered viewports on this render pass
							OutViewportsAmount++;

							// Apply viewport context settings to view (crossGPU, visibility, etc)
							ViewIt.Viewport->SetupSceneView(ViewIt.ContextNum, PreviewScene->GetWorld(), ViewFamily, *View);

							// The allowed number of viewports is limited by the "InViewportAmount" value.
							if (InViewportsAmount > 0 && OutViewportsAmount >= InViewportsAmount)
							{
								bViewportsRenderPassDone = true;
								break;
							}
						}
					}
				}

				if (ViewFamily.Views.Num() > 0)
				{
					// Screen percentage is still not supported in scene capture.
					ViewFamily.EngineShowFlags.ScreenPercentage = false;
					ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

					FCanvas Canvas(RenderTargetIt.RenderTargetPtr, nullptr, PreviewScene->GetWorld(), ERHIFeatureLevel::SM5, FCanvas::CDM_DeferDrawing /*FCanvas::CDM_ImmediateDrawing*/, 1.0f);
					Canvas.Clear(FLinearColor::Black);

					GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);

					if (GNumExplicitGPUsForRendering > 1)
					{
						const FRHIGPUMask SubmitGPUMask = ViewFamily.Views.Num() == 1 ? ViewFamily.Views[0]->GPUMask : FRHIGPUMask::All();
						ENQUEUE_RENDER_COMMAND(UDisplayClusterViewportClient_SubmitCommandList)(
							[SubmitGPUMask](FRHICommandListImmediate& RHICmdList)
						{
							SCOPED_GPU_MASK(RHICmdList, SubmitGPUMask);
							RHICmdList.SubmitCommandsHint();
						});
					}
				}
			}
		}
	}

	// Render cluster node compose on this pass if possible
	bool bHasExtraRenderPass = (InViewportsAmount < 0) || (OutViewportsAmount < InViewportsAmount);

	// bViewportsRenderPassDone means all viewports for this cluster node have been rendered
	// and we can start frame composing(creating mips, warps, composing icvfx, OutputRemap, etc.).
	// The main idea is to move the frame composition to a separate render pass.
	// This will reduce the load on the CPU + GPU and increase the FPS for the preview.
	if (!bViewportsRenderPassDone && bHasExtraRenderPass)
	{
		// Count cluster node compose render pass as viewport
		OutViewportsAmount++;

		// Handle special viewports game-thread logic at frame end
		// custom postprocess single frame flag must be removed at frame end on game thread
		FinalizeNewFrame();

		// After all render target rendered call nDisplay frame rendering:
		RenderFrame(InViewport);

		bOutFrameRendered = true;
	}

	return true;
}

#endif
