// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGUpsample.h"
#include "NNEHlslShadersUpsampleCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETypes.h"
#include "NNETensor.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorUpsample, TEXT("NNE.Operator.Hlsl.Upsample"));

	/**
	 * Upsample operator implementation
	 */
	class FUpsample : public FOperatorHlsl
	{
	public:

		FUpsample() {}
		virtual ~FUpsample() = default;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);

			const NNE::Internal::FTensor& X = *InputTensors[0];
			const NNE::Internal::FTensor& Scales = *InputTensors[1];
			
			if (!Scales.HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Upsample input 'Scale' (name: %s) should be constant for shape inference to succeed, however it is not constant."), *Scales.GetName());
				return -1;
			}

			TConstArrayView<float> ScalesData = Scales.GetPreparedData<float>();

			if (ScalesData.Num() != X.GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Upsample input 'Scale' (name: %s) have %d elements. While it should be the same as the rank of input 'X' (name : %s) witch is %d"), *Scales.GetName(), ScalesData.Num(), *X.GetName(), X.GetShape().Rank());
				return -1;
			}

			TArray<uint32> OutputShapeData;
			for (int32 i = 0; i < X.GetShape().Rank(); ++i)
			{
				
				OutputShapeData.Emplace(FMath::FloorToInt32(X.GetShape().GetData()[i] * ScalesData[i]));
			}

			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);
			OutputTensors[0]->SetShape(OutputShape);

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 2);
			check(OutputTensorDescs.Num() == 1);

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;
			
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(OutputTensors[0] != nullptr);
			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Scales = *InputTensors[1];
			const FTensorRDG& Output = *OutputTensors[0];

			check(Scales.HasPreparedData());
			TConstArrayView<float> ScalesData = Scales.GetPreparedData<float>();

			FRDGBufferSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(Output.GetVolume(), FUpsampleConstants::NUM_GROUP_THREADS);

			// Set parameters
			FUpsampleCS::FParameters* Params = GraphBuilder.AllocParameters<FUpsampleCS::FParameters>();
			Params->Input = InputSRV;
			Params->Output = OutputUAV;
			FillTensorStrideShaderParameters(Input, Params->TensorInfo, 0);
			FillTensorStrideShaderParameters(Output, Params->TensorInfo, 1);
			FillTensorSizeShaderParameters(Input, Params->TensorInfo, 2);
			FillTensorSizeShaderParameters(Output, Params->TensorInfo, 3);
			Params->Num = Output.GetVolume();
			Params->ThreadCountX = ThreadGroupCount.X * FUpsampleConstants::NUM_GROUP_THREADS;

			FUpsampleCS::FPermutationDomain PermutationVector;

			PermutationVector.Set<FUpsampleCS::FUpsampleNumDimensions>(Output.GetShape().Rank());

			TShaderMapRef<FUpsampleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Upsample");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorUpsample);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Upsample.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	bool ValidateUpsampleOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("mode"), ENNEAttributeDataType::String);
		bIsValid &= AttributeValidator.Validate(AttributeMap);
		
		FString Mode = AttributeMap.GetValueOrDefault<FString>(TEXT("mode"), TEXT("nearest"));
		if (!Mode.Equals(TEXT("nearest")))
		{
			UE_LOG(LogNNE, Warning, TEXT("Upsample HLSL operator only supports nearest mode for now"));
			return false;
		}

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateUpsampleOperator()
	{
		return new FUpsample();
	}

	bool RegisterUpsampleOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Upsample"), CreateUpsampleOperator, ValidateUpsampleOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
