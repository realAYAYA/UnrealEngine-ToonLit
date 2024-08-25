// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"

#include "OpenColorIOConfiguration.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIODisplayManager.h"
#include "OpenColorIODisplayExtension.h"
#include "OpenColorIOShader.h"

#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessMaterialInputs.h"

//////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport_OpenColorIO
//////////////////////////////////////////////////////////////////////////
FDisplayClusterViewport_OpenColorIO::FDisplayClusterViewport_OpenColorIO(const FOpenColorIOColorConversionSettings& InDisplayConfiguration)
	: ConversionSettings(InDisplayConfiguration)
{ }

FDisplayClusterViewport_OpenColorIO::~FDisplayClusterViewport_OpenColorIO()
{ }

void FDisplayClusterViewport_OpenColorIO::SetupSceneView(FSceneViewFamily& InOutViewFamily, FSceneView& InOutView)
{
	FOpenColorIORenderPassResources PassResources = FOpenColorIORendering::GetRenderPassResources(ConversionSettings, InOutViewFamily.GetFeatureLevel());
	if (PassResources.IsValid())
	{
		FOpenColorIORendering::PrepareView(InOutViewFamily, InOutView);
	}

	// Update data on rendering thread
	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[This = SharedThis(this), ResourcesRenderThread = MoveTemp(PassResources)](FRHICommandListImmediate& RHICmdList)
		{
			//Caches render thread resource to be used when applying configuration in PostRenderViewFamily_RenderThread
			This->CachedResourcesRenderThread = ResourcesRenderThread;
		}
	);
}

bool FDisplayClusterViewport_OpenColorIO::IsConversionSettingsEqual(const FOpenColorIOColorConversionSettings& InConversionSettings) const
{
	return ConversionSettings.ToString() == InConversionSettings.ToString();
}

float FDisplayClusterViewport_OpenColorIO::GetDisplayGamma(const FDisplayClusterViewport_Context& InViewportContext) const
{
	const float DefaultDisplayGamma = FOpenColorIORendering::DefaultDisplayGamma;
	const float EngineDisplayGamma = InViewportContext.RenderThreadData.EngineDisplayGamma;

	// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
	// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
	const float DisplayGamma = (InViewportContext.RenderThreadData.EngineShowFlags.Tonemapper == 0) || (InViewportContext.RenderThreadData.EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / EngineDisplayGamma;

	return DisplayGamma;
}

bool FDisplayClusterViewport_OpenColorIO::AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
	FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect, bool bUnpremultiply, bool bInvertAlpha) const
{
	FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputTextureRHI, TEXT("nDisplayOCIOInputTexture")));
	FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputTextureRHI, TEXT("nDisplayOCIORenderTargetTexture")));

	FScreenPassRenderTarget Output = FScreenPassRenderTarget(OutputTexture, OutputRect, ERenderTargetLoadAction::EClear);
	EOpenColorIOTransformAlpha TransformAlpha = EOpenColorIOTransformAlpha::None;
	if (bUnpremultiply)
	{
		TransformAlpha = bInvertAlpha ? EOpenColorIOTransformAlpha::InvertUnpremultiply : EOpenColorIOTransformAlpha::Unpremultiply;
	}

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		FScreenPassViewInfo(),
		GMaxRHIFeatureLevel,
		FScreenPassTexture(InputTexture),
		Output,
		CachedResourcesRenderThread,
		GetDisplayGamma(InViewportContext),
		TransformAlpha
	);

	return true;
}

// This is a copy of FOpenColorIODisplayExtension::PostProcessPassAfterTonemap_RenderThread()
FScreenPassTexture FDisplayClusterViewport_OpenColorIO::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
{
	const FScreenPassTexture& SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());
	FScreenPassRenderTarget Output = InOutInputs.OverrideOutput;

	// If the override output is provided, it means that this is the last pass in post processing.
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, View.GetOverwriteLoadAction(), TEXT("OCIORenderTarget"));
	}

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		View,
		SceneColor,
		Output,
		CachedResourcesRenderThread
	);

	return MoveTemp(Output);
}
