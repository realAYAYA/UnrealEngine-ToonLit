// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIORendering.h"

#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "Logging/MessageLog.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOModule.h"
#include "OpenColorIOShader.h"
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "OpenColorIOColorTransform.h"
#include "ScreenPass.h"
#include "TextureResource.h"


void ProcessOCIOColorSpaceTransform_RenderThread(
	FRHICommandListImmediate& InRHICmdList
	, ERHIFeatureLevel::Type InFeatureLevel
	, FOpenColorIOTransformResource* InOCIOColorTransformResource
	, const TSortedMap<int32, FTextureResource*>& InTextureResources
	, FTextureRHIRef InputSpaceColorTexture
	, FTextureRHIRef OutputSpaceColorTexture
	, FIntPoint OutputResolution)
{
	check(IsInRenderingThread());

	FRDGBuilder GraphBuilder(InRHICmdList);

	FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputSpaceColorTexture->GetTexture2D(), TEXT("OCIOInputTexture")));
	FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputSpaceColorTexture->GetTexture2D(), TEXT("OCIORenderTargetTexture")));
	FScreenPassTextureViewport Viewport = FScreenPassTextureViewport(OutputTexture);
	FScreenPassRenderTarget ScreenPassRenderTarget = FScreenPassRenderTarget(OutputTexture, FIntRect(FIntPoint::ZeroValue, OutputResolution), ERenderTargetLoadAction::EClear);

	TShaderRef<FOpenColorIOPixelShader> OCIOPixelShader = InOCIOColorTransformResource->GetShader<FOpenColorIOPixelShader>();

	FOpenColorIOPixelShaderParameters* Parameters = GraphBuilder.AllocParameters<FOpenColorIOPixelShaderParameters>();
	Parameters->InputTexture = InputTexture;
	Parameters->InputTextureSampler = TStaticSamplerState<>::GetRHI();
	OpenColorIOBindTextureResources(Parameters, InTextureResources);
	// Set Gamma to 1., since we do not have any display parameters or requirement for Gamma.
	Parameters->Gamma = 1.0;
	Parameters->RenderTargets[0] = ScreenPassRenderTarget.GetRenderTargetBinding();

	//Dummy ViewFamily/ViewInfo created for AddDrawScreenPass.
	FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime())
		.SetGammaCorrection(1.0f));
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.SetViewRectangle(ScreenPassRenderTarget.ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = FMatrix::Identity;
	FViewInfo DummyView(ViewInitOptions);

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("ProcessOCIOColorSpaceXfrm"), DummyView, Viewport, Viewport, OCIOPixelShader, Parameters);

	GraphBuilder.Execute();
}

// static
bool FOpenColorIORendering::ApplyColorTransform(UWorld* InWorld, const FOpenColorIOColorConversionSettings& InSettings, UTexture* InTexture, UTextureRenderTarget2D* OutRenderTarget)
{
	check(IsInGameThread());

	if (InSettings.ConfigurationSource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid config asset"));
		return false;
	}

	if (InTexture == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Input Texture"));
		return false;
	}

	if (OutRenderTarget == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Output Texture"));
		return false;
	}


	FTextureResource* InputResource = InTexture->GetResource();
	FTextureResource* OutputResource = OutRenderTarget->GetResource();
	if (InputResource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Input Texture resource"));
		return false;
	}

	if (OutputResource == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Invalid Output Texture resource"));
		return false;
	}

	const ERHIFeatureLevel::Type FeatureLevel = InWorld->Scene->GetFeatureLevel();
	FOpenColorIOTransformResource* ShaderResource = nullptr;
	TSortedMap<int32, FTextureResource*> TextureResources;
	bool bFoundTransform = InSettings.ConfigurationSource->GetRenderResources(FeatureLevel, InSettings, ShaderResource, TextureResources);
	if (!bFoundTransform)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Can't apply color transform - Couldn't find shader to transform from %s to %s"), *InSettings.SourceColorSpace.ColorSpaceName, *InSettings.DestinationColorSpace.ColorSpaceName);
		return false;
	}

	check(ShaderResource);

	if (ShaderResource->GetShaderGameThread<FOpenColorIOPixelShader>().IsNull())
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("OCIOPass - Shader was invalid for Resource %s"), *ShaderResource->GetFriendlyName());
		return false;
	}


	ENQUEUE_RENDER_COMMAND(ProcessColorSpaceTransform)(
		[FeatureLevel, InputResource, OutputResource, ShaderResource, TextureResources](FRHICommandListImmediate& RHICmdList)
		{
			ProcessOCIOColorSpaceTransform_RenderThread(
			RHICmdList,
			FeatureLevel,
			ShaderResource,
			TextureResources,
			InputResource->TextureRHI,
			OutputResource->TextureRHI,
			FIntPoint(OutputResource->GetSizeX(), OutputResource->GetSizeY()));
		}
	);
	return true;
}
