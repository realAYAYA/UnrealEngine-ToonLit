// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlBatchNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

	enum
	{
		X,
		Scale,
		Bias,
		Mean,
		Variance,
		Count
	};

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlBatchNormalization();
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
		check(InputTensors.Num() == Count);
		check(OutputTensors.Num() >= 1);

		RemappedInputs.Add(X);
		RemappedInputs.Add(Mean);
		RemappedInputs.Add(Variance);
		RemappedInputs.Add(Scale);
		RemappedInputs.Add(Bias);

		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Error, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		const float Epsilon = Attributes.GetValueOrDefault<float>(TEXT("epsilon"), DefaultEpsilon);
		const int32 bTrainingMode = Attributes.GetValueOrDefault<int32>(TEXT("training_mode"), 0);

		if (bTrainingMode || OutputTensors.Num() > 1)
		{
			UE_LOG(LogNNE, Error, TEXT("DML:BatchNormalization doesn't support training mode"));
			return false;
		}

		FTensorDescDml DmlInputTensorDescs[Count];
			
		if (!DmlInputTensorDescs[X]
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		for (int32 Idx = Scale; Idx < Count; ++Idx)
		{
			const NNE::Internal::FTensor& CurrTensor = InputTensors[Idx];
			FTensorDescDml& DmlCurrTensorDesc = DmlInputTensorDescs[Idx];

			if (CurrTensor.GetShape().Rank() == 1)
			{
				if (!DmlCurrTensorDesc
						.SetFromTensor1D(CurrTensor, InputTensor.GetShape().Rank())
						.Validate())
				{
					UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
					return false;
				}
			}
			else
			{
				if (!DmlCurrTensorDesc
						.SetFromTensor(CurrTensor)
						.Validate())
				{
					UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
					return false;
				}
			}
		}
		
		FTensorDescDml	DmlOutputTensor;

		if (!DmlOutputTensor
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_BATCH_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDescs[X].GetDmlDesc();
		OpDesc.MeanTensor = DmlInputTensorDescs[Mean].GetDmlDesc();
		OpDesc.VarianceTensor = DmlInputTensorDescs[Variance].GetDmlDesc();
		OpDesc.ScaleTensor = DmlInputTensorDescs[Scale].GetDmlDesc();
		OpDesc.BiasTensor = DmlInputTensorDescs[Bias].GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensor.GetDmlDesc();
		OpDesc.Spatial = static_cast<BOOL>(1);
		OpDesc.Epsilon = Epsilon;
		OpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_BATCH_NORMALIZATION, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(BatchNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
