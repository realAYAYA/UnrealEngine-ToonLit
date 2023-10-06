// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "SceneRendering.h"

class FMobileSceneTextureUniformParameters;
class FRDGBuilder;
class FRHICommandListImmediate;
class FScene;
class FViewInfo;
struct FRenderTargetBindingSlots;
struct FSortedLightSetSceneInfo;

extern int32 GMobileUseClusteredDeferredShading;

void MobileDeferredShadingPass(
	FRHICommandList& RHICmdList,
	int32 ViewIndex,
	int32 NumViews,
	const FViewInfo& View,
	const FScene& Scene, 
	const FSortedLightSetSceneInfo& SortedLightSet,
	const TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos);
