// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

// Reduce operator covers:
// ReduceL1
// ReduceL2
// ReduceLogSum
// ReduceLogSumExp
// ReduceMin
// ReduceMax
// ReduceMean
// ReduceProd
// ReduceSum
// ReduceSumSquare
template<DML_REDUCE_FUNCTION ReduceFunc, TCHAR const *OpName>
class FOperatorDmlReduce : public FOperatorDml
{
	inline static void HandleEmptyAxes(TArray<int32>& Axes, int32 Rank)
	{
		if (Axes.IsEmpty())
		{
			Axes.SetNumUninitialized(Rank);

			for (int32 Idx = 0; Idx < Rank; ++Idx)
			{
				Axes[Idx] = Idx;
			}
		}
	}

	Util::FSmallUIntArray			Axes;
	mutable Util::FSmallUIntArray	ReducedDims;
	mutable Util::FSmallArray<bool>	IsReducedDims;
	int32							KeepDims;
	DML_AXIS_DIRECTION				AxisDirection{ DML_AXIS_DIRECTION_INCREASING };
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlReduce<ReduceFunc, OpName>();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half, 
				ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
				ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, 
				ENNETensorDataType::UInt16, ENNETensorDataType::UInt8
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
		checkf(Inputs.Num() == NumAllowedInputTensors, TEXT("Dml Reduce op supports only 1 input"));
		check(Outputs.Num() == NumAllowedOutputTensors);

		KeepDims = Attributes.GetValueOrDefault<int32>(TEXT("keepdims"), 1);

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX || ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
		{
			AxisDirection = (DML_AXIS_DIRECTION) Attributes.GetValueOrDefault<int32>(TEXT("select_last_index"), 0);
		}

		TArray<int32>	OnnxAxes;

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX || ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
		{
			OnnxAxes.Add(Attributes.GetValueOrDefault<int32>(TEXT("axis"), 0));
		}
		else
		{
			const FNNEAttributeValue* AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));
			if (AxesAttr)
			{
				OnnxAxes = AxesAttr->GetValue<TArray<int32>>();
			}
		}

		HandleNegativeAxes(OnnxAxes, Inputs[0].GetShape().Rank());
		HandleEmptyAxes(OnnxAxes, Inputs[0].GetShape().Rank());

		const int32 InputRank = Inputs[0].GetShape().Rank();

		for (int32& Dim : OnnxAxes)
		{
			checkf(Dim < InputRank, TEXT("Index out of bounds for Reduce axis"));
			Axes.Add(Dim);
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();
		Util::FSmallUIntArray	OutputShape;

		ReducedDims.Reset();
		ReducedDims.Append(InputShape);

		for (const uint32& Dim : Axes)
		{
			checkf(Dim < (uint32) ReducedDims.Num(), TEXT("Index out of bounds for Reduce axis"));
			ReducedDims[Dim] = 1;
		}

		if (!KeepDims)
		{
			IsReducedDims.SetNumZeroed(ReducedDims.Num());
			for (const uint32& Dim : Axes)
			{
				IsReducedDims[Dim] = true;
			}

			for (int32 Idx = 0; Idx < ReducedDims.Num(); ++Idx)
			{
				if (!IsReducedDims[Idx])
				{
					OutputShape.Add(ReducedDims[Idx]);
				}
			}			
		}
		else
		{
			OutputShape.Append(ReducedDims);
		}
		
		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc
				.SetFromTensor(*InputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce input tensor for DML inference"));
			return false;
		}

		Util::FSmallUIntArray	OutputShape;

		if (KeepDims)
		{
			OutputShape.Append(ReducedDims);
		}
		else
		{
			// Example:
			//     input dims: {3, 2, 2}
			//     axes: 1
			//     keepDims: 0
			// 
			// Shape: {3, 2}, but DML: {3, 1, 2}	
			TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

			OutputShape.SetNum(ReducedDims.Num());
			for (int32 Idx = 0; Idx < OutputShape.Num(); ++Idx)
			{
				OutputShape[Idx] = IsReducedDims[Idx] ? 1 : InputShape[Idx];
			}
		}
		
		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.SetShape(OutputShape)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce output tensor for DML inference"));
			return false;
		}

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX)
        {
            DML_ARGMAX_OPERATOR_DESC OpDesc;
            OpDesc.AxisDirection = AxisDirection;
            OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
            OpDesc.Axes = Axes.GetData();
            OpDesc.AxisCount = (uint32) Axes.Num();

			return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_ARGMAX, &OpDesc });
        }
        else if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
        {
            DML_ARGMIN_OPERATOR_DESC OpDesc;
            OpDesc.AxisDirection = AxisDirection;
            OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
            OpDesc.Axes = Axes.GetData();
            OpDesc.AxisCount = (uint32) Axes.Num();

            return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_ARGMIN, &OpDesc });
        }
		else
		{
			DML_REDUCE_OPERATOR_DESC OpDesc{};

			OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
			OpDesc.Function = ReduceFunc;
			OpDesc.Axes = Axes.GetData();
			OpDesc.AxisCount = (uint32) Axes.Num();

			return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_REDUCE, &OpDesc} );
		}
	}
};


