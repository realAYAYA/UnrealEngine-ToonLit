// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "RHIDefinitions.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphFwd.h"

struct FStrataSceneData;
class FScene;
class FGlobalShaderMap;
class FRDGBuilder;
class FRHICommandListImmediate;
struct IPooledRenderTarget;
template <typename ReferencedType> class TRefCountPtr;

class RENDERER_API FRenderTargetWriteMask
{
public:
	static void Decode(
		FRHICommandListImmediate& RHICmdList,
		FGlobalShaderMap* ShaderMap,
		TArrayView<IPooledRenderTarget* const> InRenderTargets,
		TRefCountPtr<IPooledRenderTarget>& OutRTWriteMask,
		ETextureCreateFlags RTWriteMaskFastVRamConfig,
		const TCHAR* RTWriteMaskDebugName);

	static void Decode(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		TArrayView<FRDGTextureRef const> InRenderTargets,
		FRDGTextureRef& OutRTWriteMask,
		ETextureCreateFlags RTWriteMaskFastVRamConfig,
		const TCHAR* RTWriteMaskDebugName);
};

class RENDERER_API FDepthBounds
{
public:

	struct FDepthBoundsValues
	{
		float MinDepth;
		float MaxDepth;
	};

	static FDepthBoundsValues CalculateNearFarDepthExcludingSky();
};

// A minimal uniform struct providing necessary access for external systems to Strata parameters.
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FStrataPublicGlobalUniformParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, TopLayerTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

namespace Strata
{
	void PreInitViews(FScene& Scene);
	void PostRender(FScene& Scene);

	RENDERER_API TRDGUniformBufferRef<FStrataPublicGlobalUniformParameters> GetPublicGlobalUniformBuffer(FRDGBuilder& GraphBuilder, FScene& Scene);
}
