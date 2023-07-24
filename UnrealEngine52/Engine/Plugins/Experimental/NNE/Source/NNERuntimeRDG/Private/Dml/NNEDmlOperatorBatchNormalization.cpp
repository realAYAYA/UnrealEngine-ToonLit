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

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == Count);
		check(OutputTensors.Num() >= 1);

		RemappedInputs.Add(X);
		RemappedInputs.Add(Mean);
		RemappedInputs.Add(Variance);
		RemappedInputs.Add(Scale);
		RemappedInputs.Add(Bias);

		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		const float Epsilon = Attributes.GetValueOrDefault<float>(TEXT("epsilon"), DefaultEpsilon);
		const int32 bTrainingMode = Attributes.GetValueOrDefault<int32>(TEXT("training_mode"), 0);

		if (bTrainingMode || OutputTensors.Num() > 1)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML:BatchNormalization doesn't support training mode"));
			return false;
		}

		DmlUtil::FTensorDesc	DmlInputTensors[Count];
			
		if (!DmlInputTensors[X].InitFromTensor(InputTensor, InputTensor.GetShape().Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		for (int32 Idx = Scale; Idx < Count; ++Idx)
		{
			const NNECore::Internal::FTensor& CurrTensor = InputTensors[Idx];
			DmlUtil::FTensorDesc& DmlTensor = DmlInputTensors[Idx];

			DmlTensor = DmlUtil::FTensorDesc{};

			if (CurrTensor.GetShape().Rank() == 1)
			{
				if (!DmlTensor.InitFromTensor1D(CurrTensor, DmlInputTensors[X].Sizes.Num()))
				{
					UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
					return false;
				}
			}
			else
			{
				if (!DmlTensor.InitFromTensor(CurrTensor, DmlInputTensors[X].Sizes.Num()))
				{
					UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
					return false;
				}
			}
		}
		
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, DmlInputTensors[X].Sizes.Num()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_BATCH_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensors[X].Desc;
		OpDesc.MeanTensor = &DmlInputTensors[Mean].Desc;
		OpDesc.VarianceTensor = &DmlInputTensors[Variance].Desc;
		OpDesc.ScaleTensor = &DmlInputTensors[Scale].Desc;
		OpDesc.BiasTensor = &DmlInputTensors[Bias].Desc;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
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
