// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FConvTransposeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvTransposeCS);
	SHADER_USE_PARAMETER_STRUCT(FConvTransposeCS, FGlobalShader)

	class FConvMode : SHADER_PERMUTATION_ENUM_CLASS("CONV_MODE", EConvMode);
	using FPermutationDomain = TShaderPermutationDomain<FConvMode>;

	static const uint32 THREADGROUP_SIZE_X;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// All variables are used in all (1D, 2D, 3D, nD) convolutions
		// Input variables
		SHADER_PARAMETER(uint32, XVolume)
		SHADER_PARAMETER(int32, NumberConvolutionalDimensions) // Optional - Only for nD convolution
		// Input SRV variables
		SHADER_PARAMETER_SRV(Buffer<uint>, Zeros)
		SHADER_PARAMETER_SRV(Buffer<uint>, XSizes)
		SHADER_PARAMETER_SRV(Buffer<uint>, XWithZerosSizes)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, XSRV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, XWithZerosUAV)
	END_SHADER_PARAMETER_STRUCT()
};
