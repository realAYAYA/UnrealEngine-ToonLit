// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"


FDisplayClusterViewportProxyData::FDisplayClusterViewportProxyData(const FDisplayClusterViewport* SrcViewport)
{
	check(IsInGameThread());
	check(SrcViewport);

	DstViewportProxy = SrcViewport->ViewportProxy;

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
#endif

	InputShaderResources = SrcViewport->InputShaderResources;
	AdditionalTargetableResources = SrcViewport->AdditionalTargetableResources;
	MipsShaderResources = SrcViewport->MipsShaderResources;
}

void FDisplayClusterViewportProxyData::UpdateProxy_RenderThread() const
{
	check(IsInRenderingThread());
	check(DstViewportProxy);

	DstViewportProxy->OverscanSettings = OverscanSettings;

	DstViewportProxy->RemapMesh = RemapMesh;

	DstViewportProxy->RenderSettings = RenderSettings;

	DstViewportProxy->RenderSettingsICVFX.SetParameters(RenderSettingsICVFX);
	DstViewportProxy->PostRenderSettings.SetParameters(PostRenderSettings);

	DstViewportProxy->ProjectionPolicy = ProjectionPolicy;
	DstViewportProxy->Contexts         = Contexts;

	// Update viewport proxy resources from container
	DstViewportProxy->RenderTargets    = RenderTargets;

	DstViewportProxy->OutputFrameTargetableResources = OutputFrameTargetableResources;
	DstViewportProxy->AdditionalFrameTargetableResources = AdditionalFrameTargetableResources;

#if WITH_EDITOR
	DstViewportProxy->OutputPreviewTargetableResource = OutputPreviewTargetableResource;
#endif

	DstViewportProxy->InputShaderResources = InputShaderResources;
	DstViewportProxy->AdditionalTargetableResources = AdditionalTargetableResources;
	DstViewportProxy->MipsShaderResources = MipsShaderResources;
}
