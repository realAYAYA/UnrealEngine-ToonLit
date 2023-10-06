// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"
#include "NNEUtilsLogHelper.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlCast : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlCast();
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
		check(InputTensors.Num() == 1);
		check(OutputTensors.Num() == 1);
		
		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		ENNETensorDataType To = (ENNETensorDataType) Attributes.GetValue<int32>(TEXT("to"));
		
		if (To != OutputTensor.GetDataType())
		{
			UE_LOG(LogNNE, Error, TEXT("Cast should output a tensor of type %d but was of type %d."), int(To), int(OutputTensor.GetDataType()));
			return false;
		}

		TConstArrayView<uint32> InputShape = InputTensor.GetShape().GetData();
		TConstArrayView<uint32> OutputShape = OutputTensor.GetShape().GetData();

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
NNE_DML_REGISTER_OP(Cast)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
