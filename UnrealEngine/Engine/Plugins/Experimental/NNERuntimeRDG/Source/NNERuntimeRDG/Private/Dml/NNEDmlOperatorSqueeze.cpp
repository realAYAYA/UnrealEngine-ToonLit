// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlSqueeze : public FOperatorDml
{
	mutable TArray<int32>	Axes;
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSqueeze();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Squeeze");

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

		const int32					InputShapeRank = Inputs[0].GetShape().Rank();
		const FNNEAttributeValue*	AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));

		if (AxesAttr)
		{
			Axes = AxesAttr->GetValue<TArray<int32>>();
			HandleNegativeAxes(Axes, InputShapeRank);
			Algo::Sort(Axes);
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32>	InputShape = InputTensors[0]->GetShape().GetData();
		
		if (Axes.IsEmpty())
		{
			for (int32 Idx = 0; Idx < InputShape.Num(); ++Idx)
			{
				if (InputShape[Idx] == 1)
				{
					Axes.Add(Idx);
				}
			}
		}
		
		Util::FSmallUIntArray	OutputShape;

		OutputShape.Reserve(InputShape.Num());
		OutputShape.Append(InputShape);

		for (int32 Idx = Axes.Num() - 1; Idx >= 0; --Idx)
		{
			const int32 Axe = Axes[Idx];

			if (OutputShape[Axe] != 1)
			{
				UE_LOG(
					LogNNE, 
					Warning, 
					TEXT("Squeeze at axe %d for 'Data' (name: %s) should be targeting a dimension of size 1 but it is %d."), 
					Axe, 
					*InputTensors[0]->GetName(), 
					OutputShape[Axe]);
				
				return false;
			}

			OutputShape.RemoveAt(Axe);
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml DmlTensorDesc;
		
		if (!DmlTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Squeeze's output tensor for DML inference"));
			return false;
		}

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Squeeze operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Squeeze, 1)
NNE_DML_REGISTER_OP_VERSION(Squeeze, 11)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
