// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneRendering.h"

class FDistanceFieldAOParameters;

extern void AllocateOrReuseAORenderTarget(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef& Target, const TCHAR* Name, EPixelFormat Format, ETextureCreateFlags Flags = TexCreate_None);

extern void UpdateHistory(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const TCHAR* BentNormalHistoryRTName,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformBuffer,
	FRDGTextureRef VelocityTexture,
	FRDGTextureRef BentNormalInterpolation,
	FRDGTextureRef DistanceFieldNormal,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	FIntRect* DistanceFieldAOHistoryViewRect,
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	FRDGTextureRef& BentNormalHistoryOutput,
	const FDistanceFieldAOParameters& Parameters);

extern void UpsampleBentNormalAO(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformBuffer,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef DistanceFieldAOBentNormal,
	bool bModulateSceneColor);
