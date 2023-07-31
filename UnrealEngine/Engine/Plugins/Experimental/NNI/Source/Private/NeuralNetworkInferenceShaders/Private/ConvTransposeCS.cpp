// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvTransposeCS.h"



/* FConvTransposeCS static members
 *****************************************************************************/

const uint32 FConvTransposeCS::THREADGROUP_SIZE_X(128);



/* FConvTransposeCS public functions
 *****************************************************************************/

void FConvTransposeCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
}



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FConvTransposeCS, "/Plugins/NeuralNetworkInference/Private/ConvTransposeOperator.usf", "XToXWithZerosCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
