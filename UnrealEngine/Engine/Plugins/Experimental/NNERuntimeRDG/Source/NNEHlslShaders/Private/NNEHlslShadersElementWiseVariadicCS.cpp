// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersElementWiseVariadicCS.h"
#include "NNE.h"

namespace UE::NNEHlslShaders::Internal
{
	void TElementWiseVariadicCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), FElementWiseVariadicConstants::NUM_GROUP_THREADS);

		FPermutationDomain PermutationVector(InParameters.PermutationId);

		const FString OpFunc = GetOpFunc(PermutationVector.Get<FOperatorType>());

		OutEnvironment.SetDefine(TEXT("ELEMENTWISE_OP(X,Y)"), *OpFunc);
	}

	const FString TElementWiseVariadicCS::GetOpFunc(NNE::Internal::EElementWiseVariadicOperatorType OpType)
	{
		FString OpTable[(int32) NNE::Internal::EElementWiseVariadicOperatorType::MAX];

		for (int32 Idx = 0; Idx < (int32) NNE::Internal::EElementWiseVariadicOperatorType::MAX; ++Idx)
		{
			OpTable[Idx] = FString("");
		}

#define OP(OpName, OpFunc) OpTable[(int32) NNE::Internal::EElementWiseVariadicOperatorType::OpName] = OpFunc
		OP(Max, TEXT("max(X,Y)"));
		OP(Min, TEXT("min(X,Y)"));
		OP(Mean, TEXT("((X)+(Y))"));
		OP(Sum, TEXT("((X)+(Y))"));
#undef OP

		FString OpFunc = OpTable[(int32)OpType];

		if (OpFunc == "")
		{
			UE_LOG(LogNNE, Warning, TEXT("Undefined ElementWise Variadic operator name for operator:%d"), int(OpType));
		}

		return OpFunc;
	}

	IMPLEMENT_GLOBAL_SHADER(TElementWiseVariadicCS, "/NNE/NNEHlslShadersElementWiseVariadic.usf", "ElementWiseVariadic", SF_Compute);
} // UE::NNEHlslShaders::Internal
