// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlGather : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlGather();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == 2);
		check(OutputTensors.Num() == 1);
		
		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& IndicesTensor = InputTensors[1];
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		const NNECore::FTensorShape& InputShape = InputTensor.GetShape();
		const NNECore::FTensorShape& IndicesShape = IndicesTensor.GetShape();

		if (IndicesShape.Rank() > InputShape.Rank())
		{
			UE_LOG(LogNNE, Warning, TEXT("Indices tensor rank must match input tensor rank"));
			return false;
		}

		if (IndicesTensor.GetDataType() != ENNETensorDataType::UInt32 && IndicesTensor.GetDataType() != ENNETensorDataType::Int32)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML only supports UINT32/INT32 for indices tensor"));
			return false;
		}

		if (InputTensor.GetDataType() != OutputTensor.GetDataType())
		{
			UE_LOG(LogNNE, Warning, TEXT("Input and Output tensor should have same data type"));
			return false;
		}

		// Read attributes
		int32	Axis = Attributes.GetValueOrDefault<int>(TEXT("axis"), 0);

		Axis = HandleNegativeAxis(Axis, InputTensor.GetShape().Rank());

		// Compute output shape
		const int32 OutputRank = IndicesShape.Rank() + InputShape.Rank() - 1;
		DmlUtil::FSmallUIntArray OutputShape;
		int32 DataRankIdx = 0;

		for (; DataRankIdx < Axis; ++DataRankIdx)
		{
			OutputShape.Add(InputShape.GetData()[DataRankIdx]);
		}

		OutputShape.Append(IndicesShape.GetData());
		++DataRankIdx;

		for (; DataRankIdx < InputShape.Rank(); ++DataRankIdx)
		{
			OutputShape.Add(InputShape.GetData()[DataRankIdx]);
		}

		if (InputShape.Rank() != OutputRank)
		{
			UE_LOG(LogNNE, Warning, TEXT("Output tensor rank must match input tensor rank"));
			return false;
		}

		// Initialize tensor descriptors
		DmlUtil::FTensorDesc	DmlInputTensor{};
		DmlUtil::FTensorDesc	DmlIndicesTensor{};
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlInputTensor.InitFromTensor(InputTensor, 1))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlIndicesTensor.InitFromTensor(IndicesTensor, InputShape.Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, InputShape.Rank(), MakeEmptyArrayView<uint32>(), OutputShape))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_GATHER_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensor.Desc;
		OpDesc.IndicesTensor = &DmlIndicesTensor.Desc;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
		OpDesc.Axis = Axis;
		OpDesc.IndexDimensions = IndicesTensor.GetShape().Rank();

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_GATHER, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(Gather)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
