// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchUtil.h"

#include "LandscapeTexturePatchPS.h" // FSimpleTextureCopyPS
#include "LandscapeTextureBackedRenderTarget.h"
#include "RenderGraph.h"

void UE::Landscape::PatchUtil::CopyTextureOnRenderThread(FRHICommandListImmediate& RHICmdList, 
	const FTextureResource& Source, FTextureResource& Destination)
{
	using namespace UE::Landscape;

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("LandscapeTexturePatchCopyTexture"));

	FRDGTextureRef SourceTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Source.GetTexture2DRHI(), TEXT("CopySource")));
	FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Destination.GetTexture2DRHI(), TEXT("CopyDestination")));

	// All my efforts of getting CopyToResolveTarget to work without complaints have failed, so we just use our own copy shader.
	FSimpleTextureCopyPS::AddToRenderGraph(GraphBuilder, SourceTexture, DestinationTexture);

	GraphBuilder.Execute();
}
