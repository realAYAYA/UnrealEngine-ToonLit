// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultidirectionalBroadcastCS.h"
#include "Utils.h"



/* FMultidirectionalBroadcastCS static members
 *****************************************************************************/

const uint32 FMultidirectionalBroadcastCS::THREADGROUP_SIZE_X(128);
const uint32 FMultidirectionalBroadcastCS::MAX_NUMBER_DIMENSIONS(16);



/* FMultidirectionalBroadcastCS public functions
 *****************************************************************************/

void FMultidirectionalBroadcastCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
	OutEnvironment.SetDefine(TEXT("MAX_NUMBER_DIMENSIONS"), MAX_NUMBER_DIMENSIONS);

	FPermutationDomain PermutationVector(InParameters.PermutationId);
	const FString FunctionString = GetFunctionString(PermutationVector.Get<FShaderType>());
	OutEnvironment.SetDefine(TEXT("MultidirectionalBroadcastFunction(X, Y)"), *FunctionString);

	// Note: INLINED_MODE and SHAPE_MODE are already defined because we used SHADER_PERMUTATION_ENUM_CLASS with those names
}



/* FMultidirectionalBroadcastCS private static functions
 *****************************************************************************/

FString FMultidirectionalBroadcastCS::GetFunctionString(const EMultidirectionalBroadcastOperator InType)
{
	if (InType >= EMultidirectionalBroadcastOperator::MAX)
	{
		UE_LOG(LogNeuralNetworkInferenceShaders, Warning, TEXT("FMultidirectionalBroadcastCS::GetFunctionString(): Unexpected InType = %d."), (int32)InType);
		return TEXT("FMultidirectionalBroadcastCS::GetFunctionString(): Unexpected InType.");
	}
	const TArray<FString> FunctionStrings({
		TEXT("(X + Y)"),	// Add
		TEXT("(X / Y)"),	// Div
		TEXT("(X * Y)"),	// Mul
		TEXT("pow(X, Y)"),	// Pow
		TEXT("(X - Y)")});	// Sub
	return FunctionStrings[(int32)InType];
}



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FMultidirectionalBroadcastCS, "/Plugins/NeuralNetworkInference/Private/MultidirectionalBroadcastOperator.usf", "MultidirectionalBroadcastCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
