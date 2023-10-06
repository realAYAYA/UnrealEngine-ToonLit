// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersUpsampleCS.h"
#include "NNE.h"

namespace UE::NNEHlslShaders::Internal
{
	void FUpsampleCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), FUpsampleConstants::NUM_GROUP_THREADS);

		FPermutationDomain PermutationVector(InParameters.PermutationId);
	}

	IMPLEMENT_GLOBAL_SHADER(FUpsampleCS, "/NNE/NNEHlslShadersUpsample.usf", "Upsample", SF_Compute);
} // UE::NNEHlslShaders::Internal