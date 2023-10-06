// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEHlslShaderBase.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNEHlslShaders::Internal
{
	enum class ENeuralPostProcessingOverwrite : uint8
	{
		No = 0,
		Yes,
		MAX
	};

	enum class ENeuralPostProcessingInterpolate : uint8
	{
		No = 0,
		Yes,
		MAX
	};

	class FNeuralPostProcessingConstants
	{
	public:
		static const int32 THREAD_GROUP_SIZE{ 32 };
	};

	class NNEHLSLSHADERS_API TNeuralPostProcessingReadInputCS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TNeuralPostProcessingReadInputCS);
		SHADER_USE_PARAMETER_STRUCT(TNeuralPostProcessingReadInputCS, FHlslShaderBase)

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, AccumulationBuffer)
			SHADER_PARAMETER(float, Weight)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEHLSLSHADERS_API TNeuralPostProcessingPreStepCS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TNeuralPostProcessingPreStepCS);
		SHADER_USE_PARAMETER_STRUCT(TNeuralPostProcessingPreStepCS, FHlslShaderBase)

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			SHADER_PARAMETER(int32, InputBufferWidth)
			SHADER_PARAMETER(int32, InputBufferHeight)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, InputBuffer)
			SHADER_PARAMETER(float, RangeScale)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEHLSLSHADERS_API TNeuralPostProcessingPostStepCS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TNeuralPostProcessingPostStepCS);
		SHADER_USE_PARAMETER_STRUCT(TNeuralPostProcessingPostStepCS, FHlslShaderBase)

		class FNeuralPostProcessingOverwrite : SHADER_PERMUTATION_ENUM_CLASS("OVERWRITE", ENeuralPostProcessingOverwrite);
		class FNeuralPostProcessingInterpolate : SHADER_PERMUTATION_ENUM_CLASS("INTERPOLATE", ENeuralPostProcessingInterpolate);
		using FPermutationDomain = TShaderPermutationDomain<FNeuralPostProcessingOverwrite, FNeuralPostProcessingInterpolate>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, OutputBufferWidth)
			SHADER_PARAMETER(int32, OutputBufferHeight)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputBuffer)
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, AccumulationBuffer)
			SHADER_PARAMETER(float, Weight)
			SHADER_PARAMETER(float, RangeScale)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEHLSLSHADERS_API TNeuralPostProcessingWriteOutputPS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TNeuralPostProcessingWriteOutputPS);
		SHADER_USE_PARAMETER_STRUCT(TNeuralPostProcessingWriteOutputPS, FHlslShaderBase)

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, AccumulationBuffer)
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};
} // UE::NNEHlslShaders::Internal