// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlFlatten : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlFlatten();
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

		TConstArrayView<uint32>	InputShape = InputTensors[0].GetShape().GetData();
		int32					InputRank = InputShape.Num();
		int32					Axis = Attributes.GetValueOrDefault(TEXT("axis"), 1);
		
		if (Axis > InputRank || Axis < -InputRank)
		{
			UE_LOG(LogNNE, Warning, TEXT("Flatten 'Axis' attribute should be in the range [-r,r] with r being the rank of the input (name: %s) however axis is %d while rank is %d."), *InputTensors[0].GetName(), Axis, InputRank);
			return false;
		}

		HandleNegativeAxis(Axis, InputRank);

		uint32 InnerDimSize = 1;

		for (int32 Idx = 0; Idx < Axis; ++Idx)
		{
			InnerDimSize *= InputShape[Idx];
		}

		Util::FSmallUIntArray	OutputShape;

		OutputShape.Reserve(InputShape.Num());
		OutputShape.Add(InnerDimSize);
		OutputShape.Add(InputTensors[0].GetShape().Volume() / InnerDimSize);
		
		FTensorDescDml DmlTensorDesc;
		
		if (!DmlTensorDesc
				.SetFromTensor(OutputTensors[0])
				.SetShape(OutputShape)
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
NNE_DML_REGISTER_OP(Flatten)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
