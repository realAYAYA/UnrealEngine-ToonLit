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
		// Setup attributes
		float Alpha = 1.0f;
		float Beta = 1.0f;
		int32 TransA = 0;
		int32 TransB = 0;
		
		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		TransA = Attributes.GetValueOrDefault(TEXT("transA"), TransA);
		TransB = Attributes.GetValueOrDefault(TEXT("transB"), TransB);
		
		const NNE::Internal::FTensor& InputATensor = InputTensors[0];
		const NNE::Internal::FTensor& InputBTensor = InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlInputCTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(InputATensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(InputBTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& InputCTensor = InputTensors[2];

			if (!DmlInputCTensorDesc
					.SetTensorRank(2, 4)
					.SetFromTensorBroadcast(InputCTensor, OutputTensor.GetShape())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_GEMM_OPERATOR_DESC	DmlGemmOpDesc{};

		DmlGemmOpDesc.ATensor = DmlInputATensorDesc.GetDmlDesc();
		DmlGemmOpDesc.BTensor = DmlInputBTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.CTensor = InputTensors.Num() > 2 ? DmlInputCTensorDesc.GetDmlDesc() : nullptr;
		DmlGemmOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.Alpha = Alpha;
		DmlGemmOpDesc.Beta = Beta;
		DmlGemmOpDesc.TransA = TransA ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;
		DmlGemmOpDesc.TransB = TransB ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_GEMM, &DmlGemmOpDesc });
	}
};

NNE_DML_REGISTER_OP(Gemm)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
