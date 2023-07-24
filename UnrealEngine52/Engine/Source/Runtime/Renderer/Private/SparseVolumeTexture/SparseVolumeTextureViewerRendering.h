// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FScene;
class FSceneRenderer;
class FRDGBuilder;
struct FMinimalSceneTextures;
struct FSceneTextures;
struct FScreenPassTexture;


void AddSparseVolumeTextureViewerRenderPass(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, FSceneTextures& SceneTextures);

