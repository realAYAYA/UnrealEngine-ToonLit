// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIORendering.h"

#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineModule.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "Logging/MessageLog.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOModule.h"
#include "OpenColorIOShader.h"
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "OpenColorIOColorTransform.h"
#include "SceneInterface.h"
#include "ScreenPass.h"
#include "TextureResource.h"


float FOpenColorIORendering::DefaultDisplayGamma = 2.2f;

// static
void FOpenColorIORendering::AddPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo ViewInfo,
	ERHIFeatureLevel::Type FeatureLevel,
	const FScreenPassTexture& Input,
	const FScreenPassRenderTarget& Output,
	const FOpenColorIORenderPassResources& InPassResource,
	float InGamma, 
	EOpenColorIOTransformAlpha TransformAlpha)
{
	check(IsInRenderingThread());

	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

	bool bRenderPassSuccessed = false;

	TSortedMap<int32, FTextureResource*> TextureResources;
	TextureResources.Reserve(InPassResource.TextureResources.Num());

	for (const TPair<int32, TWeakObjectPtr<UTexture>>& Pair : InPassResource.TextureResources)
	{
		if (Pair.Value.IsValid(false, true))
		{
			// UTexture's GetResource() is safe to call from the render thread.
			if (FTextureResource* TextureResource = Pair.Value->GetResource())
			{
				TextureResources.Add(Pair.Key, TextureResource);
			}
		}
	}

	const bool bAllExpectedTexturesResources = TextureResources.Num() == InPassResource.TextureResources.Num();

	if (bAllExpectedTexturesResources && InPassResource.ShaderResource != nullptr)
	{
		TShaderRef<FOpenColorIOPixelShader> OCIOPixelShader = InPassResource.ShaderResource->GetShader<FOpenColorIOPixelShader>();

		FOpenColorIOPixelShaderParameters* Parameters = GraphBuilder.AllocParameters<FOpenColorIOPixelShaderParameters>();
		Parameters->InputTexture = Input.Texture;
		Parameters->InputTextureSampler = TStaticSamplerState<>::GetRHI();
		if (OpenColorIOBindTextureResources(Parameters, TextureResources))
		{
			Parameters->Gamma = InGamma;
			Parameters->TransformAlpha = (uint32)TransformAlpha;
			Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

			FRDGEventName PassName = RDG_EVENT_NAME("OpenColorIOPass %dx%d (%s)", Output.ViewRect.Width(), Output.ViewRect.Height(), InPassResource.TransformName.IsEmpty() ? TEXT("Unspecified Transform") : *InPassResource.TransformName);
			AddDrawScreenPass(GraphBuilder, MoveTemp(PassName), ViewInfo, OutputViewport, InputViewport, VertexShader, OCIOPixelShader, Parameters);

			bRenderPassSuccessed = true;
		}
	}

	if(!bRenderPassSuccessed)
	{
		// Fallback pass, printing invalid message across the viewport.
		TShaderMapRef<FOpenColorIOInvalidPixelShader> OCIOInvalidPixelShader(ShaderMap);
		FOpenColorIOInvalidShaderParameters* Parameters = GraphBuilder.AllocParameters<FOpenColorIOInvalidShaderParameters>();
		Parameters->InputTexture = Input.Texture;
		Parameters->InputTextureSampler = TStaticSamplerState<>::GetRHI();
		Parameters->MiniFontTexture = OpenColorIOGetMiniFontTexture();
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		FRDGEventName PassName = RDG_EVENT_NAME("OpenColorIOInvalidPass %dx%d", Output.ViewRect.Width(), Output.ViewRect.Height());
		AddDrawScreenPass(GraphBuilder, MoveTemp(PassName), ViewInfo, OutputViewport, InputViewport, VertexShader, OCIOInvalidPixelShader, Parameters);
	}
}

// static
void FOpenColorIORendering::AddPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FScreenPassTexture& Input,
	const FScreenPassRenderTarget& Output,
	const FOpenColorIORenderPassResources& InPassInfo,
	EOpenColorIOTransformAlpha TransformAlpha) 
{
	const float EngineDisplayGamma = View.Family->RenderTarget->GetDisplayGamma();
	// There is a special case where post processing and tonemapper are disabled. In this case tonemapper applies a static display Inverse of Gamma which defaults to 2.2.
	// In the case when Both PostProcessing and ToneMapper are disabled we apply gamma manually. In every other case we apply inverse gamma before applying OCIO.
	float DisplayGamma = (View.Family->EngineShowFlags.Tonemapper == 0) || (View.Family->EngineShowFlags.PostProcessing == 0) ? DefaultDisplayGamma : DefaultDisplayGamma / EngineDisplayGamma;

	FOpenColorIORendering::AddPass_RenderThread(
		GraphBuilder,
		View,
		View.GetFeatureLevel(),
		Input,
		Output,
		InPassInfo,
		DisplayGamma,
		TransformAlpha
	);
}

