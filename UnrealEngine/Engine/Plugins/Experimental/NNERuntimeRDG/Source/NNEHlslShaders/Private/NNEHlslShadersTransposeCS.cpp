// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersTransposeCS.h"
#include "NNE.h"

namespace UE::NNEHlslShaders::Internal
{
	void FTransposeCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), FTransposeConstants::NUM_GROUP_THREADS);

		FPermutationDomain PermutationVector(InParameters.PermutationId);
	}

	IMPLEMENT_GLOBAL_SHADER(FTransposeCS, "/NNE/NNEHlslShadersTranspose.usf", "Transpose", SF_Compute);
} // UE::NNEHlslShaders::Internal