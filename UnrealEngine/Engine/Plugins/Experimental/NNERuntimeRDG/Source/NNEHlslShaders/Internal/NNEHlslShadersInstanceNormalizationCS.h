// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEHlslShaderBase.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNEHlslShaders::Internal
{
	enum class EInstanceNormalizationAlgorithm : uint8
	{
		Simple1x265 = 0,
		SharedMemory256x1,
		SharedMemory512x1,
		SharedMemory768x1,
		SharedMemory1024x1,
		MAX
	};

	class NNEHLSLSHADERS_API TInstanceNormalizationCS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TInstanceNormalizationCS);
		SHADER_USE_PARAMETER_STRUCT(TInstanceNormalizationCS, FHlslShaderBase)

		class FInstanceNormalizationAlgorithm : SHADER_PERMUTATION_ENUM_CLASS("ALGORITHM", EInstanceNormalizationAlgorithm);
		using FPermutationDomain = TShaderPermutationDomain<FInstanceNormalizationAlgorithm>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(float, Epsilon)
			SHADER_PARAMETER(int32, C)
			SHADER_PARAMETER(int32, NxC)
			SHADER_PARAMETER(int32, W)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Input)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Scale)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Bias)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)
		END_SHADER_PARAMETER_STRUCT()

		static void FillInParameters(float Epsilon, const NNE::Internal::FTensor& Input, FParameters& Parameters);

		static FIntVector GetGroupCount(const FParameters& Parameters, EInstanceNormalizationAlgorithm Algorithm);
		static EInstanceNormalizationAlgorithm GetAlgorithm(const FParameters& Parameters);

		static void LexFromString(EInstanceNormalizationAlgorithm& OutValue, const TCHAR* StringVal);
	};
} // UE::NNEHlslShaders::Internal