// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersNeuralPostProcessingCS.h"

namespace UE::NNEHlslShaders::Internal
{
	void TNeuralPostProcessingReadInputCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNeuralPostProcessingConstants::THREAD_GROUP_SIZE);
	}

	void TNeuralPostProcessingPreStepCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNeuralPostProcessingConstants::THREAD_GROUP_SIZE);
	}

	void TNeuralPostProcessingPostStepCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNeuralPostProcessingConstants::THREAD_GROUP_SIZE);
	}

	void TNeuralPostProcessingWriteOutputPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNeuralPostProcessingConstants::THREAD_GROUP_SIZE);
	}

	IMPLEMENT_GLOBAL_SHADER(TNeuralPostProcessingReadInputCS, "/NNE/NNEHlslShadersNeuralPostProcessing.usf", "ReadInput", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(TNeuralPostProcessingPreStepCS, "/NNE/NNEHlslShadersNeuralPostProcessing.usf", "PreStep", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(TNeuralPostProcessingPostStepCS, "/NNE/NNEHlslShadersNeuralPostProcessing.usf", "PostStep", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(TNeuralPostProcessingWriteOutputPS, "/NNE/NNEHlslShadersNeuralPostProcessing.usf", "WriteOutput", SF_Pixel);
} // UE::NNEHlslShaders::Internal