// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "RHIDefinitions.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"

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
