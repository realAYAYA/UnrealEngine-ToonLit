// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserShadersDefaultCS.h"

namespace UE::NNEDenoiserShaders::Internal
{
	void FNNEDenoiserReadInputCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNNEDenoiserConstants::THREAD_GROUP_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_MAPPED_CHANNELS"), FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS);
	}

	void FNNEDenoiserWriteOutputCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNNEDenoiserConstants::THREAD_GROUP_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_MAPPED_CHANNELS"), FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS);
	}

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserReadInputCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "ReadInput", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserWriteOutputCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "WriteOutput", SF_Compute);

} // UE::NNEDenoiser::Private