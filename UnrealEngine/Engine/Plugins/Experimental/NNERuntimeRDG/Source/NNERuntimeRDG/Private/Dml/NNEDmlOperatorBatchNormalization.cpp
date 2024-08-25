// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

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

	float Epsilon;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlBatchNormalization();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("BatchNormalization");

		if (InputShapes.Num() != Count)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), Count);
        	return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], {ENNETensorDataType::Float, ENNETensorDataType::Half}))
		{
			return false;
		}
		
		const int32 bTrainingMode = AttributeMap.GetValueOrDefault<int32>(TEXT("training_mode"), 0);
		if (bTrainingMode)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: training mode not supported"), *OpName);
			return false;
		}

		for (int32 Idx = X; Idx < Count; ++Idx)
		{
			if (!CheckGenericTensor(OpName, InputTypes[Idx], InputShapes[Idx], {ENNETensorDataType::Float, ENNETensorDataType::Half}))
			{
				return false;
			}
			
			if (!IsEqualOrBroadcastable(InputShapes[0].GetData(), InputShapes[Idx].GetData()))
			{
				UE_LOG(LogNNE, Warning, TEXT("DML %s: tensor %d has shape that's not equal or broadcastable to input tensor's"), *OpName, Idx);
				return false;
			}
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == Count);
		check(OutputTensors.Num() >= 1);

		RemappedInputs.Add(X);
		RemappedInputs.Add(Mean);
		RemappedInputs.Add(Variance);
		RemappedInputs.Add(Scale);
		RemappedInputs.Add(Bias);

		// Read attributes
		Epsilon = Attributes.GetValueOrDefault<float>(TEXT("epsilon"), DefaultEpsilon);
		const int32 bTrainingMode = Attributes.GetValueOrDefault<int32>(TEXT("training_mode"), 0);

		if (bTrainingMode || OutputTensors.Num() > 1)
		{
			UE_LOG(LogNNE, Error, TEXT("DML:BatchNormalization doesn't support training mode"));
			return false;
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		check(InputTensors.Num() == Count);
		check(OutputTensors.Num() == 1);

		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

		return 0;
	};

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml DmlInputTensors[Count];
			
		if (!DmlInputTensors[X]
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		bool bIsValid = true;

		bIsValid &= SetDmlTensorDesc(DmlInputTensors[Mean],		*InputTensors[Mean], InputTensor.GetShape().Rank());
		bIsValid &= SetDmlTensorDesc(DmlInputTensors[Variance],	*InputTensors[Variance], InputTensor.GetShape().Rank());
		bIsValid &= SetDmlTensorDesc(DmlInputTensors[Scale],	*InputTensors[Scale], InputTensor.GetShape().Rank());
		bIsValid &= SetDmlTensorDesc(DmlInputTensors[Bias],		*InputTensors[Bias], InputTensor.GetShape().Rank());

		if (!bIsValid)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
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

		OpDesc.InputTensor = DmlInputTensors[X].GetDmlDesc();
		OpDesc.MeanTensor = DmlInputTensors[Mean].GetDmlDesc();
		OpDesc.VarianceTensor = DmlInputTensors[Variance].GetDmlDesc();
		OpDesc.ScaleTensor = DmlInputTensors[Scale].GetDmlDesc();
		OpDesc.BiasTensor = DmlInputTensors[Bias].GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensor.GetDmlDesc();
		OpDesc.Spatial = static_cast<BOOL>(1);
		OpDesc.Epsilon = Epsilon;
		OpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_BATCH_NORMALIZATION, &OpDesc });
	}

private:

	inline bool SetDmlTensorDesc(FTensorDescDml& DmlTensorDesc, const NNE::Internal::FTensor& Tensor, int32 InputRank)
	{
		if (Tensor.GetShape().Rank() == 1)
		{
			if (!DmlTensorDesc
				.SetFromTensor1D(Tensor, InputRank)
				.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}
		else
		{
			if (!DmlTensorDesc
				.SetFromTensor(Tensor)
				.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		return true;
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP_VERSION(BatchNormalization, 9)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
