// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElementWiseCS.h"
#include "Utils.h"



/* FElementWiseCS static members
 *****************************************************************************/

const uint32 FElementWiseCS::THREADGROUP_SIZE_X(128);



/* FElementWiseCS public functions
 *****************************************************************************/

void FElementWiseCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), THREADGROUP_SIZE_X);
	// NumberAttributes
	FPermutationDomain PermutationVector(InParameters.PermutationId);
	const uint32 NumberAttributes = GetNumberAttributes(PermutationVector.Get<FShaderType>());
	OutEnvironment.SetDefine(TEXT("NUMBER_ATTRIBUTES"), NumberAttributes);
	// ElementWiseFunction
	const FString ElementWiseFunction = GetFunctionString(PermutationVector.Get<FShaderType>());
	if (NumberAttributes == 0)
	{
		OutEnvironment.SetDefine(TEXT("ElementWiseFunction(X)"), *ElementWiseFunction);
	}
	else
	{
		OutEnvironment.SetDefine(TEXT("ElementWiseFunction(X, Attribute)"), *ElementWiseFunction);
	}
	// Note: IS_INLINED is already defined because we used SHADER_PERMUTATION_BOOL("IS_INLINED")
}



/* FElementWiseCS private static functions
 *****************************************************************************/

FString FElementWiseCS::GetFunctionString(const EElementWiseOperator InType)
{
	if (InType >= EElementWiseOperator::MAX)
	{
		UE_LOG(LogNeuralNetworkInferenceShaders, Warning, TEXT("FElementWiseCS::GetFunctionString(): Unexpected InType = %d."), (int32)InType);
		return TEXT("FElementWiseCS::GetFunctionString(): Unexpected InType.");
	}
	const TArray<FString> FunctionStrings({
		TEXT("abs(X)"),						// Abs
		TEXT("acos(X)"),					// Acos
		TEXT("asin(X)"),					// Asin
		TEXT("atan(X)"),					// Atan
		TEXT("ceil(X)"),					// Ceil
		TEXT("cos(X)"),						// Cos
		TEXT("cosh(X)"),					// Cosh
		TEXT("exp(X)"),						// Exp
		TEXT("floor(X)"),					// Floor
		TEXT("(X < 0 ? Attribute * X : X)"),// LeakyRelu
		TEXT("log(X)"),						// Log
		TEXT("-(X)"),						// Neg
		TEXT("1/(X)"),						// Reciprocal
		TEXT("round(X)"),					// Round
		TEXT("(X < 0 ? 0 : X)"),			// Relu
		TEXT("1/(1+exp(-(X)))"),			// Sigmoid
		TEXT("sign(X)"),					// Sign
		TEXT("sin(X)"),						// Sin
		TEXT("sinh(X)"),					// Sinh
		TEXT("sqrt(X)"),					// Sqrt
		TEXT("tan(X)"),						// Tan
		TEXT("tanh(X)") });					// Tanh
	return FunctionStrings[(int32)InType];
}

uint32 FElementWiseCS::GetNumberAttributes(const EElementWiseOperator InType)
{
	if (InType >= EElementWiseOperator::MAX)
	{
		UE_LOG(LogNeuralNetworkInferenceShaders, Warning, TEXT("FElementWiseCS::GetNumberAttributes(): Unexpected InType = %d."), (int32)InType);
		return 0;
	}
	const TArray<uint32> NumberAttributes({
		0,	// Abs
		0,	// Acos
		0,	// Asin
		0,	// Atan
		0,	// Ceil
		0,	// Cos
		0,	// Cosh
		0,	// Exp
		0,	// Floor
		1,	// LeakyRelu
		0,	// Log
		0,	// Neg
		0,	// Reciprocal
		0,	// Round
		0,	// Relu
		0,	// Sigmoid
		0,	// Sign
		0,	// Sin
		0,	// Sinh
		0,	// Sqrt
		0,	// Tan
		0 });// Tanh
	return NumberAttributes[(int32)InType];
}



/* Shader implementation
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FElementWiseCS, "/Plugins/NeuralNetworkInference/Private/ElementWiseOperator.usf", "ElementWiseCS", SF_Compute); // Path defined in NeuralNetworkInferenceShadersModule.cpp
