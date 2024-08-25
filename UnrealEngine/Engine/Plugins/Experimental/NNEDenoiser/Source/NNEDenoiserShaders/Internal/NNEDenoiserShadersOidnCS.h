// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNEDenoiserShaders::Internal
{

	enum class ENNEDenoiserInputKind : uint8
	{
		Color = 0,
		Albedo,
		Normal,
		Flow,
		Output,
		MAX
	};

	class FNNEDenoiserOidnConstants
	{
	public:
		static constexpr int32 THREAD_GROUP_SIZE{ 32 };
	};
	
	class NNEDENOISERSHADERS_API FNNEDenoiserOidnCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNNEDenoiserOidnCS);
		SHADER_USE_PARAMETER_STRUCT(FNNEDenoiserOidnCS, FGlobalShader)

		class FNNEDenoiserInputKind : SHADER_PERMUTATION_ENUM_CLASS("INPUT_KIND_INDEX", ENNEDenoiserInputKind);
		using FPermutationDomain = TShaderPermutationDomain<FNNEDenoiserInputKind>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER(int32, OutputTextureWidth)
			SHADER_PARAMETER(int32, OutputTextureHeight)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
			SHADER_PARAMETER(float, NormScale)
			SHADER_PARAMETER(float, InvNormScale)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

} // namespace UE::NNEDenoiser::Private