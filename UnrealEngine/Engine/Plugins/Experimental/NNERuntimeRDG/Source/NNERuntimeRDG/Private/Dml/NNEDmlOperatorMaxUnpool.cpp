// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlMaxUnpool : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlMaxUnpool();
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
		check(InputTensors.Num() == 2 || InputTensors.Num() == 3);
		check(OutputTensors.Num() == 1);

		if(InputTensors.Num() == 3)
		{
			ConstantCPUInputs.Add(2);
		}

		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& IndicesTensor = InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlIndicesTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(4, 4)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize input tensor for DML inference"));
			return false;
		}

		// DML required IndicesTensor to be in uint64 format, where ONNX allows int32 and int64
		if (IndicesTensor.GetDataType() != ENNETensorDataType::Int64)
		{
			UE_LOG(LogNNE, Error, TEXT("DML MaxUnpool requires UInt64, please use Int32 in your ONNX model"));
			return false;
		}

		if (!DmlIndicesTensorDesc
				.SetTensorRank(4, 4)
				.SetFromTensor(IndicesTensor)
				.SetDataType(ENNETensorDataType::UInt64)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize indices tensor for DML inference"));
			return false;
		}
		
		if (!DmlOutputTensorDesc
				.SetTensorRank(4, 4)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize output tensor for DML inference"));
			return false;
		}

		DML_MAX_UNPOOLING_OPERATOR_DESC DmlMaxUnpoolOpDesc{};

		DmlMaxUnpoolOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlMaxUnpoolOpDesc.IndicesTensor = DmlIndicesTensorDesc.GetDmlDesc();
		DmlMaxUnpoolOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MAX_UNPOOLING, &DmlMaxUnpoolOpDesc });
	}
};

// Register MaxUnpool operator on Module startup
NNE_DML_REGISTER_OP(MaxUnpool)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
