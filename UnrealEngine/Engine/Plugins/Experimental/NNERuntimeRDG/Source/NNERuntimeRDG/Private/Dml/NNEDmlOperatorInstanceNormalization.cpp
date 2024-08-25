// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlInstanceNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 0.00001f;
	float Epsilon;
	static constexpr uint32 NumAllowedInputTensors = 3, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 2, MaxTensorRank = 4;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlInstanceNormalization();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("InstanceNormalization");

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

		if (!CheckGenericTensor1D(OpName, InputTypes[1], InputShapes[1], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
			}
		  	))
		{
			return false;
		}

		if (!CheckGenericTensor1D(OpName, InputTypes[2], InputShapes[2], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
			}
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
		Epsilon = Attributes.GetValueOrDefault<float>(TEXT("epsilon"), DefaultEpsilon);

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

		// DML accepts only (N x C x H x W) shapes. When a smaller shape is provided we have to fill with 1s after N and C.
		TArray<uint32> InputShape(InputTensor.GetShape().GetData());
		{
			const int32 InputShapeOffset = 4 - InputShape.Num();

			for (int32 Idx = 0; Idx < InputShapeOffset; ++Idx)
			{
				InputShape.Insert(1, 2);
			}
		}

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlScalingTensorDesc;
		FTensorDescDml	DmlBiasTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;
			
		// Make sure that input is 4D
		if (!DmlInputTensorDesc
				.SetTensorRank(MaxTensorRank, MaxTensorRank)
				.SetFromTensor(InputTensor)
				.SetShape(InputShape)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 1)
		{
			const NNE::Internal::FTensor& ScaleTensor = *InputTensors[1];

			if (!DmlScalingTensorDesc
					.SetTensorRank(MaxTensorRank, MaxTensorRank)
					.SetFromTensor1D(ScaleTensor, InputTensor.GetShape().Rank())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& BiasTensor = *InputTensors[2];

			if (!DmlBiasTensorDesc
					.SetTensorRank(MaxTensorRank, MaxTensorRank)
					.SetFromTensor1D(BiasTensor, InputTensor.GetShape().Rank())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
					.SetTensorRank(MaxTensorRank, MaxTensorRank)
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
NNE_DML_REGISTER_OP_VERSION(InstanceNormalization, 6)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
