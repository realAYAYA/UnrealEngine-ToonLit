// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"
#include "Math/Range.h"
#include "Algo/Transform.h"
#include "Algo/Reverse.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlTranspose : public FOperatorDml
{
	// Apply permutations to input array view
	Util::FSmallUIntArray Permute(TConstArrayView<uint32> InputView) const
	{
		Util::FSmallUIntArray Permuted;

		for (int32 PermVal : Perm)
		{
			Permuted.Add(InputView[PermVal]);
		}

		return Permuted;
	};

	TArray<int32> Perm;

	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlTranspose();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Transpose");

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

		const int32 NumDims = Inputs[0].GetShape().Rank();
		check(NumDims > 0);
		check(NumDims == Outputs[0].GetShape().Rank());

		// Default permutation is reverse
		Util::FSmallIntArray ReversePerm;

		for (int Idx = NumDims - 1; Idx >= 0; Idx--)
		{
			ReversePerm.Add(Idx);
		}

		Perm = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("perm"), (TArray<int32>) ReversePerm);
		check(Perm.Num() == NumDims);
		return true;

	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		Util::FSmallUIntArray OutputShape = Permute(InputTensors[0]->GetShape().GetData());

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32>	InputShape = InputTensors[0]->GetShape().GetData();

		FTensorDescDml DmlInputTensorDesc;

		DmlInputTensorDesc
			.SetFromTensor(*InputTensors[0])
			.SetShape(Permute(InputShape))
			.SetStridesFromShape(InputShape)
		;

		DmlInputTensorDesc.SetStrides(Permute(DmlInputTensorDesc.GetStrides()));

		if (!DmlInputTensorDesc.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose input for DML inference"));
			return false;
		}

		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose output for DML inference"));
			return false;
		}

		check(Util::IsSameShape(DmlOutputTensorDesc, DmlInputTensorDesc));

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Transpose operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Transpose, 1)
NNE_DML_REGISTER_OP_VERSION(Transpose, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
