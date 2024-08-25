// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlGemm : public FOperatorDml
{
	static constexpr uint32 MinAllowedInputTensors = 2, MaxAllowedInputTensors = 3, NumAllowedOutputTensors = 1;
	static constexpr int32 MinTensorRank = 2, MaxTensorRank = 4;
	float Alpha = 1.0f;
	float Beta = 1.0f;
	int32 TransA = 0;
	int32 TransB = 0;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlGemm();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Gemm");

		if (InputShapes.Num() < MinAllowedInputTensors || InputShapes.Num() > MaxAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be in [%d, %d]."), 
										*OpName, InputShapes.Num(), MinAllowedInputTensors, MaxAllowedInputTensors);
			return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ ENNETensorDataType::Float, ENNETensorDataType::Half },
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[1], InputShapes[1], 
			{ ENNETensorDataType::Float, ENNETensorDataType::Half },
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if(InputShapes.Num() == 3)
		{
			if (!CheckGenericTensor(OpName, InputTypes[2], InputShapes[2], 
				{ ENNETensorDataType::Float, ENNETensorDataType::Half },
				0, MaxTensorRank
				))
			{
				return false;
			}
		}
		
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		TransA = Attributes.GetValueOrDefault(TEXT("transA"), TransA);
		TransB = Attributes.GetValueOrDefault(TEXT("transB"), TransB);
		
		const NNE::FTensorDesc& InputA = Inputs[0];
		const NNE::FTensorDesc& InputB = Inputs[1];

		if (InputA.GetShape().Rank() < MinTensorRank || InputA.GetShape().Rank() > MaxTensorRank)
		{
			UE_LOG(LogNNE, Error, TEXT("Gemm InputA tensor rank needs to be [%d,%d]"), MinTensorRank, MaxTensorRank);
			return false;
		}

		if (InputB.GetShape().Rank() < MinTensorRank || InputB.GetShape().Rank() > MaxTensorRank)
		{
			UE_LOG(LogNNE, Error, TEXT("Gemm InputB tensor rank needs to be [%d,%d]"), MinTensorRank, MaxTensorRank);
			return false;
		}

		if (Inputs.Num() == 3)
		{
			const NNE::FTensorDesc& InputC = Inputs[2];

			if (InputC.GetShape().Rank() > MaxTensorRank)
			{
				UE_LOG(LogNNE, Error, TEXT("Gemm InputC tensor rank needs to be max rank %d"), MaxTensorRank);
				return false;
			}
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		check(InputTensors.Num() >= MinAllowedInputTensors && InputTensors.Num() <= MaxAllowedInputTensors);
		check(OutputTensors.Num() == NumAllowedOutputTensors);

		const NNE::FTensorShape& InputA = InputTensors[0]->GetShape();
		const NNE::FTensorShape& InputB = InputTensors[1]->GetShape();
		
		checkf(InputA.Rank() >= MinTensorRank, TEXT("Gemm InputA needs to have tensor rank at least size of %d"), MinTensorRank);
		checkf(InputB.Rank() >= MinTensorRank, TEXT("Gemm InputB needs to have tensor rank at least size of %d"), MinTensorRank);

		const uint32 M = TransA != 0 ? InputA.GetData()[1] : InputA.GetData()[0];
		const uint32 N = TransB != 0 ? InputB.GetData()[0] : InputB.GetData()[1];
		
		TArray<uint32> OutputShape;
		OutputShape.Emplace(M);
		OutputShape.Emplace(N);

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputATensor = *InputTensors[0];
		const NNE::Internal::FTensor& InputBTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlInputCTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputATensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputBTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& InputCTensor = *InputTensors[2];

			if (!DmlInputCTensorDesc
					.SetTensorRank(MinTensorRank, MaxTensorRank)
					.SetFromTensorBroadcast(InputCTensor, OutputTensor.GetShape())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
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

NNE_DML_REGISTER_OP_VERSION(Gemm, 7)
NNE_DML_REGISTER_OP_VERSION(Gemm, 9)
NNE_DML_REGISTER_OP_VERSION(Gemm, 11)
NNE_DML_REGISTER_OP_VERSION(Gemm, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
