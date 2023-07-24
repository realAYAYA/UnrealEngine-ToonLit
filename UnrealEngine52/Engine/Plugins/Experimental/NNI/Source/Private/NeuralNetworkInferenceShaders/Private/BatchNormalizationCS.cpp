// Copyright Epic Games, Inc. All Rights Reserved.

#include "BatchNormalizationCS.h"



/* FBatchNormalizationCS public functions
 *****************************************************************************/

void FBatchNormalizationCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
}



const uint32 FBatchNormalizationCS::THREADGROUP_SIZE_X = 128;



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FBatchNormalizationCS, "/Plugins/NeuralNetworkInference/Private/BatchNormalizationOperator.usf", "BatchNormalizationCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
