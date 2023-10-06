// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthCopy.h: Depth copy utilities.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphUtils.h"

class FViewInfo;

// This is a temporary workaround while we get AddCopyTexturePass to do a proper copy of depth texture (with source texture HTile maintained).
// On some platforms this is not the case: depth is decompressed so that the depth format can be read through SRV and HTile optimizations are thus lost on the source texture.
// While we wait for such support, we do a simple copy from SRV to UAV.
void AddViewDepthCopyCSPass(FRDGBuilder& GraphBuilder, FViewInfo& View, FRDGTextureRef SourceSceneDepthTexture, FRDGTextureRef DestinationDepthTexture);

