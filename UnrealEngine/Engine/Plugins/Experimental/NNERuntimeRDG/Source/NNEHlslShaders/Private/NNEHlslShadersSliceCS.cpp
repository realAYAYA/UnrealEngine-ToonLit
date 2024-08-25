// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersSliceCS.h"
#include "NNE.h"

namespace UE::NNEHlslShaders::Internal
{
	void FSliceCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), FSliceConstants::NUM_GROUP_THREADS);

		FPermutationDomain PermutationVector(InParameters.PermutationId);
	}

	IMPLEMENT_GLOBAL_SHADER(FSliceCS, "/NNE/NNEHlslShadersSlice.usf", "Slice", SF_Compute);
} // UE::NNEHlslShaders::Internal