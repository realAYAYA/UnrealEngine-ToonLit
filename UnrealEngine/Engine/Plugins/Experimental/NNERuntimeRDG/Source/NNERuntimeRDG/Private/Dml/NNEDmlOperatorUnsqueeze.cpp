// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlUnsqueeze : public FOperatorDml
{
	TArray<int32> Axes;
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;
	
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlUnsqueeze();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Unsqueeze");

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

		const FNNEAttributeValue* AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));
		if (!AxesAttr)
		{
			UE_LOG(LogNNE, Warning, TEXT("Missing axes attribute for operator Unsqueeze"));
			return false;
		}

		Axes = AxesAttr->GetValue<TArray<int32>>();
		
		const int32 OutShapeRank = Inputs[0].GetShape().Rank() + Axes.Num();

		HandleNegativeAxes(Axes, OutShapeRank);
		Axes.Sort();

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32>	InputShape = InputTensors[0]->GetShape().GetData();
		const int32				OutShapeRank = InputShape.Num() + Axes.Num();

		Util::FSmallUIntArray	OutputShape;
		OutputShape.Reserve(OutShapeRank);
		OutputShape.Append(InputShape);

		for (int32 Idx = 0; Idx < Axes.Num(); ++Idx)
		{
			if (Axes[Idx] >= OutShapeRank)
			{
				UE_LOG(LogNNE, Warning, TEXT("Unsqueeze operator does not support axes greater than the number of dimensions of the resulting tensor shape"));
				return false;
			}

			OutputShape.Insert(1, Axes[Idx]);
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml DmlTensorDesc;
		
		if (!DmlTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Unsqueeze's output tensor for DML inference"));
			return false;
		}

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Reshape operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Unsqueeze, 1)
NNE_DML_REGISTER_OP_VERSION(Unsqueeze, 11)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
