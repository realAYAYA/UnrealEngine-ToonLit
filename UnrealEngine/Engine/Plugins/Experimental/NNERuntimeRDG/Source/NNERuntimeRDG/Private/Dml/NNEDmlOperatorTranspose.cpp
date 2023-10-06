// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "Math/Range.h"
#include "Algo/Transform.h"
#include "Algo/Reverse.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Transpose
 */
class FOperatorDmlTranspose : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlTranspose();
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

		int32 NumDims = InputTensors[0].GetShape().Rank();
		check(NumDims > 0);
		check(NumDims == OutputTensors[0].GetShape().Rank());

		// Default permutation is reverse
		Util::FSmallIntArray ReversePerm;

		for(int Idx = NumDims-1; Idx >= 0; Idx--)
		{
			ReversePerm.Add(Idx);
		}

		TArray<int32> Perm = 
			Attributes.GetValueOrDefault<TArray<int32>>(TEXT("perm"), (TArray<int32>) ReversePerm);

		check(Perm.Num() == NumDims);

		// Initialize Input tensor desc
		// Apply permutations to both sizes and strides
		auto PermuteFunc = [&, Perm] (TConstArrayView<uint32> InputView) -> Util::FSmallUIntArray
		{
			Util::FSmallUIntArray Permuted;
			
			for (int32 PermVal : Perm)
			{
				Permuted.Add(InputView[PermVal]);
			}
			
			return Permuted;
		};

		FTensorDescDml DmlInputTensorDesc;

		DmlInputTensorDesc.SetFromTensor(InputTensors[0]);
		DmlInputTensorDesc.SetShape(PermuteFunc(DmlInputTensorDesc.GetSizes()));

		DmlInputTensorDesc.SetStridesFromShape(InputTensors[0].GetShape());
		DmlInputTensorDesc.SetStrides(PermuteFunc(DmlInputTensorDesc.GetStrides()));

		if (!DmlInputTensorDesc.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose input for DML inference"));
			return false;
		}

		// Initialize Output tensor desc
		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose output for DML inference"));
			return false;
		}

		check(Util::IsSameShape(DmlOutputTensorDesc, DmlInputTensorDesc));

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc} );
	}
};

// Register Transpose operator on Module startup
NNE_DML_REGISTER_OP(Transpose)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
