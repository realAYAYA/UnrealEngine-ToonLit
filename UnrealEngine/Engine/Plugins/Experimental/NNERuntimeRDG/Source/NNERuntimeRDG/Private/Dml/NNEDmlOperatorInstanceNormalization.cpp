// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlInstanceNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 0.00001f;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlInstanceNormalization();
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
		check(InputTensors.Num() >= 1 && InputTensors.Num() <= 3);
		check(OutputTensors.Num() == 1);

		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InstanceNormalization:InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		float	Epsilon = Attributes.GetValueOrDefault<float>(TEXT("epsilon"), DefaultEpsilon);

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlScalingTensorDesc;
		FTensorDescDml	DmlBiasTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;
			
		// Make sure that input is 4D
		if (!DmlInputTensorDesc
				.SetTensorRank(4, 4)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 1)
		{
			const NNE::Internal::FTensor& ScaleTensor = InputTensors[1];

			if (!DmlScalingTensorDesc
					.SetTensorRank(4, 4)
					.SetFromTensor1D(ScaleTensor, InputTensor.GetShape().Rank())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& BiasTensor = InputTensors[2];

			if (!DmlBiasTensorDesc
					.SetTensorRank(4, 4)
					.SetFromTensor1D(BiasTensor, InputTensor.GetShape().Rank())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
					.SetTensorRank(4, 4)
					.SetFromTensor(OutputTensor)
					.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

#ifdef NNE_USE_DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1

		// "Instance" normalization is really spatial normalization,
		// where the spatial channels are reduced and normalized, while
		// batch and channel remain independent. So pass a list of axes
		// just beyond the leading batch and channel dimensions (starting
		// at axis 2 up to the last spatial dimension).
		const int32 InputRank = DmlInputTensor.Sizes.Num();

		Util::FSmallUIntArray Axes;
		Axes.SetNumUninitialized(NcdhwSpatialDimensionCount);

		for (int32 Dim = 0, Value = InputRank - Axes.Num(); Dim < Axes.Num(); ++Dim, ++Value)
		{
			Axes[Dim] = Value;
		}

		DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.DmlDesc();
		OpDesc.ScaleTensor = InputTensors.Num() > 1 ? DmlScalingTensorDesc.DmlDesc() : nullptr;
		OpDesc.BiasTensor = InputTensors.Num() > 2 ? DmlBiasTensorDesc.DmlDesc() : nullptr;
		OpDesc.OutputTensor = DmlOutputTensorDesc.DmlDesc();
		OpDesc.AxisCount = Axes.Num();
		OpDesc.Axes = Axes.GetData();
		OpDesc.NormalizeVariance = true;
		OpDesc.Epsilon = Epsilon;
			
		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1, &OpDesc });
#else

		DML_MEAN_VARIANCE_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.ScaleTensor = InputTensors.Num() > 1 ? DmlScalingTensorDesc.GetDmlDesc() : nullptr;
		OpDesc.BiasTensor = InputTensors.Num() > 2 ? DmlBiasTensorDesc.GetDmlDesc() : nullptr;
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.CrossChannel = false;
		OpDesc.NormalizeVariance = true;
		OpDesc.Epsilon = Epsilon;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION, &OpDesc });

#endif
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(InstanceNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
