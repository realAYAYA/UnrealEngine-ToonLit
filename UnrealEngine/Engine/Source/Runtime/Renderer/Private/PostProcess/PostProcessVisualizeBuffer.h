// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OverridePassSequence.h"

// Returns whether the gbuffer visualization pass needs to render on screen.
bool IsVisualizeGBufferOverviewEnabled(const FViewInfo& View);

// Returns whether the gubffer visualization pass needs to dump targets to files.
bool IsVisualizeGBufferDumpToFileEnabled(const FViewInfo& View);

// Returns whether the gbuffer visualization pass needs to dump to a pipe.
bool IsVisualizeGBufferDumpToPipeEnabled(const FViewInfo& View);

// Returns whether the gbuffer visualization pass should output in floating point format.
bool IsVisualizeGBufferInFloatFormat();

struct FVisualizeGBufferOverviewInputs
{
	FScreenPassRenderTarget OverrideOutput;

	// The current scene color being processed.
	FScreenPassTexture SceneColor;

	// The HDR scene color immediately before tonemapping is applied.
	FScreenPassTexture SceneColorBeforeTonemap;

	// The scene color immediately after tonemapping is applied.
	FScreenPassTexture SceneColorAfterTonemap;

	// The separate translucency texture to composite.
	FScreenPassTexture SeparateTranslucency;

	// The original scene velocity texture to composite.
	FScreenPassTexture Velocity;

	/** The uniform buffer containing all scene textures. */
	FSceneTextureShaderParameters SceneTextures;

	// Dump targets to files on disk.
	bool bDumpToFile = false;

	// Render an overview of the GBuffer targets.
	bool bOverview = false;

	// Whether to emit outputs in HDR.
	bool bOutputInHDR = false;
};

FScreenPassTexture AddVisualizeGBufferOverviewPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeGBufferOverviewInputs& Inputs);