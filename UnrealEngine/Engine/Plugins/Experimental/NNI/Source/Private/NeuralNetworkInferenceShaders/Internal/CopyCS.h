// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FCopyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyCS);
	SHADER_USE_PARAMETER_STRUCT(FCopyCS, FGlobalShader)

	static const uint32 THREADGROUP_SIZE_X;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input variables
		SHADER_PARAMETER(uint32, Num)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InputSRV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
	END_SHADER_PARAMETER_STRUCT()
};