#define NNE_DML_REGISTER_REDUCE_OP(OpName, ReduceFunc, Version) \
TCHAR const Op##OpName##Version##Name[] = TEXT(#OpName); \
struct FDmlOperator##OpName##Version##Registrator \
{ \
	FDmlOperator##OpName##Version##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd({{TEXT(#OpName), TEXT("Onnx")}, Version}, FOperatorDmlReduce<ReduceFunc, Op##OpName##Version##Name>::Create, FOperatorDmlReduce<ReduceFunc, Op##OpName##Version##Name>::Validate); \
	} \
}; \
\
static FDmlOperator##OpName##Version##Registrator RegisterDmlOperator##OpName##Version;

#define NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(OpName, ReduceFunc) \
NNE_DML_REGISTER_REDUCE_OP(OpName, ReduceFunc, 1) \
NNE_DML_REGISTER_REDUCE_OP(OpName, ReduceFunc, 11) \
NNE_DML_REGISTER_REDUCE_OP(OpName, ReduceFunc, 13)

NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceL1,			DML_REDUCE_FUNCTION_L1)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceL2,			DML_REDUCE_FUNCTION_L2)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceLogSum,		DML_REDUCE_FUNCTION_LOG_SUM)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceLogSumExp,	DML_REDUCE_FUNCTION_LOG_SUM_EXP)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceMin,			DML_REDUCE_FUNCTION_MIN)
NNE_DML_REGISTER_REDUCE_OP(ReduceMin,						DML_REDUCE_FUNCTION_MIN, 12)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceMax,			DML_REDUCE_FUNCTION_MAX)
NNE_DML_REGISTER_REDUCE_OP(ReduceMax,						DML_REDUCE_FUNCTION_MAX, 12)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceMean,			DML_REDUCE_FUNCTION_AVERAGE)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceProd,			DML_REDUCE_FUNCTION_MULTIPLY)
NNE_DML_REGISTER_REDUCE_OP(ReduceSum,						DML_REDUCE_FUNCTION_SUM, 1)
NNE_DML_REGISTER_REDUCE_OP(ReduceSum,						DML_REDUCE_FUNCTION_SUM, 11)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ReduceSumSquare,	DML_REDUCE_FUNCTION_SUM_SQUARE)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ArgMax,				DML_REDUCE_FUNCTION_ARGMAX)
NNE_DML_REGISTER_REDUCE_OP(ArgMax,							DML_REDUCE_FUNCTION_ARGMAX, 12)
NNE_DML_REGISTER_REDUCE_OP_COMMON_VERSIONS(ArgMin,				DML_REDUCE_FUNCTION_ARGMIN)
NNE_DML_REGISTER_REDUCE_OP(ArgMin,							DML_REDUCE_FUNCTION_ARGMIN, 12)

#undef NNE_DML_REGISTER_REDUCE_OP



} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
