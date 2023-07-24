// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Gemm
 */
class FOperatorDmlGemm : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlGemm();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		// Setup attributes
		float Alpha = 1.0f;
		float Beta = 1.0f;
		int32 TransA = 0;
		int32 TransB = 0;
		
		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		TransA = Attributes.GetValueOrDefault(TEXT("transA"), TransA);
		TransB = Attributes.GetValueOrDefault(TEXT("transB"), TransB);
		
		const NNECore::Internal::FTensor& InputATensorDesc = InputTensors[0];
		const NNECore::Internal::FTensor& InputBTensorDesc = InputTensors[1];
		const NNECore::Internal::FTensor& OutputTensorDesc = OutputTensors[0];

		// Initialize tensor descriptors
		DmlUtil::FTensorDesc	DmlInputATensorDesc{};
		DmlUtil::FTensorDesc	DmlInputBTensorDesc{};
		DmlUtil::FTensorDesc	DmlInputCTensorDesc{};
		DmlUtil::FTensorDesc	DmlOutputTensorDesc{};

		if (!InitDmlTensorDesc(DmlInputATensorDesc, InputATensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!InitDmlTensorDesc(DmlInputBTensorDesc, InputBTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 2)
		{
			const NNECore::Internal::FTensor& InputCTensorDesc = InputTensors[2];

			if (!InitDmlTensorDesc(DmlInputCTensorDesc, InputCTensorDesc, OutputTensorDesc))
			{
				UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!InitDmlTensorDesc(DmlOutputTensorDesc, OutputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_GEMM_OPERATOR_DESC	DmlGemmOpDesc{};

		DmlGemmOpDesc.ATensor = &DmlInputATensorDesc.Desc;
		DmlGemmOpDesc.BTensor = &DmlInputBTensorDesc.Desc;
		DmlGemmOpDesc.CTensor = InputTensors.Num() > 2 ? &DmlInputCTensorDesc.Desc : nullptr;
		DmlGemmOpDesc.OutputTensor = &DmlOutputTensorDesc.Desc;
		DmlGemmOpDesc.Alpha = Alpha;
		DmlGemmOpDesc.Beta = Beta;
		DmlGemmOpDesc.TransA = TransA ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;
		DmlGemmOpDesc.TransB = TransB ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DML_OPERATOR_GEMM;
		DmlOpDesc.Desc = &DmlGemmOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

};

NNE_DML_REGISTER_OP(Gemm)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
