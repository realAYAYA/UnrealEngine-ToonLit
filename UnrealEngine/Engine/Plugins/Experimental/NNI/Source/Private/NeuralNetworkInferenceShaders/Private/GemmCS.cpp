// Copyright Epic Games, Inc. All Rights Reserved.

#include "GemmCS.h"



/* FGemmCS public functions
 *****************************************************************************/

void FGemmCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), THREADGROUP_SIZE_Y);
}



const uint32 FGemmCS::THREADGROUP_SIZE_X = 128;
const uint32 FGemmCS::THREADGROUP_SIZE_Y = 1;



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FGemmCS, "/Plugins/NeuralNetworkInference/Private/GemmOperator.usf", "GemmCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
