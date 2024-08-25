// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGGather.h"
#include "NNERuntimeRDGHelperGather.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNEHlslShadersGatherCS.h"
#include "NNEAttributeMap.h"
#include "NNETypes.h"
#include "NNETensor.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorGather, TEXT("NNE.Operator.Hlsl.Gather"));

	/**
	 * Gather operator implementation
	 */
	template <typename DataElementType, typename IndicesElementType>
	class FGather : public FOperatorHlsl
	{
	public:

		FGather() = default;
		virtual ~FGather() = default;

	private:

		int32 Axis = 0;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
		{
			check(InputTensors.Num() == 2)
			check(OutputTensors.Num() == 1)

			const NNE::Internal::FTensor& DataTensor = *InputTensors[0];
			const NNE::Internal::FTensor& IndicesTensor = *InputTensors[1];
			const NNE::FTensorShape& DataShape = DataTensor.GetShape();
			const NNE::FTensorShape& IndicesShape = IndicesTensor.GetShape();

			const int32 OutputRank = IndicesShape.Rank() + DataShape.Rank() - 1;
			
			TArray<uint32> OutputShapeData;
			OutputShapeData.Reserve(OutputRank);

			int32 DataRankIdx = 0;
			while (DataRankIdx < Axis)
			{
				OutputShapeData.Add(DataShape.GetData()[DataRankIdx++]);
			}

			OutputShapeData.Append(IndicesShape.GetData());
			DataRankIdx++;

			while (DataRankIdx < DataShape.Rank())
			{
				OutputShapeData.Add(DataShape.GetData()[DataRankIdx++]);
			}

			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);
			check(OutputShape.Rank() == OutputRank);

			OutputTensors[0]->SetShape(OutputShape);

			Internal::CPUHelper::Gather::Apply(DataTensor, IndicesTensor, Axis, *OutputTensors[0]);

			return 0;
		};
		
		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 2)
			check(OutputTensorDescs.Num() == 1)

			const int32 MaxNumDimensions = NNEHlslShaders::Internal::FGatherConstants::MAX_NUM_DIMENSIONS;

			const NNE::FTensorDesc& Data = InputTensorDescs[0];
			const NNE::FTensorDesc& Indices = InputTensorDescs[1];
			const NNE::FTensorDesc& Output = OutputTensorDescs[0];

			if (Output.GetShape().Rank() > MaxNumDimensions)
			{
				UE_LOG(LogNNE, Warning, TEXT("Gather first input should be of rank %d or less but is %d"), MaxNumDimensions, Output.GetShape().Rank());
				return false;
			}

			if ((Data.GetShape().Rank() + Indices.GetShape().Rank() - 1) > MaxNumDimensions)
			{
				UE_LOG(LogNNE, Warning, TEXT("Gather sum of input 0 and 1 ranks -1 should be less than %d"), MaxNumDimensions);
				return false;
			}

			Axis = Attributes.GetValueOrDefault(TEXT("axis"), Axis);
			if (Axis >= Data.GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Gather Axis attribute should be inferior to first input rank"));
				return false;
			}
			if (Axis < -Data.GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Gather Axis attribute should be superior or equal to minus the first input rank"));
				return false;
			}
			Axis = Axis >= 0 ? Axis : Data.GetShape().Rank() + Axis;

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 2)
			check(OutputTensors.Num() == 1)
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(OutputTensors[0] != nullptr);
			check(OutputTensors[0]->GetShape().Rank() <= FGatherConstants::MAX_NUM_DIMENSIONS)
			check(InputTensors[0]->GetShape().Rank() > 0)
			check(InputTensors[1]->GetShape().Rank() > 0)
			check(InputTensors[0]->GetShape().Rank() + (InputTensors[1]->GetShape().Rank() - 1) <= FGatherConstants::MAX_NUM_DIMENSIONS)

			const FTensorRDG& Data = *InputTensors[0];
			const FTensorRDG& Indices = *InputTensors[1];
			const FTensorRDG& Output = *OutputTensors[0];

			// Set parameters
			TGatherCS::FParameters* Parameters = GraphBuilder.AllocParameters<TGatherCS::FParameters>();
			TGatherCS::FillInParameters(Axis, Data, Indices, *Parameters);
			Parameters->Data = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Data.GetBuffer(), PF_R32_FLOAT));
			Parameters->Indices = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Indices.GetBuffer(), PF_R32_FLOAT));
			Parameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			TGatherCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<TGatherCS::FGatherNumOutputDimensions>(Output.GetShape().Rank());
			TShaderMapRef<TGatherCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			FIntVector ThreadGroupCount = TGatherCS::GetGroupCount(*Parameters);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Gather");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorGather);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Gather.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Parameters,
				ThreadGroupCount);
		}
	};

	bool ValidateGatherOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axis"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.SetTemplateCount(2);
	
		InputValidator.AddSupportedType(ENNETensorDataType::Float, 0);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32, 0);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64, 0);
		InputValidator.AddRequired(0);

		InputValidator.AddSupportedType(ENNETensorDataType::Int32, 1);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64, 1);
		InputValidator.AddRequired(1);
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateGatherOperator()
	{
		return new FGather<float, int32>();
	}

	bool RegisterGatherOperator(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
		Registry.OpAdd({{TEXT("Gather"), TEXT("Onnx")}, 1}, CreateGatherOperator, ValidateGatherOperator);
		Registry.OpAdd({{TEXT("Gather"), TEXT("Onnx")}, 11}, CreateGatherOperator, ValidateGatherOperator);
		Registry.OpAdd({{TEXT("Gather"), TEXT("Onnx")}, 13}, CreateGatherOperator, ValidateGatherOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl