// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"

#include "OpenColorIOConfiguration.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIODisplayManager.h"
#include "OpenColorIODisplayExtension.h"
#include "OpenColorIOShader.h"

#include "Shader.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
// for FPostProcessMaterialInputs
#include "PostProcess/PostProcessMaterial.h"

namespace {
	/** Create dummy view for OCIO render pass
	 *
	 * @param InViewRect - input texture view rect
	 *
	 * @return - new view info
	 */
	FViewInfo CreateDummyViewInfo(const FIntRect& InViewRect)
	{
		FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
			.SetTime(FGameTime())
			.SetGammaCorrection(1.0f));
		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SetViewRectangle(InViewRect);
		ViewInitOptions.ViewOrigin = FVector::ZeroVector;
		ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
		ViewInitOptions.ProjectionMatrix = FMatrix::Identity;

		return FViewInfo(ViewInitOptions);
	}
}

//////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport_OpenColorIO
//////////////////////////////////////////////////////////////////////////
FDisplayClusterViewport_OpenColorIO::FDisplayClusterViewport_OpenColorIO(const FOpenColorIOColorConversionSettings& InDisplayConfiguration)
	: ConversionSettings(InDisplayConfiguration)
{ }

FDisplayClusterViewport_OpenColorIO::~FDisplayClusterViewport_OpenColorIO()
{ }

void FDisplayClusterViewport_OpenColorIO::SetupSceneView(FSceneViewFamily& InOutViewFamily, FSceneView& InOutView) const
{
	if (bShaderResourceValid)
	{
		// Code copied from FOpenColorIODisplayExtension::SetupView():

		//Force ToneCurve to be off while we'are alive to make sure the input color space is the working space : srgb linear
		InOutViewFamily.EngineShowFlags.SetToneCurve(false);
		// This flags sets tonampper to output to ETonemapperOutputDevice::LinearNoToneCurve
		InOutViewFamily.SceneCaptureSource = SCS_FinalColorHDR;

		InOutView.FinalPostProcessSettings.bOverride_ToneCurveAmount = 1;
		InOutView.FinalPostProcessSettings.ToneCurveAmount = 0.0;
	}
}

void FDisplayClusterViewport_OpenColorIO::UpdateOpenColorIORenderPassResources()
{
	bShaderResourceValid = false;
	bIsEnabled = ConversionSettings.IsValid() ? ConversionSettings.ConfigurationSource->IsTransformReady(ConversionSettings) : false;

	if (!bIsEnabled)
	{
		// The data in this configuration was not correctly initialized in the previous frame. Ignore this OCIO
		return;
	}

	FOpenColorIOTransformResource* ShaderResource = nullptr;
	TSortedMap<int32, FTextureResource*> TransformTextureResources;

	if (ConversionSettings.ConfigurationSource != nullptr)
	{
		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

		const bool bFoundTransform = ConversionSettings.ConfigurationSource->GetRenderResources(
			FeatureLevel
			, ConversionSettings
			, ShaderResource
			, TransformTextureResources);

		if (bFoundTransform)
		{
			// Transform was found, so shader must be there but doesn't mean the actual shader is available
			check(ShaderResource);
			if (ShaderResource->GetShaderGameThread<FOpenColorIOPixelShader>().IsNull())
			{
				ensureMsgf(false, TEXT("Can't apply display look - Shader was invalid for Resource %s"), *ShaderResource->GetFriendlyName());

				//Invalidate shader resource
				ShaderResource = nullptr;
			}
			else
			{
				bShaderResourceValid = true;
			}
		}
	}

	if (!bShaderResourceValid)
	{
		ShaderResource = nullptr;
		TransformTextureResources.Empty();
	}

	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[This = SharedThis(this), ShaderResource, TextureResources = MoveTemp(TransformTextureResources), TransformName = ConversionSettings.ToString()](FRHICommandListImmediate& RHICmdList)
		{
			//Caches render thread resource to be used when applying configuration in PostRenderViewFamily_RenderThread
			This->CachedResourcesRenderThread.ShaderResource = ShaderResource;
			This->CachedResourcesRenderThread.TextureResources = TextureResources;
			This->CachedResourcesRenderThread.TransformName = TransformName;
		}
	);
}

bool FDisplayClusterViewport_OpenColorIO::IsEnabled_RenderThread() const
{
	return bIsEnabled;
}

bool FDisplayClusterViewport_OpenColorIO::IsConversionSettingsEqual(const FOpenColorIOColorConversionSettings& InConversionSettings) const
{
	return ConversionSettings.ToString() == InConversionSettings.ToString();
}

float FDisplayClusterViewport_OpenColorIO::GetDisplayGamma(const FDisplayClusterViewport_Context& InViewportContext) const
{
	const float DefaultDisplayGamma = FOpenColorIODisplayExtension::DefaultDisplayGamma;
	const float EngineDisplayGamma = InViewportContext.RenderThreadData.EngineDisplayGamma;

	// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
	// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
	const float DisplayGamma = (InViewportContext.RenderThreadData.EngineShowFlags.Tonemapper == 0) || (InViewportContext.RenderThreadData.EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / EngineDisplayGamma;

	return DisplayGamma;
}

bool FDisplayClusterViewport_OpenColorIO::AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
	FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect, bool bUnpremultiply, bool bInvertAlpha) const
{
	if (IsEnabled_RenderThread())
	{
		FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputTextureRHI, TEXT("nDisplayOCIOInputTexture")));
		FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputTextureRHI, TEXT("nDisplayOCIORenderTargetTexture")));

		FScreenPassRenderTarget Output = FScreenPassRenderTarget(OutputTexture, OutputRect, ERenderTargetLoadAction::EClear);
		EOpenColorIOTransformAlpha TransformAlpha = EOpenColorIOTransformAlpha::None;
		if (bUnpremultiply)
		{
			TransformAlpha = bInvertAlpha ? EOpenColorIOTransformAlpha::InvertUnpremultiply: EOpenColorIOTransformAlpha::Unpremultiply;
		}

		FOpenColorIORendering::AddPass_RenderThread(
			GraphBuilder,
			CreateDummyViewInfo(Output.ViewRect),
			FScreenPassTexture(InputTexture),
			Output,
			CachedResourcesRenderThread,
			GetDisplayGamma(InViewportContext),
			TransformAlpha
		);

		return true;
	}

	return false;
}

// This is a copy of FOpenColorIODisplayExtension::PostProcessPassAfterTonemap_RenderThread()
FScreenPassTexture FDisplayClusterViewport_OpenColorIO::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
{
	const FScreenPassTexture& SceneColor = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());
	checkSlow(View.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FScreenPassRenderTarget Output = InOutInputs.OverrideOutput;

	// If the override output is provided, it means that this is the last pass in post processing.
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, ViewInfo.GetOverwriteLoadAction(), TEXT("OCIORenderTarget"));
	}

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		ViewInfo,
		SceneColor,
		Output,
		CachedResourcesRenderThread,
		GetDisplayGamma(InViewportContext)
	);

	return MoveTemp(Output);
}
