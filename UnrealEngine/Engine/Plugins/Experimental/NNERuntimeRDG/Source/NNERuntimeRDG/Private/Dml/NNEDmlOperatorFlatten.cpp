// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlFlatten : public FOperatorDml
{
	int32 Axis;
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlFlatten();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Flatten");
		
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

		const int32 InputRank = Inputs[0].GetShape().Rank();

		Axis = Attributes.GetValueOrDefault(TEXT("axis"), 1);
		if (Axis > InputRank || Axis < -InputRank)
		{
			UE_LOG(
				LogNNE, 
				Warning, 
				TEXT("Flatten 'Axis' attribute should be in the range [-r,r] with r being the rank of the input (name: %s) however axis is %d while rank is %d."), 
				*Inputs[0].GetName(), Axis, InputRank);
			
			return false;
		}

		Axis = HandleNegativeAxis(Axis, InputRank);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

		uint32 InnerDimSize = 1;

		for (int32 Idx = 0; Idx < Axis; ++Idx)
		{
			InnerDimSize *= InputShape[Idx];
		}

		Util::FSmallUIntArray	OutputShape;

		OutputShape.Reserve(InputShape.Num());
		OutputShape.Add(InnerDimSize);
		OutputShape.Add(InputTensors[0]->GetShape().Volume() / InnerDimSize);

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice * Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml DmlTensorDesc;
		
		if (!DmlTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Flatten's output tensor for DML inference"));
			return false;
		}

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Reshape operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Flatten, 1)
NNE_DML_REGISTER_OP_VERSION(Flatten, 9)
NNE_DML_REGISTER_OP_VERSION(Flatten, 11)
NNE_DML_REGISTER_OP_VERSION(Flatten, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
