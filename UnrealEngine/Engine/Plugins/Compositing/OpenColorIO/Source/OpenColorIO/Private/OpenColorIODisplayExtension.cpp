// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIODisplayExtension.h"

#include "Containers/SortedMap.h"
#include "CoreGlobals.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIODisplayManager.h"
#include "OpenColorIOModule.h"
#include "SceneView.h"

#include "GlobalShader.h"
#include "OpenColorIOShader.h"
#include "PipelineStateCache.h"
#include "RenderTargetPool.h"
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
		return DisplayConfiguration.ColorConfiguration.IsValid();
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

	if (DisplayConfiguration.ColorConfiguration.ConfigurationSource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply display look - Invalid config asset"));
	}
	else
	{
		const bool bFoundTransform = DisplayConfiguration.ColorConfiguration.ConfigurationSource->GetRenderResources(
			InViewFamily.GetFeatureLevel()
			, DisplayConfiguration.ColorConfiguration
			, ShaderResource
			, TransformTextureResources);

		if (!bFoundTransform)
		{
			UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply display look - Couldn't find shader to transform from %s to %s"), *DisplayConfiguration.ColorConfiguration.SourceColorSpace.ColorSpaceName, *DisplayConfiguration.ColorConfiguration.DestinationColorSpace.ColorSpaceName);
		}
		else
		{
			// Transform was found, so shader must be there but doesn't mean the actual shader is available
			check(ShaderResource);
			if (ShaderResource->GetShaderGameThread<FOpenColorIOPixelShader>().IsNull())
			{
				UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply display look - Shader was invalid for Resource %s"), *ShaderResource->GetFriendlyName());

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
		[this, ShaderResource, TextureResources = MoveTemp(TransformTextureResources)](FRHICommandListImmediate& RHICmdList)
		{
			//Caches render thread resource to be used when applying configuration in PostRenderViewFamily_RenderThread
			CachedResourcesRenderThread.ShaderResource = ShaderResource;
			CachedResourcesRenderThread.TextureResources = TextureResources;
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
	const FSceneViewFamily& ViewFamily = *View.Family;
	const FScreenPassTexture& SceneColor = InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());

	//If the shader resource could not be found, skip this frame. The LUT isn't required
	if (CachedResourcesRenderThread.ShaderResource == nullptr)
	{
		return SceneColor;
	}

	if (!EnumHasAnyFlags(SceneColor.Texture->Desc.Flags, TexCreate_ShaderResource))
	{
		return SceneColor;
	}

	checkSlow(View.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	
	FScreenPassRenderTarget Output = InOutInputs.OverrideOutput;

	// If the override output is provided it means that this is the last pass in post processing.
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, ViewInfo.GetOverwriteLoadAction(), TEXT("OCIORenderTarget"));
	}

	const FScreenPassTextureViewport InputViewport(SceneColor);
	const FScreenPassTextureViewport OutputViewport(Output);

	TShaderRef<FOpenColorIOPixelShader> OCIOPixelShader = CachedResourcesRenderThread.ShaderResource->GetShader<FOpenColorIOPixelShader>();

	const float DisplayGamma = View.Family->RenderTarget->GetDisplayGamma();

	FOpenColorIOPixelShaderParameters* Parameters = GraphBuilder.AllocParameters<FOpenColorIOPixelShaderParameters>();
	Parameters->InputTexture = SceneColor.Texture;
	Parameters->InputTextureSampler = TStaticSamplerState<>::GetRHI();
	OpenColorIOBindTextureResources(Parameters, CachedResourcesRenderThread.TextureResources);

	// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
	// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
	Parameters->Gamma = (ViewFamily.EngineShowFlags.Tonemapper == 0) || (ViewFamily.EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / DisplayGamma;
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("OCIODisplayLook"), ViewInfo, OutputViewport, InputViewport, OCIOPixelShader, Parameters);

	return MoveTemp(Output);
}