// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserShadersOidnCS.h"

namespace UE::NNEDenoiserShaders::Internal
{
	void FNNEDenoiserOidnCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNNEDenoiserOidnConstants::THREAD_GROUP_SIZE);
	}

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserOidnCS, "/NNEDenoiserShaders/NNEDenoiserShadersOidn.usf", "PreOrPostprocess", SF_Compute);

} // UE::NNEDenoiser::Private