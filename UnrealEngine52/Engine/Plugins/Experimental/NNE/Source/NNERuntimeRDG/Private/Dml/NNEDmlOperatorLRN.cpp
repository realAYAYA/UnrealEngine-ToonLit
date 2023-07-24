// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
//
//
class FOperatorDmlLRN : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlLRN();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == 1);
		check(OutputTensors.Num() == 1);

		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		const int32 Size = Attributes.GetValueOrDefault<int32>(TEXT("size"), 0);
		const float Alpha = Attributes.GetValueOrDefault<float>(TEXT("alpha"), 0.0f);
		const float Beta = Attributes.GetValueOrDefault<float>(TEXT("beta"), 0.0f);
		const float Bias = Attributes.GetValueOrDefault<float>(TEXT("bias"), 0.0f);

		DmlUtil::FTensorDesc	DmlInputTensor{};

		if (!DmlInputTensor.InitFromTensor(InputTensor, InputTensor.GetShape().Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, DmlInputTensor.Sizes.Num()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_LOCAL_RESPONSE_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensor.Desc;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
		OpDesc.CrossChannel = true; // ONNX only supports cross-channel
		OpDesc.LocalSize = Size;
		OpDesc.Alpha = Alpha;
		OpDesc.Beta = Beta;
		OpDesc.Bias = Bias;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_LOCAL_RESPONSE_NORMALIZATION, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(LRN)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
