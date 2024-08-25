// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayDevice/Proxy/DisplayClusterDisplayDeviceProxy_OpenColorIO.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"

#include "Shader.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessMaterialInputs.h"

namespace UE::DisplayCluster
{
	static inline float GetDisplayGamma(const FDisplayClusterViewport_Context& InViewportContext)
	{
		const float DefaultDisplayGamma = FOpenColorIORendering::DefaultDisplayGamma;
		const float EngineDisplayGamma = InViewportContext.RenderThreadData.EngineDisplayGamma;

		// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
		// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
		const float DisplayGamma = (InViewportContext.RenderThreadData.EngineShowFlags.Tonemapper == 0) || (InViewportContext.RenderThreadData.EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / EngineDisplayGamma;

		return DisplayGamma;
	}
};

FDisplayClusterDisplayDeviceProxy_OpenColorIO::FDisplayClusterDisplayDeviceProxy_OpenColorIO(FOpenColorIOColorConversionSettings& InColorConversionSettings)
	: OCIOPassId(InColorConversionSettings.ToString())
	, OCIOPassResources(FOpenColorIORendering::GetRenderPassResources(InColorConversionSettings, GMaxRHIFeatureLevel))
{ }

bool FDisplayClusterDisplayDeviceProxy_OpenColorIO::HasFinalPass_RenderThread() const
{
	return true;
}

bool FDisplayClusterDisplayDeviceProxy_OpenColorIO::AddFinalPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
	FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect) const
{
	FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputTextureRHI, TEXT("FDisplayClusterDisplayDeviceOCIOInputTexture")));
	FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputTextureRHI, TEXT("FDisplayClusterDisplayDeviceOCIORenderTargetTexture")));

	FScreenPassRenderTarget Output = FScreenPassRenderTarget(OutputTexture, OutputRect, ERenderTargetLoadAction::EClear);
	EOpenColorIOTransformAlpha TransformAlpha = EOpenColorIOTransformAlpha::None;

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		FScreenPassViewInfo(),
		GMaxRHIFeatureLevel,
		FScreenPassTexture(InputTexture),
		Output,
		OCIOPassResources,
		UE::DisplayCluster::GetDisplayGamma(InViewportContext),
		TransformAlpha
	);

	return true;
}
