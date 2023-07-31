// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"

// TODO: Heavy work in progress and experiment (do not use!) vertex factory
// that performs Nanite base pass material shading with compute shaders 
// instead of vertex and pixel shaders. This will (likely) eventually replace
// the Nanite::FVertexFactory at some point.
class ENGINE_API FNaniteVertexFactory final : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FNaniteVertexFactory);

public:
	FNaniteVertexFactory(ERHIFeatureLevel::Type FeatureLevel);
	~FNaniteVertexFactory();

	virtual void InitRHI() override final;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
