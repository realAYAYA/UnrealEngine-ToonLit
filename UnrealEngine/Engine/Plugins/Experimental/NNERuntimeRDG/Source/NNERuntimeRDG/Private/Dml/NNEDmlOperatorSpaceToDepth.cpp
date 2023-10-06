// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlSpaceToDepth : public FOperatorDml
{


public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSpaceToDepth();
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

		int32 BlockSize;

		const FNNEAttributeValue* BlockSizeAttr = Attributes.GetAttributeValue(TEXT("blocksize"));
		if (BlockSizeAttr)
		{
			BlockSize = BlockSizeAttr->GetValue<int32>();
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("blocksize attribute is required"));
			return false;
		}


		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize input tensor for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize output tensor for DML inference"));
			return false;
		}

		DML_SPACE_TO_DEPTH1_OPERATOR_DESC DmlSpaceToDepthOpDesc{};

		DmlSpaceToDepthOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlSpaceToDepthOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlSpaceToDepthOpDesc.BlockSize = BlockSize;
		DmlSpaceToDepthOpDesc.Order = DML_DEPTH_SPACE_ORDER_DEPTH_COLUMN_ROW;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_SPACE_TO_DEPTH1, &DmlSpaceToDepthOpDesc });
	}
};

// Register SpaceToDepth operator on Module startup
NNE_DML_REGISTER_OP(SpaceToDepth)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
