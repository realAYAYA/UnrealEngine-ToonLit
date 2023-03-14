// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCaptureRendering.h: SceneCaptureRendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "SceneTextures.h"

RENDERER_API void CopySceneCaptureComponentToTarget(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef ViewFamilyTexture,
	const FSceneViewFamily& ViewFamily,
	const TArray<FViewInfo>& Views);
