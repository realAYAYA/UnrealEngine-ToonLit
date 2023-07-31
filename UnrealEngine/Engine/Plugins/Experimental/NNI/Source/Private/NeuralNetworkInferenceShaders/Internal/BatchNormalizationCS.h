// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FBatchNormalizationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBatchNormalizationCS);
	SHADER_USE_PARAMETER_STRUCT(FBatchNormalizationCS, FGlobalShader)

	class FBatchNormalizationMode : SHADER_PERMUTATION_ENUM_CLASS("BATCH_NORMALIZATION_MODE", EBatchNormalizationMode);
	class FIsInlined : SHADER_PERMUTATION_BOOL("IS_INLINED");
	using FPermutationDomain = TShaderPermutationDomain<FBatchNormalizationMode, FIsInlined>;

	static const uint32 THREADGROUP_SIZE_X;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input variables
		SHADER_PARAMETER(uint32, BatchSize)
		SHADER_PARAMETER(uint32, ChannelSize)
		SHADER_PARAMETER(uint32, ChannelsVolume)
		SHADER_PARAMETER(uint32, ImageArea)
		SHADER_PARAMETER(float, Epsilon)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, XSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Scale)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Bias)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Mean)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Variance)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
	END_SHADER_PARAMETER_STRUCT()
};
