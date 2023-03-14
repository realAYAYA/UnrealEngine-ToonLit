// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvBaseCS.h"



/* FConvBaseCS static members
 *****************************************************************************/

const uint32 FConvBaseCS::THREADGROUP_SIZE_X(128);
const uint32 FConvBaseCS::MAX_NUMBER_DIMENSIONS(16);



/* FConvBaseCS public functions
 *****************************************************************************/

void FConvBaseCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
	OutEnvironment.SetDefine(TEXT("MAX_NUMBER_DIMENSIONS"), MAX_NUMBER_DIMENSIONS);
}



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FConvBaseCS, "/Plugins/NeuralNetworkInference/Private/ConvBaseOperator.usf", "ConvBaseCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
