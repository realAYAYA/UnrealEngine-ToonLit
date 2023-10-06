// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FRDGBuilder;
class FScene;
class FViewInfo;
struct FMinimalSceneTextures;

namespace Nanite
{

void DrawVisibleBricks(
	FRDGBuilder& GraphBuilder,
	FScene& Scene,
	const FViewInfo& View,
	FMinimalSceneTextures& SceneTextures );

}