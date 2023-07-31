// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FViewInfo;

struct FShadingEnergyConservationStateData
{
	bool bEnergyConservation = false;
	bool bEnergyPreservation = false;
	EPixelFormat Format = PF_Unknown;
	TRefCountPtr<IPooledRenderTarget> GGXSpecEnergyTexture = nullptr;
	TRefCountPtr<IPooledRenderTarget> GGXGlassEnergyTexture = nullptr;
	TRefCountPtr<IPooledRenderTarget> ClothEnergyTexture = nullptr;
	TRefCountPtr<IPooledRenderTarget> DiffuseEnergyTexture = nullptr;

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

namespace ShadingEnergyConservation
{
	void Init(FRDGBuilder& GraphBuilder, FViewInfo& View);
	void Debug(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSceneTextures& SceneTextures);

	RENDERER_API bool IsEnable();
}
