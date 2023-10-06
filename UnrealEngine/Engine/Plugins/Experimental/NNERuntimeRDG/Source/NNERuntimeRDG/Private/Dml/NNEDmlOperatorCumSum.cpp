// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlCumSum : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlCumSum();
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
		check(InputTensors.Num() == 2);
		check(OutputTensors.Num() == 1);

		ConstantCPUInputs.Add(1);

		UINT Axis; 

        // axis tensor must be constant
        check(InputTensors[1].HasPreparedData());

		switch(InputTensors[1].GetDataType())
        {
        case ENNETensorDataType::Int32:
			{
				TConstArrayView<int32> TensorContent = InputTensors[1].GetPreparedData<int32>();
				if (TensorContent.Num() != 1)
				{
					UE_LOG(LogNNE, Error, TEXT("axis tensor should be 0-D"));
					return false;
				}
				Axis = (UINT) HandleNegativeAxis(TensorContent[0], InputTensors[0].GetShape().Rank());
			}
            break;

        case ENNETensorDataType::Int64:
			{
				TConstArrayView<int64> TensorContent = InputTensors[1].GetPreparedData<int64>();
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
				Axis = (UINT) HandleNegativeAxis(TensorContent[0], InputTensors[0].GetShape().Rank());
			}
            break;

        default:
            UE_LOG(LogNNE, Error, TEXT("axis tensor has invalid data type"));
			return false;
        };

		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

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
		DmlCumSumOpDesc.AxisDirection = (DML_AXIS_DIRECTION) Attributes.GetValueOrDefault<int32>(TEXT("reverse"), 0);
		DmlCumSumOpDesc.HasExclusiveSum = (bool) Attributes.GetValueOrDefault<int32>(TEXT("exclusive"), 0);

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_CUMULATIVE_SUMMATION, &DmlCumSumOpDesc });
	}
};

// Register CumSum operator on Module startup
NNE_DML_REGISTER_OP(CumSum)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
