// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGInstanceNormalization.h"
#include "NNEHlslShadersInstanceNormalizationCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"

namespace
{

int ValidateInput(const UE::NNE::FTensorShape& Input, const UE::NNE::FTensorShape& Scale, const UE::NNE::FTensorShape& Bias)
{
	if (Input.Rank() < 3)
	{
		UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization input data should be at least of rank 4: %d"), Input.Rank());
		return -1;
	}

	if (Scale.Rank() != 1)
	{
		UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization input scale should be of rank 1: %d"), Scale.Rank());
		return -1;
	}
	if (Scale.GetData()[0] != Input.GetData()[1])
	{
		UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization input scale size should be equal to channel count: %d vs %d"), Scale.GetData()[0], Input.GetData()[1]);
		return -1;
	}

	if (Bias.GetData()[0] != Input.GetData()[1])
	{
		UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization intput B size should be equal to channel count: : %d vs %d"), Bias.GetData()[0], Input.GetData()[1]);
		return -1;
	}
	if (Bias.Rank() != 1)
	{
		UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization input B should be of rank 1: %d"), Bias.Rank());
		return -1;
	}

	return 0;
}

}

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorInstanceNormalization, TEXT("NNE.Operator.Hlsl.InstanceNormalization"));

	using EInstanceNormalizationAlgorithm = UE::NNEHlslShaders::Internal::EInstanceNormalizationAlgorithm;

	/**
	 * InstanceNormalization operator implementation
	 */
	class TInstanceNormalization : public FOperatorHlsl
	{
	public:

		TInstanceNormalization() {}
		virtual ~TInstanceNormalization() = default;

	private:

		float Epsilon = 1e-5;

		EInstanceNormalizationAlgorithm Algorithm = EInstanceNormalizationAlgorithm::MAX;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 3);
			check(OutputTensors.Num() == 1);

			if (const int Res = ValidateInput(InputTensors[0]->GetShape(), InputTensors[1]->GetShape(), InputTensors[2]->GetShape()); Res < 0)
			{
				return Res;
			}

			OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensorDescs.Num() == 3);
			check(OutputTensorDescs.Num() == 1);

			Epsilon = Attributes.GetValue<float>(TEXT("epsilon"));

			// For testing only
			TInstanceNormalizationCS::LexFromString(Algorithm, *Attributes.GetValueOrDefault<FString>(TEXT("__UE__algorithm"), "MAX"));

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 3);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(InputTensors[2] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Scale = *InputTensors[1];
			const FTensorRDG& Bias = *InputTensors[2];
			const FTensorRDG& Output = *OutputTensors[0];

			// Set parameters
			TInstanceNormalizationCS::FParameters* Parameters = GraphBuilder.AllocParameters<TInstanceNormalizationCS::FParameters>();
			TInstanceNormalizationCS::FillInParameters(Epsilon, Input, *Parameters);
			Parameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			Parameters->Scale = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Scale.GetBuffer(), PF_R32_FLOAT));
			Parameters->Bias = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Bias.GetBuffer(), PF_R32_FLOAT));
			Parameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			EInstanceNormalizationAlgorithm DispatchAlgorithm = Algorithm;
			if (DispatchAlgorithm == EInstanceNormalizationAlgorithm::MAX)
			{
				DispatchAlgorithm = TInstanceNormalizationCS::GetAlgorithm(*Parameters);
			}
			
			TInstanceNormalizationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<TInstanceNormalizationCS::FInstanceNormalizationAlgorithm>(DispatchAlgorithm);
			TShaderMapRef<TInstanceNormalizationCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
			FIntVector ThreadGroupCount = TInstanceNormalizationCS::GetGroupCount(*Parameters, DispatchAlgorithm);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.InstanceNormalization");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorInstanceNormalization);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.InstanceNormalization.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Parameters,
				ThreadGroupCount);
		}
	};

	bool ValidateInstanceNormalizationOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddRequired(TEXT("epsilon"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("__UE__algorithm"), ENNEAttributeDataType::String);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateInstanceNormalizationOperator()
	{
		return new TInstanceNormalization();
	}

	bool RegisterInstanceNormalizationOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("InstanceNormalization"), CreateInstanceNormalizationOperator, ValidateInstanceNormalizationOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
