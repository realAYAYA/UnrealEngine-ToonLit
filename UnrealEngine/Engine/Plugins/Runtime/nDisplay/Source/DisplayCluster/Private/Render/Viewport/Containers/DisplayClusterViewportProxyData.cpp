// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"


FDisplayClusterViewportProxyData::FDisplayClusterViewportProxyData(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& SrcViewport)
{
	check(IsInGameThread());
	check(SrcViewport.IsValid());

	DstViewportProxy = SrcViewport->ViewportProxy;

	OpenColorIO = SrcViewport->OpenColorIO;

	RenderSettings = SrcViewport->RenderSettings;
	RenderSettingsICVFX.SetParameters(SrcViewport->RenderSettingsICVFX);
	PostRenderSettings.SetParameters(SrcViewport->PostRenderSettings);

	// Additional parameters
	OverscanSettings = SrcViewport->OverscanRendering.Get();

	RemapMesh = SrcViewport->ViewportRemap.GetRemapMesh();

	ProjectionPolicy = SrcViewport->ProjectionPolicy;
	Contexts         = SrcViewport->Contexts;

	// Save resources ptrs into container
	RenderTargets    = SrcViewport->RenderTargets;

	OutputFrameTargetableResources     = SrcViewport->OutputFrameTargetableResources;
	AdditionalFrameTargetableResources = SrcViewport->AdditionalFrameTargetableResources;


#if WITH_EDITOR
	OutputPreviewTargetableResource = SrcViewport->OutputPreviewTargetableResource;
	ViewStates = SrcViewport->ViewStates;
#endif

	InputShaderResources = SrcViewport->InputShaderResources;
	AdditionalTargetableResources = SrcViewport->AdditionalTargetableResources;
	MipsShaderResources = SrcViewport->MipsShaderResources;
}

void FDisplayClusterViewportProxyData::UpdateProxy_RenderThread() const
{
	check(IsInRenderingThread());
	check(DstViewportProxy);

	DstViewportProxy->OpenColorIO = OpenColorIO;

	DstViewportProxy->OverscanSettings = OverscanSettings;

	DstViewportProxy->RemapMesh = RemapMesh;

	DstViewportProxy->RenderSettings = RenderSettings;

	DstViewportProxy->RenderSettingsICVFX.SetParameters(RenderSettingsICVFX);
	DstViewportProxy->PostRenderSettings.SetParameters(PostRenderSettings);

	DstViewportProxy->ProjectionPolicy = ProjectionPolicy;
	
	// The RenderThreadData for DstViewportProxy has been updated in DisplayClusterViewportManagerViewExtension on the rendering thread.
	// Therefore, the RenderThreadData values from the game thread must be overridden by current data from the render thread.
	{
		const TArray<FDisplayClusterViewport_Context> CurrentContexts = DstViewportProxy->Contexts;
		DstViewportProxy->Contexts = Contexts;

		int32 ContextAmmount = FMath::Min(CurrentContexts.Num(), Contexts.Num());
		for (int32 ContextIndex = 0; ContextIndex < ContextAmmount; ContextIndex++)
		{
			DstViewportProxy->Contexts[ContextIndex].RenderThreadData = CurrentContexts[ContextIndex].RenderThreadData;
		}
	}

	// Update viewport proxy resources from container
	DstViewportProxy->RenderTargets    = RenderTargets;

	DstViewportProxy->OutputFrameTargetableResources = OutputFrameTargetableResources;
	DstViewportProxy->AdditionalFrameTargetableResources = AdditionalFrameTargetableResources;

#if WITH_EDITOR
	DstViewportProxy->OutputPreviewTargetableResource = OutputPreviewTargetableResource;
	DstViewportProxy->ViewStates = ViewStates;
#endif

	DstViewportProxy->InputShaderResources = InputShaderResources;
	DstViewportProxy->AdditionalTargetableResources = AdditionalTargetableResources;
	DstViewportProxy->MipsShaderResources = MipsShaderResources;
}
