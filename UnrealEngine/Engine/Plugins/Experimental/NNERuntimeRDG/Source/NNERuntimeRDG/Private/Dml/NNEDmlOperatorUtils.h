// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

enum EAutoPad
{
    NOTSET,
    SAME_UPPER,
    SAME_LOWER,
    VALID
};

static EAutoPad AutoPadFromString(FStringView StringVal) 
{
    if (FCString::Stricmp(StringVal.GetData(), TEXT("NOTSET")) == 0) 
    {
        return EAutoPad::NOTSET;
    }
    else if (FCString::Stricmp(StringVal.GetData(), TEXT("SAME_UPPER")) == 0)
    {
        return EAutoPad::SAME_UPPER;
    }
    else if (FCString::Stricmp(StringVal.GetData(), TEXT("SAME_LOWER")) == 0)
    {
        return EAutoPad::SAME_LOWER;
    }
    else if (FCString::Stricmp(StringVal.GetData(), TEXT("VALID")) == 0)
    {
        return EAutoPad::VALID;
    }
    else
    {
        return EAutoPad::NOTSET;
    }
}

static bool CheckElementwiseTensor(ENNETensorDataType DataType, const NNE::FSymbolicTensorShape& TensorShape)
{
    if (DataType != ENNETensorDataType::Float)
    {
        UE_LOG(LogNNE, Warning, TEXT("Invalid DML tensor data type"));
        return false;
    }

    if(!TensorShape.IsConcrete())
    {
        UE_LOG(LogNNE, Warning, TEXT("DML tensor shape must be concrete"));
        return false;
    }

    if (NNE::FTensorShape::MakeFromSymbolic(TensorShape).Volume() == 0)
    {
        UE_LOG(LogNNE, Warning, TEXT("Invalid DML tensor size, it's 0"));
        return false;
    }

    const int32 MinTensorRank(0), MaxTensorRank(DML_TENSOR_DIMENSION_COUNT_MAX1);

    if (TensorShape.Rank() < MinTensorRank || TensorShape.Rank() > MaxTensorRank)
    {
        UE_LOG(LogNNE, Warning, TEXT("Invalid DML tensor rank: %d [%d,%d]"), TensorShape.Rank(), MinTensorRank, MaxTensorRank);
        return false;
    }

    return true;
}

static Util::FSmallUIntArray KernelPadding(
    TConstArrayView<uint32> InputShape, TConstArrayView<uint32> WindowSize, 
    TConstArrayView<uint32> Dilations, TConstArrayView<uint32> Strides
    )
{
    const uint32 NumSpatialDimensions = InputShape.Num() - NonspatialDimensionCount;
    check(NumSpatialDimensions >= 1);

    Util::FSmallUIntArray Padding;
    Padding.SetNumUninitialized(NumSpatialDimensions);

    for (uint32 Dim = 0; Dim < NumSpatialDimensions; ++Dim)
    {
        uint32 InputLen = uint32(InputShape[Dim + NonspatialDimensionCount]);
        uint32 StridedOutLen = (InputLen + Strides[Dim] - 1) / Strides[Dim];
        uint32 KernelLen = 1 + (WindowSize[Dim] - 1) * Dilations[Dim];
        uint32 Len = Strides[Dim] * (StridedOutLen - 1) + KernelLen;
        
        Padding[Dim] = (Len <= InputLen) ? 0 : (Len - InputLen);
    }

    return Padding;
}

static void ComputeStartEndPaddings(
    TConstArrayView<uint32> InputShape,
    const NNE::FAttributeMap& Attributes, 
    Util::FSmallUIntArray &OutStartPadding, 
	Util::FSmallUIntArray &OutEndPadding,
    TConstArrayView<uint32> Padding)
{
    const uint32 NumSpatialDimensions = InputShape.Num() - NonspatialDimensionCount;
    check(NumSpatialDimensions >= 1);

    EAutoPad AutoPad = AutoPadFromString(*Attributes.GetValue<FString>(TEXT("auto_pad")));

    if (AutoPad == EAutoPad::NOTSET)
    {
        Util::FSmallUIntArray Pads;

        Pads.Init(0u, 2 * NumSpatialDimensions);
        check(Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("pads")), Pads, TConstArrayView<uint32>(Pads)))


        for (uint32 Dim = 0; Dim < NumSpatialDimensions; ++Dim)
        {
            OutStartPadding.Add(Pads[Dim]);
            OutEndPadding.Add(Pads[Dim + NumSpatialDimensions]);
        }
    }
    else if (AutoPad == EAutoPad::VALID)
    {
        OutStartPadding.Init(0, NumSpatialDimensions);
        OutEndPadding.Init(0, NumSpatialDimensions);
    }
    else
    {
        OutStartPadding.Init(0, NumSpatialDimensions);
        OutEndPadding.Init(0, NumSpatialDimensions);

        for (uint32 Dim = 0; Dim < NumSpatialDimensions; ++Dim)
        {
            if (AutoPad == EAutoPad::SAME_LOWER)
            {
                OutStartPadding[Dim] = (Padding[Dim] + 1) / 2;
            }
            else
            {
                OutStartPadding[Dim] = Padding[Dim] / 2;
            }

            OutEndPadding[Dim] = Padding[Dim] - OutStartPadding[Dim];
        }
    }
}

inline int32 HandleNegativeAxis(int32 Axis, int32 Rank)
{
	if (Axis < 0)
	{
		Axis += Rank;
		check(Axis < Rank);
	}

	return Axis;
}

inline void HandleNegativeAxes(TArrayView<int32> Axes, int32 Rank)
{
	for (int32& Axis : Axes)
	{
		Axis = HandleNegativeAxis(Axis, Rank);
	}
}

inline int32 GetDmlAxis(int32 OnnxAxis, int32 OnnxDim, int32 DmlDim)
{
	check(DmlDim >= OnnxDim);
	OnnxAxis = HandleNegativeAxis(OnnxAxis, OnnxDim);
	uint32 DmlAxis = OnnxAxis + DmlDim - OnnxDim;

	return DmlAxis;
}

inline void SetDmlAxesFromOnnx(Util::FSmallUIntArray& DmlAxes, int32 Rank, TConstArrayView<int32> OnnxAxes)
{
	DmlAxes.Reset();
	DmlAxes.Reserve(Rank);

	for (int32 Axis : OnnxAxes)
	{
		DmlAxes.Add(GetDmlAxis(Axis, OnnxAxes.Num(), Rank));
	}
}

}