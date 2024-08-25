// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlGather : public FOperatorDml
{
	int32	Axis;
	static constexpr uint32 NumAllowedInputTensors = 2, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlGather();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Gather");

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

		if (!CheckGenericTensor(OpName, InputTypes[1], InputShapes[1], 
			{ 	ENNETensorDataType::Int64, ENNETensorDataType::Int32, 
				ENNETensorDataType::UInt64, ENNETensorDataType::UInt32
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

		const NNE::FTensorDesc& InputTensor = Inputs[0];
		const NNE::FTensorDesc& IndicesTensor = Inputs[1];
		const NNE::FTensorDesc& OutputTensor = Outputs[0];

		if (IndicesTensor.GetShape().Rank() > InputTensor.GetShape().Rank())
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
		Axis = Attributes.GetValueOrDefault<int>(TEXT("axis"), 0);
		Axis = HandleNegativeAxis(Axis, InputTensor.GetShape().Rank());

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::FTensorShape& InputShape = InputTensors[0]->GetShape();
		const NNE::FTensorShape& IndicesShape = InputTensors[1]->GetShape();

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

		if (OutputShape.Num() != OutputRank)
		{
			UE_LOG(LogNNE, Warning, TEXT("Output tensor rank must match computed output tensor rank"));
			return false;
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor	= *InputTensors[0];
		const NNE::Internal::FTensor& IndicesTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor	= *OutputTensors[0];

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
				.SetShape(OutputTensor.GetShape(), InputTensor.GetShape().Rank())
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
NNE_DML_REGISTER_OP_VERSION(Gather, 1)
NNE_DML_REGISTER_OP_VERSION(Gather, 11)
NNE_DML_REGISTER_OP_VERSION(Gather, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
