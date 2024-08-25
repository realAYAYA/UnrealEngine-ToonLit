// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FRDGBuilder;
class FSceneRenderer;
class FScene;


void RenderWaterInfoTexture(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, const FScene* Scene);