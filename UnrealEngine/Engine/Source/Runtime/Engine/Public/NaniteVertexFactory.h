// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"

// Vertex factory that performs Nanite base pass material shading with 
// compute shaders instead of vertex and pixel shaders.
class FNaniteVertexFactory final : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE_API(FNaniteVertexFactory, ENGINE_API);

public:
	FNaniteVertexFactory(ERHIFeatureLevel::Type FeatureLevel);
	ENGINE_API ~FNaniteVertexFactory();

	ENGINE_API virtual void InitRHI(FRHICommandListBase& RHICmdList) override final;

	static ENGINE_API bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static ENGINE_API void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
