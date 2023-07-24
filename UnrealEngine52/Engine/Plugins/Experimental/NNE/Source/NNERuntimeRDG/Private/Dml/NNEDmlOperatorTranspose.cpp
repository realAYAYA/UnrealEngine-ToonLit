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

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
        check(InputTensors.Num() == 1);
        check(OutputTensors.Num() == 1);

        int32 NumDims = InputTensors[0].GetShape().Rank();
        check(NumDims > 0);
        check(NumDims == OutputTensors[0].GetShape().Rank());

        // Default permutation is reverse
        DmlUtil::FSmallIntArray ReversePerm;
        for(int Idx = NumDims-1; Idx >= 0; Idx--)
        {
            ReversePerm.Add(Idx);
        }
        TArray<int32> Perm = 
			Attributes.GetValueOrDefault<TArray<int32>>(TEXT("perm"), (TArray<int32>) ReversePerm);

        check(Perm.Num() == NumDims);

        // Initialize Input tensor desc
        DmlUtil::FTensorDesc DmlInputTensorDesc;
        if (!DmlInputTensorDesc.InitFromTensor(InputTensors[0], NumDims))
        {
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose input for DML inference"));
            return false;
        }
        
        DmlInputTensorDesc.SetStridesFromTensor(InputTensors[0]);

        // Initialize Output tensor desc
        DmlUtil::FTensorDesc DmlOutputTensorDesc;
        if (!DmlOutputTensorDesc.InitFromTensor(OutputTensors[0], NumDims))
        {
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose output for DML inference"));
            return false;
        }

        // Apply permutations to both sizes and strides
        auto PermuteFunc = [&, Perm] (TConstArrayView<uint32> InputView) -> DmlUtil::FSmallUIntArray
            {
                DmlUtil::FSmallUIntArray Permuted;
                for(int32 PermVal : Perm)
                {
                    Permuted.Add(InputView[PermVal]);
                }
                return Permuted;
            };
        
        DmlInputTensorDesc.UpdateShapeAndStrides(PermuteFunc(DmlInputTensorDesc.Sizes), PermuteFunc(DmlInputTensorDesc.Strides));
        
        check(DmlOutputTensorDesc.Sizes == DmlInputTensorDesc.Sizes);        

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

        DmlIdentityOpDesc.InputTensor = &DmlInputTensorDesc.Desc;
        DmlIdentityOpDesc.OutputTensor = &DmlOutputTensorDesc.Desc;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc} );

	}
};

// Register Transpose operator on Module startup
NNE_DML_REGISTER_OP(Transpose)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
