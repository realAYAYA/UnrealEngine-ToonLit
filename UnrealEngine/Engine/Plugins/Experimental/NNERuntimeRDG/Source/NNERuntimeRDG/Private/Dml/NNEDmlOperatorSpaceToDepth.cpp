// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlSpaceToDepth : public FOperatorDml
{
	enum InputDims
	{
		N, C, H, W,
		DIM_COUNT
	};

	int32 BlockSize;

	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 4, MaxTensorRank = 4;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSpaceToDepth();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("SpaceToDepth");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Double, ENNETensorDataType::Float, ENNETensorDataType::Half, 
				ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
				ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, 
				ENNETensorDataType::UInt16, ENNETensorDataType::UInt8
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == NumAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		const FNNEAttributeValue* BlockSizeAttr = Attributes.GetAttributeValue(TEXT("blocksize"));
		if (BlockSizeAttr)
		{
			BlockSize = BlockSizeAttr->GetValue<int32>();
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("blocksize attribute is required"));
			return false;
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32>		InputShape = InputTensors[0]->GetShape().GetData();
		Util::FSmallUIntArray		OutputShape;

		OutputShape.SetNum(DIM_COUNT);
		OutputShape[N] = InputShape[N];
		OutputShape[C] = InputShape[C] * BlockSize * BlockSize;
		OutputShape[H] = InputShape[H] / BlockSize;
		OutputShape[W] = InputShape[W] / BlockSize;

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize input tensor for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize output tensor for DML inference"));
			return false;
		}

		DML_SPACE_TO_DEPTH1_OPERATOR_DESC DmlSpaceToDepthOpDesc{};

		DmlSpaceToDepthOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlSpaceToDepthOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlSpaceToDepthOpDesc.BlockSize = BlockSize;
		DmlSpaceToDepthOpDesc.Order = DML_DEPTH_SPACE_ORDER_DEPTH_COLUMN_ROW;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_SPACE_TO_DEPTH1, &DmlSpaceToDepthOpDesc });
	}
};

// Register SpaceToDepth operator on Module startup
NNE_DML_REGISTER_OP_VERSION(SpaceToDepth, 1)
NNE_DML_REGISTER_OP_VERSION(SpaceToDepth, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
