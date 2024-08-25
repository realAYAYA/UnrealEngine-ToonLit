// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersReduceCS.h"
#include "NNE.h"
#include "RenderGraphBuilder.h"

namespace UE::NNEHlslShaders::Internal
{
	void TReduceCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), FReduceConstants::NUM_GROUP_THREADS);
	}

	void TReduceCS::FillInParameters(TConstArrayView<uint32> Shape, int32 Axis, TReduceCS::FParameters* Parameters)
	{
		check(Axis >= 0 && Axis <= Shape.Num());
		check(Parameters);

		int32 NumElemBeforeAxis = 1;
		int32 NumElemAfterAxis = 1;
		for (int32 d = 0; d < Axis; ++d)
		{
			NumElemBeforeAxis *= Shape[d];
		}
		for (int32 d = Axis + 1; d < Shape.Num(); ++d)
		{
			NumElemAfterAxis *= Shape[d];
		}

		Parameters->NumElemBeforeAxis = NumElemBeforeAxis;
		Parameters->AxisSize = Shape[Axis];
		Parameters->NumElemAfterAxis = NumElemAfterAxis;
		Parameters->Epsilon = 0;
	}

	void TReduceCS::EnqueueRDG(FRDGBuilder& GraphBuilder, TReduceCS::FParameters* Parameters, FRDGBufferRef Input, FRDGBufferRef Output, EReduceOperatorType OperatorType, FRDGBufferRef Output2)
	{
		check(Parameters);

		Parameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input, PF_R32_FLOAT));
		Parameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output, PF_R32_FLOAT));
		if (Output2 != nullptr) {
			Parameters->Output2 = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output2, PF_R32_FLOAT));
		}

		TReduceCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<TReduceCS::FReduceType>(OperatorType);

		TShaderMapRef<TReduceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		FIntVector ThreadGroupCount{ 1, (int32)Parameters->NumElemAfterAxis, (int32)Parameters->NumElemBeforeAxis };

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NNE.Operator.Hlsl.Reduce.OneAxis.Dispatch"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			ThreadGroupCount);
	}

	IMPLEMENT_GLOBAL_SHADER(TReduceCS, "/NNE/NNEHlslShadersReduce.usf", "Reduce", SF_Compute);
} // UE::NNEHlslShaders::Internal