void FOpenColorIORendering::PrepareView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	//Force ToneCurve to be off while we'are alive to make sure the input color space is the working space : srgb linear
	InViewFamily.EngineShowFlags.SetToneCurve(false);
	// This flags sets tonampper to output to ETonemapperOutputDevice::LinearNoToneCurve
	InViewFamily.SceneCaptureSource = SCS_FinalColorHDR;

	InView.FinalPostProcessSettings.bOverride_ToneCurveAmount = 1;
	InView.FinalPostProcessSettings.ToneCurveAmount = 0.0;
}

// static
FOpenColorIORenderPassResources FOpenColorIORendering::GetRenderPassResources(const FOpenColorIOColorConversionSettings& InSettings, ERHIFeatureLevel::Type InFeatureLevel)
{
	FOpenColorIORenderPassResources Result = { nullptr, {}, InSettings.ToString() };

	if (InSettings.ConfigurationSource != nullptr)
	{
		const bool bFoundTransform = InSettings.ConfigurationSource->GetRenderResources(
			InFeatureLevel,
			InSettings,
			Result.ShaderResource,
			Result.TextureResources
		);

		if (bFoundTransform)
		{
			// Transform was found, so shader must be there but doesn't mean the actual shader is available
			check(Result.ShaderResource);
			if (Result.ShaderResource->GetShaderGameThread<FOpenColorIOPixelShader>().IsNull())
			{
				//Invalidate shader resource
				Result.ShaderResource = nullptr;
			}
		}
	}

	return Result;
}

// static
bool FOpenColorIORendering::ApplyColorTransform(UWorld* InWorld, const FOpenColorIOColorConversionSettings& InSettings, UTexture* InTexture, UTextureRenderTarget2D* OutRenderTarget)
{
	check(IsInGameThread());

	if (!ensureMsgf(InTexture, TEXT("Can't apply color transform - Invalid Input Texture")))
	{
		return false;
	}

	if (!ensureMsgf(OutRenderTarget, TEXT("Can't apply color transform - Invalid Output Texture")))
	{
		return false;
	}

	FTextureResource* InputResource = InTexture->GetResource();
	FTextureResource* OutputResource = OutRenderTarget->GetResource();
	if (!ensureMsgf(InputResource, TEXT("Can't apply color transform - Invalid Input Texture resource")))
	{
		return false;
	}

	if (!ensureMsgf(OutputResource, TEXT("Can't apply color transform - Invalid Output Texture resource")))
	{
		return false;
	}

	const ERHIFeatureLevel::Type FeatureLevel = InWorld->Scene->GetFeatureLevel();
	FOpenColorIOTransformResource* ShaderResource = nullptr;
	TSortedMap<int32, TWeakObjectPtr<UTexture>> TransformTextures;

	if (InSettings.ConfigurationSource != nullptr)
	{
		const bool bFoundTransform = InSettings.ConfigurationSource->GetRenderResources(FeatureLevel, InSettings, ShaderResource, TransformTextures);

		if (bFoundTransform)
		{
			check(ShaderResource);
			if (ShaderResource->GetShaderGameThread<FOpenColorIOPixelShader>().IsNull())
			{
				ensureMsgf(false, TEXT("Can't apply display look - Shader was invalid for Resource %s"), *ShaderResource->GetFriendlyName());

				//Invalidate shader resource
				ShaderResource = nullptr;
			}
		}
	}
	
	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[FeatureLevel, InputResource, OutputResource, ShaderResource, Textures = MoveTemp(TransformTextures), TransformName = InSettings.ToString()](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputResource->TextureRHI, TEXT("OCIOInputTexture")));
			FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputResource->TextureRHI, TEXT("OCIORenderTargetTexture")));
			FIntPoint  OutputResolution = FIntPoint(OutputResource->GetSizeX(), OutputResource->GetSizeY());
			FScreenPassRenderTarget Output = FScreenPassRenderTarget(OutputTexture, FIntRect(FIntPoint::ZeroValue, OutputResolution), ERenderTargetLoadAction::EClear);

			AddPass_RenderThread(
				GraphBuilder,
				FScreenPassViewInfo(),
				FeatureLevel,
				FScreenPassTexture(InputTexture),
				Output,
				FOpenColorIORenderPassResources{ShaderResource, Textures, TransformName},
				1.0f); // Set Gamma to 1., since we do not have any display parameters or requirement for Gamma.

			GraphBuilder.Execute();
		}
	);

	return ShaderResource != nullptr;
}
