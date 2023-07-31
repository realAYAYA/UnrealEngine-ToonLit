// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperatorEnumClasses.h"
// GPU/RHI/shaders
#include "GlobalShader.h"
#include "RHI.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"



class NEURALNETWORKINFERENCESHADERS_API FConvBaseCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvBaseCS);
	SHADER_USE_PARAMETER_STRUCT(FConvBaseCS, FGlobalShader)

	class FHasBias : SHADER_PERMUTATION_BOOL("HAS_BIAS");
	class FConvMode : SHADER_PERMUTATION_ENUM_CLASS("CONV_MODE", EConvMode);
	using FPermutationDomain = TShaderPermutationDomain<FHasBias, FConvMode>;

	static const uint32 THREADGROUP_SIZE_X;
	static const uint32 MAX_NUMBER_DIMENSIONS;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// All variables are used in all (1D, 2D, 3D, nD) convolutions
		// Input variables
		SHADER_PARAMETER(uint32, bIsTransposed)
		SHADER_PARAMETER(uint32, Group)
		SHADER_PARAMETER(uint32, MIntoGroup)
		SHADER_PARAMETER(uint32, CIntoGroup)
		SHADER_PARAMETER(uint32, OutputVolume)
		SHADER_PARAMETER(uint32, OutputBatchVolume)
		SHADER_PARAMETER(uint32, OutputImageArea)
		SHADER_PARAMETER(uint32, WBatchVolume)
		SHADER_PARAMETER(uint32, WImageArea)
		SHADER_PARAMETER(uint32, XOrXWithZerosBatchVolume)
		SHADER_PARAMETER(uint32, XOrXWithZerosImageArea)
		SHADER_PARAMETER(int32, NumberConvolutionalDimensions) // Optional - Only for nD convolution
		// Input SRV variables
		SHADER_PARAMETER_SRV(Buffer<uint>, OutputSizes)
		SHADER_PARAMETER_SRV(Buffer<uint>, XOrXWithZerosSizes)
		SHADER_PARAMETER_SRV(Buffer<uint>, WSizes)
		SHADER_PARAMETER_SRV(Buffer<uint>, Dilations)
		SHADER_PARAMETER_SRV(Buffer<uint>, Strides)
		SHADER_PARAMETER_SRV(Buffer<uint>, Pads)
		// SRV/UAV variables
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, XOrXWithZerosSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, WSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, BSRV) // Optional - Only if bias
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputUAV)
	END_SHADER_PARAMETER_STRUCT()
};
