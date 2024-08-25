// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlMeanVarianceNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

	Util::FSmallUIntArray	Axes;
	int32					AcrossChannels;
	int32					NormalizeVariance;
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlMeanVarianceNormalization();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("MeanVarianceNormalization");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == NumAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		// Read attributes
		AcrossChannels = Attributes.GetValueOrDefault<int32>(TEXT("across_channels"), 0);
		NormalizeVariance = Attributes.GetValueOrDefault<int32>(TEXT("normalize_variance"), 1);

		TArray<int32>	OnnxAxes;

		const FNNEAttributeValue* AttrAxes = Attributes.GetAttributeValue("axes");

		if (AttrAxes)
		{
			OnnxAxes = AttrAxes->GetValue<TArray<int32>>();
		}
		else
		{
			constexpr int32 CrossChannelAxes[] = { 0, 1, 2, 3 };
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

		SetDmlAxesFromOnnx(Axes, Inputs[0].GetShape().Rank(), OnnxAxes);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.ScaleTensor = nullptr;
		OpDesc.BiasTensor = nullptr;
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.AxisCount = Axes.Num();
		OpDesc.Axes = Axes.GetData();
		OpDesc.NormalizeVariance = NormalizeVariance;
		OpDesc.Epsilon = DefaultEpsilon;
		OpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP_VERSION(MeanVarianceNormalization, 9)
NNE_DML_REGISTER_OP_VERSION(MeanVarianceNormalization, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
