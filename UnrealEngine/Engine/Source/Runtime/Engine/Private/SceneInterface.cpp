// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneInterface.h"
#include "RenderGraphBuilder.h"
#include "SceneTypes.h"
#include "SceneUtils.h"

FSceneInterface::FSceneInterface(ERHIFeatureLevel::Type InFeatureLevel)
	: FeatureLevel(InFeatureLevel)
{
}

void FSceneInterface::UpdateAllPrimitiveSceneInfos(FRHICommandListImmediate& RHICmdList)
{
	FRDGBuilder GraphBuilder(RHICmdList, FRDGEventName(TEXT("UpdateAllPrimitiveSceneInfos")));
	UpdateAllPrimitiveSceneInfos(GraphBuilder);
	GraphBuilder.Execute();
}

void FSceneInterface::ProcessAndRenderIlluminanceMeter(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views, FRDGTextureRef SceneColorTexture)
{
	// Deprecated
}

EShaderPlatform FSceneInterface::GetShaderPlatform() const
{
	return GShaderPlatformForFeatureLevel[GetFeatureLevel()];
}

EShadingPath FSceneInterface::GetShadingPath(ERHIFeatureLevel::Type InFeatureLevel)
{
	return GetFeatureLevelShadingPath(InFeatureLevel);
}
