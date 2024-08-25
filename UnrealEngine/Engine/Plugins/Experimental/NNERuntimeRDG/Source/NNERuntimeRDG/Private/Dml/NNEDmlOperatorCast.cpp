// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlCast : public FOperatorDml
{
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlCast();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Cast");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ENNETensorDataType::Float, ENNETensorDataType::Half, ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
			ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, ENNETensorDataType::UInt16, ENNETensorDataType::UInt8}
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
		
		TConstArrayView<int32> InputShape = Inputs[0].GetShape().GetData();
		TConstArrayView<int32> OutputShape = Outputs[0].GetShape().GetData();

		ENNETensorDataType To = (ENNETensorDataType) Attributes.GetValue<int32>(TEXT("to"));
		
		if (To != Outputs[0].GetDataType())
		{
			UE_LOG(LogNNE, Error, TEXT("Cast should output a tensor of type %d but was of type %d."), int(To), int(Outputs[0].GetDataType()));
			return false;
		}

		if (InputShape.Num() != OutputShape.Num())
		{
			UE_LOG(LogNNE, Error, TEXT("Cast input and output shapes need to have a same rank"));
			return false;
		}

		for (int32 Idx = 0; Idx < InputShape.Num(); ++Idx)
		{
			if (InputShape[Idx] != OutputShape[Idx])
			{
				UE_LOG(LogNNE, Error, TEXT("Input shape and output shape need to have a same dimension at dim %d (%d != %d"), Idx, InputShape[Idx], OutputShape[Idx]);
				return false;
			}
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		check(InputTensors.Num() == NumAllowedInputTensors);
		check(OutputTensors.Num() == NumAllowedOutputTensors);
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

		return 0;
	};

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		// Initialize tensor descriptors
		FTensorDescDml DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_CAST_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_CAST, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Cast, 6)
NNE_DML_REGISTER_OP_VERSION(Cast, 9)
NNE_DML_REGISTER_OP_VERSION(Cast, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
