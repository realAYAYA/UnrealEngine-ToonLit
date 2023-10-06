// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "Misc/EnumerateRange.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"

#include <numeric>

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Slice
 */
class FOperatorDmlSlice : public FOperatorDml
{	
	template<typename DataType>
	void ComputeOffsetsSizesStrides(
		TArrayView<const NNE::Internal::FTensor> InputTensors,
		Util::FSmallUIntArray& OutOffsets, 
		Util::FSmallUIntArray& OutSizes, 
		Util::FSmallIntArray& OutStrides)
	{
		TConstArrayView<DataType> Starts =  InputTensors[1].GetPreparedData<DataType>();
		TConstArrayView<DataType> Ends = InputTensors[2].GetPreparedData<DataType>();
		check(Starts.Num() == Ends.Num());

		Util::FSmallArray<DataType> Axes;
		Util::FSmallArray<DataType> Steps;

		if (InputTensors.Num() >= 4)
		{
			auto NormalizeAxes = [NumDims = (DataType) InputTensors[0].GetShape().Rank()] (DataType& Axis)
			{
				if(Axis < 0)
				{
					Axis += NumDims;
				}
			};
			Axes = InputTensors[3].GetPreparedData<DataType>();
			check(Axes.Num() == Starts.Num());
			Algo::ForEach(Axes, NormalizeAxes);
		}
		else
		{
			Axes.SetNumUninitialized(Starts.Num());
			std::iota(Axes.begin(), Axes.end(), 0);
		}

		if (InputTensors.Num() >= 5)
		{
			Steps = InputTensors[4].GetPreparedData<DataType>();
			check(Steps.Num() == Axes.Num());
		}
		else
		{
			Steps.Init((DataType)1, Axes.Num());
		}

		Util::FSmallUIntArray OutputShape;
		Algo::Copy(InputTensors[0].GetShape().GetData(), OutputShape);
		OutSizes = OutputShape;
		OutOffsets.SetNumZeroed(OutputShape.Num());

		OutStrides.Init(1, OutputShape.Num());

		for (TConstEnumerateRef<DataType> Elem : EnumerateRange(Starts))
		{
			int32 Idx = Elem.GetIndex();
			DataType Start = *Elem;
			DataType End = Ends[Idx];
			
			DataType DimIndex = Axes[Idx];
			check(DimIndex < (DataType) InputTensors[0].GetShape().Rank());
			DataType Stride = Steps[Idx];
			check(Stride != 0);

			uint32 Dim = InputTensors[0].GetShape().GetData()[DimIndex];
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
                std::swap(Start, End);
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

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSlice();
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
		check(InputTensors.Num() >= 3);
		check(InputTensors.Num() <= 5);
		check(OutputTensors.Num() == 1);

		ENNETensorDataType InputIndexDataType = ENNETensorDataType::None;

		for(int Idx = 1; Idx < InputTensors.Num(); Idx++)
		{
			check(InputTensors[Idx].GetShape().Rank() == 1);
			if (Idx == 1)
			{
				InputIndexDataType = InputTensors[Idx].GetDataType();
				check( InputIndexDataType == ENNETensorDataType::Int32 
			   		|| InputIndexDataType == ENNETensorDataType::Int64);
			}
			else
			{
				check(InputTensors[Idx].GetDataType() == InputIndexDataType);
			}
			check(InputTensors[Idx].HasPreparedData());
			ConstantCPUInputs.Add(Idx);
		}

		// Initialize Input tensor desc
        FTensorDescDml DmlInputTensorDesc;
        
		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensors[0])
				.Validate())
        {
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice input for DML inference"));
            return false;
        }

		// Initialize Output tensor desc
        FTensorDescDml DmlOutputTensorDesc;
        
		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensors[0])
				.Validate())
        {
            UE_LOG(LogNNE, Error, TEXT("Failed to initialize Slice Output for DML inference"));
            return false;
        }

		Util::FSmallUIntArray Offsets, Sizes;
		Util::FSmallIntArray Strides;

		switch(InputIndexDataType)
		{
		case ENNETensorDataType::Int32:
			ComputeOffsetsSizesStrides<int32>(InputTensors, Offsets, Sizes, Strides);
			break;
		case ENNETensorDataType::Int64:
			ComputeOffsetsSizesStrides<int64>(InputTensors, Offsets, Sizes, Strides);
			break;
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
NNE_DML_REGISTER_OP(Slice)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
