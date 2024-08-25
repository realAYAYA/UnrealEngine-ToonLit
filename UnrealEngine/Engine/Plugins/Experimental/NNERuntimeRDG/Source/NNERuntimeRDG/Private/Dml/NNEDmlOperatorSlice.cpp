// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"
#include "Misc/EnumerateRange.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"

#include <numeric>

namespace UE::NNERuntimeRDG::Private::Dml
{

namespace Util
{

/**
* Implement std::iota() replacement for TArray
*/
template<typename ArrayT, class ValueT>
void Iota(ArrayT& Array, ValueT Value)
{
	auto First = Array.begin();
	auto Last = Array.end();

	for (auto It = First; It != Last; ++It)
	{
		(*It) = Value;
		++Value;
	}
}

} // Util

class FOperatorDmlSlice : public FOperatorDml
{	
	template<typename DataType>
	static void ComputeOffsetsSizesStrides(
		TArrayView<const NNE::Internal::FTensorRef> InputTensors,
		Util::FSmallUIntArray& OutOffsets, 
		Util::FSmallUIntArray& OutSizes, 
		Util::FSmallIntArray& OutStrides,
		Util::FSmallUIntArray& OutputShape)
	{
		TConstArrayView<DataType> Starts =  InputTensors[1]->GetPreparedData<DataType>();
		TConstArrayView<DataType> Ends = InputTensors[2]->GetPreparedData<DataType>();
		check(Starts.Num() == Ends.Num());

		Util::FSmallArray<DataType> Axes;
		Util::FSmallArray<DataType> Steps;

		if (InputTensors.Num() >= 4)
		{
			auto NormalizeAxes = [NumDims = (DataType) InputTensors[0]->GetShape().Rank()] (DataType& Axis)
			{
				if(Axis < 0)
				{
					Axis += NumDims;
				}
			};
			Axes = InputTensors[3]->GetPreparedData<DataType>();
			check(Axes.Num() == Starts.Num());
			Algo::ForEach(Axes, NormalizeAxes);
		}
		else
		{
			Axes.SetNumUninitialized(Starts.Num());
			Util::Iota(Axes, 0);
		}

		if (InputTensors.Num() >= 5)
		{
			Steps = InputTensors[4]->GetPreparedData<DataType>();
			check(Steps.Num() == Axes.Num());
		}
		else
		{
			Steps.Init((DataType) 1, Axes.Num());
		}

		OutputShape.Reset();
		Algo::Copy(InputTensors[0]->GetShape().GetData(), OutputShape);
		OutSizes = OutputShape;
		OutOffsets.SetNumZeroed(OutputShape.Num());

		OutStrides.Init(1, OutputShape.Num());

		for (TConstEnumerateRef<DataType> Elem : EnumerateRange(Starts))
		{
			int32 Idx = Elem.GetIndex();
			DataType Start = *Elem;
			DataType End = Ends[Idx];
			
			DataType DimIndex = Axes[Idx];
			check(DimIndex < (DataType) InputTensors[0]->GetShape().Rank());
			DataType Stride = Steps[Idx];
			check(Stride != 0);

			uint32 Dim = InputTensors[0]->GetShape().GetData()[DimIndex];
			if (Start < 0 && Start > TNumericLimits<DataType>::Min())
			{
				Start += (DataType) Dim;
			}
			
			if (End < 0 && Start > TNumericLimits<DataType>::Min())
			{
				End += (DataType) Dim;
			}

			if (Stride < 0)
            {
                Swap(Start, End);
                Start += (Start < TNumericLimits<DataType>::Max()) ? 1 : 0;
                End += (End < TNumericLimits<DataType>::Max()) ? 1 : 0;
            }

			Start = FMath::Max(Start, 0);
            End = FMath::Min(End, (DataType) Dim);
            DataType Size = FMath::Max(End - Start, 0);
			
			DataType AbsStride = FMath::Abs(Stride);
            OutputShape[DimIndex] = (uint32) ((Size / AbsStride) + (Size % AbsStride != 0));
            OutOffsets[DimIndex] = (uint32) Start;
            OutStrides[DimIndex] = (int32) Stride;
            OutSizes[DimIndex] = (uint32) Size;
		}
	}

