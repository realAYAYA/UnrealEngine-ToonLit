// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
//
//
class FOperatorDmlMeanVarianceNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlMeanVarianceNormalization();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == 1);
		check(OutputTensors.Num() == 1);

		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		const int32 AcrossChannels = Attributes.GetValueOrDefault<int32>(TEXT("across_channels"), 0);
		const int32 NormalizeVariance = Attributes.GetValueOrDefault<int32>(TEXT("normalize_variance"), 1);

		TArray<int32>	OnnxAxes;

		const FNNEAttributeValue* AttrAxes = Attributes.GetAttributeValue("axes");

		if (AttrAxes)
		{
			OnnxAxes = AttrAxes->GetValue<TArray<int32>>();
		}
		else
		{
			constexpr int32 CrossChannelAxes[] = { 0, 1, 2, 3};
			constexpr int32 NonChannelAxes[] = { 0, 2, 3 };

			if (AcrossChannels)
			{
				OnnxAxes.Append(CrossChannelAxes, UE_ARRAY_COUNT(CrossChannelAxes));
			}
			else
			{
				OnnxAxes.Append(NonChannelAxes, UE_ARRAY_COUNT(NonChannelAxes));
			}
		}

		DmlUtil::FTensorDesc	DmlInputTensor{};

		if (!DmlInputTensor.InitFromTensor(InputTensor, InputTensor.GetShape().Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, DmlInputTensor.Sizes.Num()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlUtil::FSmallUIntArray DmlAxes;
		SetDmlAxesFromOnnx(DmlAxes, DmlInputTensor.Sizes.Num(), OnnxAxes);

		DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensor.Desc;
		OpDesc.ScaleTensor = nullptr;
		OpDesc.BiasTensor = nullptr;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
		OpDesc.AxisCount = DmlAxes.Num();
		OpDesc.Axes = DmlAxes.GetData();
		OpDesc.NormalizeVariance = NormalizeVariance;
		OpDesc.Epsilon = DefaultEpsilon;
		OpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(MeanVarianceNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
