// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FElementWiseCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FElementWiseCS);
	SHADER_USE_PARAMETER_STRUCT(FElementWiseCS, FGlobalShader)

	static const uint32 THREADGROUP_SIZE_X;

	class FShaderType : SHADER_PERMUTATION_ENUM_CLASS("SHADER_FUNCTION", EElementWiseOperator);
	class FIsInlined : SHADER_PERMUTATION_BOOL("IS_INLINED");
	using FPermutationDomain = TShaderPermutationDomain<FShaderType, FIsInlined>;

	/** Set desired multidirectional operator. */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input variables
		SHADER_PARAMETER(uint32, TensorSize)
		SHADER_PARAMETER(float, Attribute)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
		// Optional SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InputSRV)
	END_SHADER_PARAMETER_STRUCT()

private:
	static FString GetFunctionString(const EElementWiseOperator InType);
	static uint32 GetNumberAttributes(const EElementWiseOperator InType);
};
