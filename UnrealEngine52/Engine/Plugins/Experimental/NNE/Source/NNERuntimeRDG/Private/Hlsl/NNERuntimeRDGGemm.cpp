// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGGemm.h"
#include "NNEHlslShadersGemmCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorGemm, TEXT("NNE.Operator.Hlsl.Gemm"));

	/**
	 * Gemm operator implementation
	 */
	class TGemm : public FOperatorHlsl
	{
	public:

		TGemm() {}
		virtual ~TGemm() = default;

	private:

		float InputAlpha = 1.0f;
		float InputBeta = 1.0f;
		int32 InputTransA = 0;
		int32 InputTransB = 0;

		bool bIsCScalar = false;
		bool bNoBias = true;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, TArrayView<NNECore::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() >= 2 && InputTensors.Num() <= 3);
			check(OutputTensors.Num() == 1);

			const NNECore::FTensorShape& InputA = InputTensors[0]->GetShape();
			const NNECore::FTensorShape& InputB = InputTensors[1]->GetShape();
			if (InputA.Rank() != 2 || InputB.Rank() != 2)
			{
				return -1;
			}

			uint32 M = InputTransA != 0 ? InputA.GetData()[1] : InputA.GetData()[0];
			uint32 N = InputTransB != 0 ? InputB.GetData()[0] : InputB.GetData()[1];
			TArray<uint32> OutputShapeData;

			OutputShapeData.Emplace(M);
			OutputShapeData.Emplace(N);
			
			NNECore::FTensorShape OutputShape = NNECore::FTensorShape::Make(OutputShapeData);
			
			OutputTensors[0]->SetShape(OutputShape);
			return 0;
		};
		
		virtual bool Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() >= 2 && InputTensorDescs.Num() <= 3);
			check(OutputTensorDescs.Num() == 1);

            const NNECore::FTensorDesc& InputA = InputTensorDescs[0];
			const NNECore::FTensorDesc& InputB = InputTensorDescs[1];

			if (InputA.GetShape().Rank() != 2)
			{
				UE_LOG(LogNNE, Warning, TEXT("Gemm first input should be of rank 2"));
				return false;
			}
			if (InputB.GetShape().Rank() != 2)
			{
				UE_LOG(LogNNE, Warning, TEXT("Gemm second input should be of rank 2"));
				return false;
			}
			if (InputTensorDescs.Num() == 3)
			{
				const NNECore::FTensorDesc& InputC = InputTensorDescs[2];
				if (InputC.GetShape().Rank() > 2)
				{
					UE_LOG(LogNNE, Warning, TEXT("Gemm third input should be of rank 2 or less"));
					return false;
				}
				if (InputC.GetShape().Rank() == 1 && InputC.GetShape().GetData()[0] == 1)
				{
					UE_LOG(LogNNE, Warning, TEXT("Gemm third input as scalar not supported"));
					return false;
				}
			}

			// C is treated as a scalar if there is no valid C, either width or height is zero or C dimension is 1x1
			bIsCScalar = false; // InputTensors.Num() != 3 || InputC.Sizes[0] * InputC.Sizes[1] < 2;
			// CScalar = C != nullptr ? C[0] : (InElementType)0;
			bNoBias = InputTensorDescs.Num() != 3 /*|| InputC.Sizes[0] * InputC.Sizes[1] < 1*/;

			InputAlpha = Attributes.GetValueOrDefault(TEXT("alpha"), InputAlpha);
			InputBeta = Attributes.GetValueOrDefault(TEXT("beta"), InputBeta);
			InputTransA = Attributes.GetValueOrDefault(TEXT("transA"), InputTransA);
			InputTransB = Attributes.GetValueOrDefault(TEXT("transB"), InputTransB);

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			const EGemmAlgorithm Algorithm = EGemmAlgorithm::Simple32x32;

			const float CScalar = 0.0f;

			check(InputTensors.Num() >= 2 && InputTensors.Num() <= 3);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(OutputTensors[0] != nullptr);
			const FTensorRDG& InputA = *InputTensors[0];
			const FTensorRDG& InputB = *InputTensors[1];
			const FTensorRDG& Output = *OutputTensors[0];
			const FTensorRDG* InputC = nullptr;

			if (InputTensors.Num() == 3)
			{
				check(InputTensors[2] != nullptr);
				InputC = InputTensors[2];
			}
			
			// Set parameters
			TGemmCS::FParameters* Parameters = GraphBuilder.AllocParameters<TGemmCS::FParameters>();
			TGemmCS::FillInParameters(InputAlpha, InputBeta, InputTransA, InputTransB, InputA, InputB, InputC, CScalar, *Parameters);
			Parameters->A = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputA.GetBuffer(), PF_R32_FLOAT));
			Parameters->B = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputB.GetBuffer(), PF_R32_FLOAT));
			if (InputC != nullptr) {
				Parameters->C = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputC->GetBuffer(), PF_R32_FLOAT));
			}
			Parameters->Y = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			TGemmCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<TGemmCS::FGemmCScalar>(bNoBias ? EGemmCScalar::NoBias : (bIsCScalar ? EGemmCScalar::Yes : EGemmCScalar::No));
			PermutationVector.Set<TGemmCS::FGemmAlgorithm>(Algorithm);
			PermutationVector.Set<TGemmCS::FGemmNumStackDimensions>(0);
			TShaderMapRef<TGemmCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			FIntVector ThreadGroupCount = TGemmCS::GetGroupCount(*Parameters, Algorithm, 0);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Gemm");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorGemm);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Parameters,
				ThreadGroupCount);
		}
	};

	bool ValidateGemmOperator(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("alpha"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("beta"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("transA"), ENNEAttributeDataType::Int32);
		AttributeValidator.AddOptional(TEXT("transB"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		InputValidator.AddOptional();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateGemmOperator()
	{
		return new TGemm();
	}

	bool RegisterGemmOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Gemm"), CreateGemmOperator, ValidateGemmOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
