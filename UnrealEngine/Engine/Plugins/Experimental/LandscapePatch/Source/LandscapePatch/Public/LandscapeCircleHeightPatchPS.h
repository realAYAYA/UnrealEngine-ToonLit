// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

/**
 * Shader that applies a circle height patch to a landscape heightmap.
 */
class LANDSCAPEPATCH_API FLandscapeCircleHeightPatchPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLandscapeCircleHeightPatchPS);
	SHADER_USE_PARAMETER_STRUCT(FLandscapeCircleHeightPatchPS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceHeightmap) // Our input texture
		SHADER_PARAMETER(FVector3f, InCenter)
		SHADER_PARAMETER(float, InRadius)
		SHADER_PARAMETER(float, InFalloff)
		RENDER_TARGET_BINDING_SLOTS() // Holds our output
		END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static void AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters);
};