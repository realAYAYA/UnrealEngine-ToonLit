// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlMaxUnpool : public FOperatorDml
{
	class FUnpoolingArgs : public FKernelArgs
	{
		Util::FSmallUIntArray	KernelShape;

	public:

		bool Init(const NNE::FAttributeMap& Attributes, const int32 InputShapeRank)
		{
			if (!FKernelArgs::Init(Attributes, InputShapeRank, /*bIsGlobalKernel*/ false, /*bIsTransposed*/ true))
			{
				return false;
			}

			if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("kernel_shape")), KernelShape))
			{
				UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute cast led to overflow"));
				return false;
			}

			if (KernelShape.IsEmpty())
			{
				UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute is required for pooling operators"));
				return false;
			}

			check(KernelShape.Num() == NumDimensions);

			for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
			{
				WindowSize.Add(uint32(KernelShape[KernelShape.Num() - NumDimensions + Dim]));
			}

			return true;
		}		
	};

	mutable FUnpoolingArgs Args;
	static constexpr uint32 MinAllowedInputTensors = 2, MaxAllowedInputTensors = 3, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 4, MaxTensorRank = 4;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlMaxUnpool();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("MaxUnpool");

		if (InputShapes.Num() < MinAllowedInputTensors || InputShapes.Num() > MaxAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be in [%d, %d]."), 
										*OpName, InputShapes.Num(), MinAllowedInputTensors, MaxAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ ENNETensorDataType::Float, ENNETensorDataType::Half, 
			  ENNETensorDataType::Int64, ENNETensorDataType::Int32, 
			  ENNETensorDataType::Int16, ENNETensorDataType::Int8, 
			  ENNETensorDataType::UInt64, ENNETensorDataType::UInt32,
			  ENNETensorDataType::UInt16, ENNETensorDataType::UInt8 },
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[1], InputShapes[1], 
			{ 	ENNETensorDataType::Int64
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if(InputShapes.Num() == 3)
		{
			if (!CheckGenericTensor1D(OpName, InputTypes[2], InputShapes[2], 
				{ 	ENNETensorDataType::Int64
				}
				))
			{
				return false;
			}
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() >= MinAllowedInputTensors && Inputs.Num() <= MaxAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		if (Inputs.Num() == 3)
		{
			ConstantCPUInputs.Add(2);
		}

		// DML required IndicesTensor to be in uint64 format, where ONNX allows int32 and int64
		if (Inputs[1].GetDataType() != ENNETensorDataType::Int64)
		{
			UE_LOG(LogNNE, Error, TEXT("DML MaxUnpool requires UInt64, please use Int64 in your ONNX model"));
			return false;
		}

		if (!Args.Init(Attributes, Inputs[0].GetShape().Rank()))
		{
			UE_LOG(LogNNE, Error, TEXT("DML MaxUnpool failed to initialize arguments"));
			return false;
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		Args.Evaluate(InputTensors[0]->GetShape().GetData());

		if (InputTensors.Num() == 3)
		{
			TConstArrayView<int64>	Shape = InputTensors[2]->GetPreparedData<int64>();
			Util::FSmallUIntArray	OutputShape;

			for (int32 Idx = 0; Idx < Shape.Num(); ++Idx)
			{
				int64 ClampedVal = Shape[Idx];
				
				ClampedVal = ClampedVal < INT32_MIN ? INT32_MIN : ClampedVal;
				ClampedVal = ClampedVal > INT32_MAX ? INT32_MAX : ClampedVal;

				OutputShape.Add(static_cast<uint32>(ClampedVal));
			}

			OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));
		}
		else
		{
			OutputTensors[0]->SetShape(NNE::FTensorShape::Make(Args.GetOutputShape()));
		}

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& IndicesTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlIndicesTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize input tensor for DML inference"));
			return false;
		}

		if (!DmlIndicesTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(IndicesTensor)
				.SetDataType(ENNETensorDataType::UInt64)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize indices tensor for DML inference"));
			return false;
		}
		
		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize output tensor for DML inference"));
			return false;
		}

		DML_MAX_UNPOOLING_OPERATOR_DESC DmlMaxUnpoolOpDesc{};

		DmlMaxUnpoolOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlMaxUnpoolOpDesc.IndicesTensor = DmlIndicesTensorDesc.GetDmlDesc();
		DmlMaxUnpoolOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MAX_UNPOOLING, &DmlMaxUnpoolOpDesc });
	}
};

// Register MaxUnpool operator on Module startup
NNE_DML_REGISTER_OP_VERSION(MaxUnpool, 9)
NNE_DML_REGISTER_OP_VERSION(MaxUnpool, 11)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
