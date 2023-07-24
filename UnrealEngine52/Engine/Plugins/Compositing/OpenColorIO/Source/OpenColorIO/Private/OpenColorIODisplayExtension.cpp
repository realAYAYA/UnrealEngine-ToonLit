// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIODisplayExtension.h"

#include "Containers/SortedMap.h"
#include "CoreGlobals.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIODisplayManager.h"
#include "OpenColorIOModule.h"

#include "OpenColorIOShader.h"
#include "Shader.h"

#include "RHI.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
#include "Containers/DynamicRHIResourceArray.h"
// for FPostProcessMaterialInputs
#include "PostProcess/PostProcessMaterial.h"


float FOpenColorIODisplayExtension::DefaultDisplayGamma = 2.2f;

FOpenColorIODisplayExtension::FOpenColorIODisplayExtension(const FAutoRegister& AutoRegister, FViewportClient* AssociatedViewportClient)
	: FSceneViewExtensionBase(AutoRegister)
	, LinkedViewportClient(AssociatedViewportClient)
{
}

bool FOpenColorIODisplayExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if (Context.Viewport && LinkedViewportClient == Context.Viewport->GetClient() && DisplayConfiguration.bIsEnabled)
	{
		if (DisplayConfiguration.ColorConfiguration.IsValid())
		{
			return DisplayConfiguration.ColorConfiguration.ConfigurationSource->IsTransformReady(DisplayConfiguration.ColorConfiguration);
		}
	}

	return false;
}

void FOpenColorIODisplayExtension::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (DisplayConfiguration.ColorConfiguration.ConfigurationSource)
	{
		Collector.AddReferencedObject(DisplayConfiguration.ColorConfiguration.ConfigurationSource);
	}
}

void FOpenColorIODisplayExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	//Cache render resource so they are available on the render thread (Can't access UObjects on RT)
	//If something fails, cache invalid resources to invalidate them
	FOpenColorIOTransformResource* ShaderResource = nullptr;
	TSortedMap<int32, FTextureResource*> TransformTextureResources;
	const FOpenColorIOColorConversionSettings& ConversionSettings = DisplayConfiguration.ColorConfiguration;

	if (ConversionSettings.ConfigurationSource != nullptr)
	{
		const bool bFoundTransform = ConversionSettings.ConfigurationSource->GetRenderResources(
			InViewFamily.GetFeatureLevel()
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
				//Force ToneCurve to be off while we'are alive to make sure the input color space is the working space : srgb linear
				InViewFamily.EngineShowFlags.SetToneCurve(false);
				// This flags sets tonampper to output to ETonemapperOutputDevice::LinearNoToneCurve
				InViewFamily.SceneCaptureSource = SCS_FinalColorHDR;

				InView.FinalPostProcessSettings.bOverride_ToneCurveAmount = 1;
				InView.FinalPostProcessSettings.ToneCurveAmount = 0.0;
			}
		}
	}

	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[this, ShaderResource, TextureResources = MoveTemp(TransformTextureResources), TransformName = ConversionSettings.ToString()](FRHICommandListImmediate& RHICmdList)
		{
			//Caches render thread resource to be used when applying configuration in PostRenderViewFamily_RenderThread
			CachedResourcesRenderThread.ShaderResource = ShaderResource;
			CachedResourcesRenderThread.TextureResources = TextureResources;
			CachedResourcesRenderThread.TransformName = TransformName;
		}
	);
}

void FOpenColorIODisplayExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (PassId == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FOpenColorIODisplayExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

FScreenPassTexture FOpenColorIODisplayExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
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

	const float EngineDisplayGamma = View.Family->RenderTarget->GetDisplayGamma();
	// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
	// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
	float DisplayGamma = (View.Family->EngineShowFlags.Tonemapper == 0) || (View.Family->EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / EngineDisplayGamma;

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		ViewInfo,
		SceneColor,
		Output,
		CachedResourcesRenderThread,
		DisplayGamma
	);

	return MoveTemp(Output);
}