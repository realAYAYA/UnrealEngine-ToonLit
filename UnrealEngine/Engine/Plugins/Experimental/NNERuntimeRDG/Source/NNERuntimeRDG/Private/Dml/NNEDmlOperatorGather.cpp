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

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		//TODO
		return true;
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == 2);
		check(OutputTensors.Num() == 1);
		
		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& IndicesTensor = InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		const NNE::FTensorShape& InputShape = InputTensor.GetShape();
		const NNE::FTensorShape& IndicesShape = IndicesTensor.GetShape();

		if (IndicesShape.Rank() > InputShape.Rank())
		{
			UE_LOG(LogNNE, Warning, TEXT("Indices tensor rank must match input tensor rank"));
			return false;
		}

		ENNETensorDataType IndicesDataType = IndicesTensor.GetDataType();

		if (IndicesDataType != ENNETensorDataType::UInt32 && IndicesDataType != ENNETensorDataType::Int32 &&
			IndicesDataType != ENNETensorDataType::UInt64 && IndicesDataType != ENNETensorDataType::Int64)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML only supports UINT32/INT32/UINT64/INT64 for indices tensor"));
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
		Util::FSmallUIntArray OutputShape;
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

		if (OutputTensor.GetShape().Rank() != OutputRank)
		{
			UE_LOG(LogNNE, Warning, TEXT("Output tensor rank must match computed output tensor rank"));
			return false;
		}

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlIndicesTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlIndicesTensorDesc
				.SetFromTensor(IndicesTensor, InputTensor.GetShape().Rank())
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.SetShape(OutputShape, InputTensor.GetShape().Rank())
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_GATHER_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.IndicesTensor = DmlIndicesTensorDesc.GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.Axis = Axis;
		OpDesc.IndexDimensions = IndicesTensor.GetShape().Rank();

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_GATHER, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(Gather)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
