// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
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
template<DML_REDUCE_FUNCTION ReduceFunc>
class FOperatorDmlReduce : public FOperatorDml
{

	inline void HandleEmptyAxes(TArray<int32>& Axes, int32 Rank)
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

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlReduce<ReduceFunc>();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		checkf(InputTensors.Num() == 1, TEXT("Dml Reduce op supports only 1 input"));
		check(OutputTensors.Num() == 1);

		const int32 KeepDims = Attributes.GetValueOrDefault<int32>(TEXT("keepdims"), 1);
		
		TArray<int32>	Axes;
		
		const FNNEAttributeValue* AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));
		if (AxesAttr)
		{
			Axes = AxesAttr->GetValue<TArray<int32>>();
		}

		HandleNegativeAxes(Axes, InputTensors[0].GetShape().Rank());
		HandleEmptyAxes(Axes, InputTensors[0].GetShape().Rank());

		DmlUtil::FTensorDesc DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc.InitFromTensor(InputTensors[0], InputTensors[0].GetShape().Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce input tensor for DML inference"));
			return false;
		}

		DmlUtil::FSmallUIntArray	DmlAxes;
		DmlUtil::FSmallUIntArray	ReducedDims;

		ReducedDims.SetNum(DmlInputTensorDesc.Sizes.Num());

		for (int32& Dim : Axes)
		{
			checkf(Dim < ReducedDims.Num(), TEXT("Index out of bounds for Reduce axis"));
			ReducedDims[Dim] = 1;
			DmlAxes.Add(Dim);
		}

		DmlUtil::FSmallUIntArray	OutputShape;

		if (!KeepDims)
		{
			// ReduceSum example:
			//     input dims: {3, 2, 2}
			//     axes: 1
			//     keepDims: 0
			// 
			// ONNX: {3, 2}, but DML: {3, 1, 2}	
			TConstArrayView<uint32> InputShape = InputTensors[0].GetShape().GetData();

			OutputShape.SetNum(ReducedDims.Num());

			for (int32 Idx = 0; Idx < OutputShape.Num(); ++Idx)
			{
				OutputShape[Idx] = ReducedDims[Idx] ? 1 : InputShape[Idx];
			}
		}
		else
		{
			OutputShape.Append(OutputTensors[0].GetShape().GetData());
		}

		DmlUtil::FTensorDesc DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc.InitFromTensor(OutputTensors[0], OutputShape.Num(), 
			/*Broadcast shape*/ MakeEmptyArrayView<uint32>(),
			/*Custom shape*/ OutputShape))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce output tensor for DML inference"));
			return false;
		}

		DML_REDUCE_OPERATOR_DESC OpDesc{};

		OpDesc.InputTensor = &DmlInputTensorDesc.Desc;
		OpDesc.OutputTensor = &DmlOutputTensorDesc.Desc;
		OpDesc.Function = ReduceFunc;
		OpDesc.Axes = DmlAxes.GetData();
		OpDesc.AxisCount = (uint32) DmlAxes.Num();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_REDUCE, &OpDesc} );
	}
};

// Register Reshape operator on Module startup
#define OP(OpName, ReduceFunc) FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), FOperatorDmlReduce<ReduceFunc>::Create)

struct FOperatorDmlReduceRegistrator
{
	FOperatorDmlReduceRegistrator()
	{
		OP(ReduceL1,		DML_REDUCE_FUNCTION_L1);
		OP(ReduceL2,		DML_REDUCE_FUNCTION_L2);
		OP(ReduceLogSum,	DML_REDUCE_FUNCTION_LOG_SUM);
		OP(ReduceLogSumExp,	DML_REDUCE_FUNCTION_LOG_SUM_EXP);
		OP(ReduceMin,		DML_REDUCE_FUNCTION_MIN);
		OP(ReduceMax,		DML_REDUCE_FUNCTION_MAX);
		OP(ReduceMean,		DML_REDUCE_FUNCTION_AVERAGE);
		OP(ReduceProd,		DML_REDUCE_FUNCTION_MULTIPLY);
		OP(ReduceSum,		DML_REDUCE_FUNCTION_SUM);
		OP(ReduceSumSquare,	DML_REDUCE_FUNCTION_SUM_SQUARE);
	}
};

#undef OP

static FOperatorDmlReduceRegistrator RegisterReduceOperators;


} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
