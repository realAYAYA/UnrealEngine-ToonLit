// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FMultidirectionalBroadcastCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMultidirectionalBroadcastCS);
	SHADER_USE_PARAMETER_STRUCT(FMultidirectionalBroadcastCS, FGlobalShader)

	static const uint32 THREADGROUP_SIZE_X;
	static const uint32 MAX_NUMBER_DIMENSIONS;

	class FShaderType : SHADER_PERMUTATION_ENUM_CLASS("SHADER_FUNCTION", EMultidirectionalBroadcastOperator);
	class FInlinedMode : SHADER_PERMUTATION_ENUM_CLASS("INLINED_MODE", EMultidirectionalBroadcastInlinedMode);
	class FShapeMode : SHADER_PERMUTATION_ENUM_CLASS("SHAPE_MODE", EMultidirectionalBroadcastShapeMode);
	using FPermutationDomain = TShaderPermutationDomain<FShaderType, FInlinedMode, FShapeMode>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input variables
		SHADER_PARAMETER(uint32, TensorSize)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
		// Optional elementwise SRV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, ASRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, BSRV)
		// Optional multidirectional broadcast variables
		SHADER_PARAMETER(uint32, ShapeDimensions)
		SHADER_PARAMETER_SRV(Buffer<float>, ShapeOutput)
		SHADER_PARAMETER_SRV(Buffer<float>, ShapeA)
		SHADER_PARAMETER_SRV(Buffer<float>, ShapeB)
	END_SHADER_PARAMETER_STRUCT()

private:
	static FString GetFunctionString(const EMultidirectionalBroadcastOperator InType);
};
