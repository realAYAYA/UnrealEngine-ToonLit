// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlCumSum : public FOperatorDml
{
	DML_AXIS_DIRECTION	AxisDirection;
	int32				HasExclusiveSum;
	mutable int32		Axis;
	static constexpr uint32 NumAllowedInputTensors = 2, NumAllowedOutputTensors = 1;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlCumSum();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("CumSum");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ ENNETensorDataType::Float, ENNETensorDataType::Half, 
			  ENNETensorDataType::Int64, ENNETensorDataType::Int32, 
			  ENNETensorDataType::UInt64, ENNETensorDataType::UInt32 }
		  	))
		{
			return false;
		}

		//axis is scalar
		if (!CheckGenericTensor1D(OpName, InputTypes[1], InputShapes[1], 
			{ ENNETensorDataType::Int64, ENNETensorDataType::Int32 }
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

		ConstantCPUInputs.Add(1);

		AxisDirection = (DML_AXIS_DIRECTION) Attributes.GetValueOrDefault<int32>(TEXT("reverse"), 0);
		HasExclusiveSum = Attributes.GetValueOrDefault<int32>(TEXT("exclusive"), 0);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		// axis tensor must be constant
		check(InputTensors[1]->HasPreparedData());

		switch(InputTensors[1]->GetDataType())
		{
			case ENNETensorDataType::Int32:
			{
				TConstArrayView<int32> TensorContent = InputTensors[1]->GetPreparedData<int32>();
				if (TensorContent.Num() != 1)
				{
					UE_LOG(LogNNE, Error, TEXT("axis tensor should be 0-D"));
					return false;
				}
				
				Axis = HandleNegativeAxis(TensorContent[0], InputTensors[0]->GetShape().Rank());
			}
			break;

			case ENNETensorDataType::Int64:
			{
				TConstArrayView<int64> TensorContent = InputTensors[1]->GetPreparedData<int64>();
				if (TensorContent.Num() != 1)
				{
					UE_LOG(LogNNE, Error, TEXT("axis tensor should be 0-D"));
					return false;
				}
				
				if (Util::IsOverflowing<int64, int32>(TensorContent[0]))
				{
					UE_LOG(LogNNE, Error, TEXT("axis number is overflowing"));
					return false;
				}
				
				Axis = HandleNegativeAxis(TensorContent[0], InputTensors[0]->GetShape().Rank());
			}
			break;

			default:
				UE_LOG(LogNNE, Error, TEXT("axis tensor has invalid data type"));
				return -1;
		};

		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize input tensor for DML inference"));
			return false;
		}
		
		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize output tensor for DML inference"));
			return false;
		}

		DML_CUMULATIVE_SUMMATION_OPERATOR_DESC DmlCumSumOpDesc{};

		DmlCumSumOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlCumSumOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlCumSumOpDesc.Axis = Axis;
		DmlCumSumOpDesc.AxisDirection = AxisDirection;
		DmlCumSumOpDesc.HasExclusiveSum = HasExclusiveSum;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_CUMULATIVE_SUMMATION, &DmlCumSumOpDesc });
	}
};

// Register CumSum operator on Module startup
NNE_DML_REGISTER_OP_VERSION(CumSum, 11)
NNE_DML_REGISTER_OP_VERSION(CumSum, 14)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
