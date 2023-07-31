// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FGemmCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGemmCS);
	SHADER_USE_PARAMETER_STRUCT(FGemmCS, FGlobalShader)

	class FGemmMode : SHADER_PERMUTATION_ENUM_CLASS("GEMM_MODE", EGemmMode);
	using FPermutationDomain = TShaderPermutationDomain<FGemmMode>;

	static const uint32 THREADGROUP_SIZE_X;
	static const uint32 THREADGROUP_SIZE_Y;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input variables
		SHADER_PARAMETER(float, Alpha)
		SHADER_PARAMETER(float, Beta)
		SHADER_PARAMETER(uint32, OutputRows)
		SHADER_PARAMETER(uint32, OutputColumns)
		SHADER_PARAMETER(uint32, AColsOrBRows)
		SHADER_PARAMETER(uint32, AStrideX)
		SHADER_PARAMETER(uint32, AStrideY)
		SHADER_PARAMETER(uint32, BStrideX)
		SHADER_PARAMETER(uint32, BStrideY)
		SHADER_PARAMETER(uint32, CSizeX)
		SHADER_PARAMETER(uint32, CSizeY)
		SHADER_PARAMETER(uint32, OutputStride)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, ASRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, BSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, CSRV)
		SHADER_PARAMETER(float, BetaTimesCScalar)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
	END_SHADER_PARAMETER_STRUCT()
};
