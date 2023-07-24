// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelFormat.h"
#include "RenderGraphFwd.h"
#include "RHIFwd.h"

struct FBuildHZBAsyncComputeParams
{
	FRDGPassRef Prerequisite = nullptr;
};

static constexpr EPixelFormat BuildHZBDefaultPixelFormat = PF_R16F;

void BuildHZB(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VisBufferTexture,
	const FIntRect ViewRect,
	ERHIFeatureLevel::Type FeatureLevel,
	EShaderPlatform ShaderPlatform,
	const TCHAR* ClosestHZBName,
	FRDGTextureRef* OutClosestHZBTexture,
	const TCHAR* FurthestHZBName,
	FRDGTextureRef* OutFurthestHZBTexture,
	EPixelFormat Format = BuildHZBDefaultPixelFormat,
	const FBuildHZBAsyncComputeParams* AsyncComputeParams = nullptr);

// Build only the furthest HZB
void BuildHZBFurthest(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VisBufferTexture,
	const FIntRect ViewRect,
	ERHIFeatureLevel::Type FeatureLevel,
	EShaderPlatform ShaderPlatform,
	const TCHAR* FurthestHZBName,
	FRDGTextureRef* OutFurthestHZBTexture,
	EPixelFormat Format = BuildHZBDefaultPixelFormat,
	const FBuildHZBAsyncComputeParams* AsyncComputeParams = nullptr);