	mutable Util::FSmallUIntArray	OutputShape;
	mutable Util::FSmallUIntArray	Offsets, Sizes;
	mutable Util::FSmallIntArray	Strides;

	ENNETensorDataType InputIndexDataType;

	static constexpr uint32 MinAllowedInputTensors = 3, MaxAllowedInputTensors = 5, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSlice();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Slice");

		if (InputShapes.Num() < MinAllowedInputTensors || InputShapes.Num() > MaxAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be in [%d, %d]."), 
										*OpName, InputShapes.Num(), MinAllowedInputTensors, MaxAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Double, ENNETensorDataType::Float, ENNETensorDataType::Half, 
				ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
				ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, 
				ENNETensorDataType::UInt16, ENNETensorDataType::UInt8
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}
		
		ENNETensorDataType IndexDataTypeToCheck = ENNETensorDataType::None;

		for (int Idx = 1; Idx < InputShapes.Num(); Idx++)
		{
			if (!CheckGenericTensor1D(OpName, InputTypes[Idx], InputShapes[Idx], 
				{ 	ENNETensorDataType::Int64, ENNETensorDataType::Int32
				}
				))
			{
				return false;
			}
			
			if (Idx == 1)
			{
				IndexDataTypeToCheck = InputTypes[Idx];
			}
			else
			{
				if (InputTypes[Idx] != IndexDataTypeToCheck)
				{
					UE_LOG(LogNNE, Warning, TEXT("DML %s: data type of tensor at position %d differs from data type of 'starts' tensor."), 
										*OpName, Idx);
					return false;
				}
			}

		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() >= MinAllowedInputTensors);
		check(Inputs.Num() <= MaxAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		InputIndexDataType = ENNETensorDataType::None;

		for (int Idx = 1; Idx < Inputs.Num(); Idx++)
		{
			check(Inputs[Idx].GetShape().Rank() == 1);
			
			if (Idx == 1)
			{
				InputIndexDataType = Inputs[Idx].GetDataType();
				if (InputIndexDataType != ENNETensorDataType::Int32 && InputIndexDataType != ENNETensorDataType::Int64)
				{
					UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice input tensor at index 1, data type needs to be Int32 or Int64"));
					return false;
				}
			}
			else
			{
				if (Inputs[Idx].GetDataType() != InputIndexDataType)
				{
					UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice input tensor at index %d, data type needs to same as at index 1"), Idx);
					return false;
				}
			}

			ConstantCPUInputs.Add(Idx);
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		switch (InputIndexDataType)
		{
			case ENNETensorDataType::Int32:
				ComputeOffsetsSizesStrides<int32>(InputTensors, Offsets, Sizes, Strides, OutputShape);
				break;

			case ENNETensorDataType::Int64:
				ComputeOffsetsSizesStrides<int64>(InputTensors, Offsets, Sizes, Strides, OutputShape);
				break;
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
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice input for DML inference"));
            return false;
        }

        FTensorDescDml DmlOutputTensorDesc;
        
		if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
        {
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice Output for DML inference"));
            return false;
        }

		DML_SLICE1_OPERATOR_DESC DmlSliceOpDesc{};
		
		DmlSliceOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
        DmlSliceOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
        DmlSliceOpDesc.DimensionCount = (uint32) Offsets.Num();
        DmlSliceOpDesc.InputWindowOffsets = Offsets.GetData();
        DmlSliceOpDesc.InputWindowSizes = Sizes.GetData();
        DmlSliceOpDesc.InputWindowStrides = Strides.GetData();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_SLICE1, &DmlSliceOpDesc} );

	}
};

// Register Slice operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Slice, 10)
NNE_DML_REGISTER_OP_VERSION(Slice, 11)
NNE_DML_REGISTER_OP_VERSION(Slice, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
