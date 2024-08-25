// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlLRN : public FOperatorDml
{
	int32 Size;
	float Alpha;
	float Beta;
	float Bias;
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 4, MaxTensorRank = 4;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlLRN();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("LRN");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
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

		// Read attributes
		Size = Attributes.GetValueOrDefault<int32>(TEXT("size"), 0);
		Alpha = Attributes.GetValueOrDefault<float>(TEXT("alpha"), 0.0f);
		Beta = Attributes.GetValueOrDefault<float>(TEXT("beta"), 0.0f);
		Bias = Attributes.GetValueOrDefault<float>(TEXT("bias"), 0.0f);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_LOCAL_RESPONSE_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.CrossChannel = true; // ONNX only supports cross-channel
		OpDesc.LocalSize = Size;
		OpDesc.Alpha = Alpha;
		OpDesc.Beta = Beta;
		OpDesc.Bias = Bias;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_LOCAL_RESPONSE_NORMALIZATION, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP_VERSION(LRN, 1)
NNE_DML_REGISTER_OP_VERSION(LRN, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
