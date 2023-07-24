// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "NNECoreOperator.h"

namespace UE::NNEHlslShaders::Internal
{
	class FElementWiseUnaryConstants
	{
	public:

		static const int32 NUM_GROUP_THREADS{ 256 };
	};

	class NNEHLSLSHADERS_API TElementWiseUnaryCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(TElementWiseUnaryCS);
		SHADER_USE_PARAMETER_STRUCT(TElementWiseUnaryCS, FGlobalShader)

		class FOperatorType : SHADER_PERMUTATION_ENUM_CLASS("OP_TYPENAME", NNECore::Internal::EElementWiseUnaryOperatorType);
		using FPermutationDomain = TShaderPermutationDomain<FOperatorType>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Input)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)
			SHADER_PARAMETER(uint32, Num)
			SHADER_PARAMETER(uint32, ThreadCountX)
			SHADER_PARAMETER(float, Alpha)
			SHADER_PARAMETER(float, Beta)
			SHADER_PARAMETER(float, Gamma)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	private:

		static const FString GetOpFunc(NNECore::Internal::EElementWiseUnaryOperatorType OpType);
	};
} // UE::NNEHlslShaders::Internal
