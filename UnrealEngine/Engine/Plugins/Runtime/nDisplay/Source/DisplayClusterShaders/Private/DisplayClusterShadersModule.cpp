// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterShadersModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#include "Shaders/DisplayClusterShadersPreprocess_UVLightCards.h"
#include "Shaders/DisplayClusterShadersPostprocess_Blur.h"
#include "Shaders/DisplayClusterShadersPostprocess_OutputRemap.h"
#include "Shaders/DisplayClusterShadersWarpblend_ICVFX.h"
#include "Shaders/DisplayClusterShadersWarpblend_MPCDI.h"
#include "Shaders/DisplayClusterShadersGenerateMips.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IModuleInterface
//////////////////////////////////////////////////////////////////////////////////////////////

#define NDISPLAY_SHADERS_MAP TEXT("/Plugin/nDisplay")

void FDisplayClusterShadersModule::StartupModule()
{
	if (!AllShaderSourceDirectoryMappings().Contains(NDISPLAY_SHADERS_MAP))
	{
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("nDisplay"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(NDISPLAY_SHADERS_MAP, PluginShaderDir);
	}
}

void FDisplayClusterShadersModule::ShutdownModule()
{
}

bool FDisplayClusterShadersModule::RenderWarpBlend_MPCDI(FRHICommandListImmediate& RHICmdList, const FDisplayClusterShaderParameters_WarpBlend& InWarpBlendParameters) const
{
	return FDisplayClusterShadersWarpblend_MPCDI::RenderWarpBlend_MPCDI(RHICmdList, InWarpBlendParameters);
}

bool FDisplayClusterShadersModule::RenderWarpBlend_ICVFX(FRHICommandListImmediate& RHICmdList, const FDisplayClusterShaderParameters_WarpBlend& InWarpBlendParameters, const FDisplayClusterShaderParameters_ICVFX& InICVFXParameters) const
{
	return FDisplayClusterShadersWarpblend_ICVFX::RenderWarpBlend_ICVFX(RHICmdList, InWarpBlendParameters, InICVFXParameters);
}

bool FDisplayClusterShadersModule::RenderPreprocess_UVLightCards(FRHICommandListImmediate& RHICmdList, FSceneInterface* InScene, FRenderTarget* InRenderTarget, float ProjectionPlaneSize) const
{
	return FDisplayClusterShadersPreprocess_UVLightCards::RenderPreprocess_UVLightCards(RHICmdList, InScene, InRenderTarget, ProjectionPlaneSize);
}

bool FDisplayClusterShadersModule::RenderPostprocess_OutputRemap(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InSourceTexture, FRHITexture2D* InRenderTargetableDestTexture, const IDisplayClusterRender_MeshComponentProxy& MeshProxy) const
{
	return FDisplayClusterShadersPostprocess_OutputRemap::RenderPostprocess_OutputRemap(RHICmdList, InSourceTexture, InRenderTargetableDestTexture, MeshProxy);
}

bool FDisplayClusterShadersModule::RenderPostprocess_Blur(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InSourceTexture, FRHITexture2D* InRenderTargetableDestTexture, const FDisplayClusterShaderParameters_PostprocessBlur& InSettings) const
{
	return FDisplayClusterShadersPostprocess_Blur::RenderPostprocess_Blur(RHICmdList, InSourceTexture, InRenderTargetableDestTexture, InSettings);
}

bool FDisplayClusterShadersModule::GenerateMips(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InOutMipsTexture, const FDisplayClusterShaderParameters_GenerateMips& InSettings) const
{
	return FDisplayClusterShadersGenerateMips::GenerateMips(RHICmdList, InOutMipsTexture, InSettings);
}


IMPLEMENT_MODULE(FDisplayClusterShadersModule, DisplayClusterShaders);

