// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef NNE_USE_DIRECTML

#include "Containers/StringView.h"
#include "NNE.h"
#include "NNETypes.h"
#include "Dml/NNEDmlCommon.h"
#include "Dml/NNEDmlOperator.h"

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

template<typename T>
static bool IsEqualOrBroadcastable(TConstArrayView<T> ShapeA, TConstArrayView<T> ShapeB)
{
	if (ShapeA.Num() < ShapeB.Num())
	{
		return false;
	}

	int32 BIdx = 0;

	for (int32 Idx = 0; Idx < ShapeA.Num() && BIdx < ShapeB.Num(); ++Idx)
	{
		if (BIdx != 0)
		{
			if (ShapeA[Idx] != ShapeB[BIdx])
			{
				if (ShapeB[BIdx] != 1)
				{
					return false;
				}
			}
			++BIdx;
		}

		if (BIdx == 0 && ShapeA[Idx] == ShapeB[BIdx])
		{
			++BIdx;
		}
	}

	if (BIdx == 0)
	{
		return false;
	}

	return true;
}

static bool CheckGenericTensor(const FString& OpName, ENNETensorDataType DataType, const NNE::FSymbolicTensorShape& TensorShape, 
							   const TSet<ENNETensorDataType>& AllowedDataTypes,
							   const int32 MinTensorRank = 0, const int32 MaxTensorRank = GMaxTensorRank)
{
	if(!AllowedDataTypes.Contains(DataType))
	{
		UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid data type: %s"), *OpName, *UEnum::GetDisplayValueAsText(DataType).ToString());
		return false;
	}
	if (TensorShape.Rank() < MinTensorRank || TensorShape.Rank() > MaxTensorRank)
	{
		UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid tensor rank: %d [%d,%d]"), *OpName, TensorShape.Rank(), MinTensorRank, MaxTensorRank);
		return false;
	}

	return true;
}

static bool CheckGenericTensor1D(const FString& OpName, ENNETensorDataType DataType, const NNE::FSymbolicTensorShape& TensorShape, 
							   const TSet<ENNETensorDataType>& AllowedDataTypes)
{
	return CheckGenericTensor(OpName, DataType, TensorShape, AllowedDataTypes, 0, 1); // Scalar is "cast" to 1D due to lack of support in Dml.
}

static bool CheckElementwiseTensor(const FString& OpName, ENNETensorDataType DataType, const NNE::FSymbolicTensorShape& TensorShape,
								   TSet<ENNETensorDataType> AllowedDataTypes = {ENNETensorDataType::Float, ENNETensorDataType::Half})
{
	return CheckGenericTensor(OpName, DataType, TensorShape, AllowedDataTypes);
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

/**
* Utility class used by FKernelArgs 
*/
class FPaddingsHelper
{
	Util::FSmallUIntArray	Pads;
	EAutoPad				AutoPad;
	uint32					NumSpatialDimensions;

public:

	bool Init(const NNE::FAttributeMap& Attributes, uint32 InputRank)
	{
		NumSpatialDimensions = InputRank - NonspatialDimensionCount;
		check(NumSpatialDimensions >= 1);

		const FNNEAttributeValue* AttrAutoPad = Attributes.GetAttributeValue(TEXT("auto_pad"));

		if (AttrAutoPad)
		{
			AutoPad = AutoPadFromString(AttrAutoPad->GetValue<FString>());
		}
		else
		{
			AutoPad = EAutoPad::NOTSET;
		}

		if (AutoPad == EAutoPad::NOTSET)
		{
			Pads.Init(0u, 2 * NumSpatialDimensions);
			if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("pads")), Pads, TConstArrayView<uint32>(Pads)))
			{
				UE_LOG(LogNNE, Error, TEXT("Pads attribute cast led to overflow"));
				return false;
			}
		}
		
		return true;
	}

	bool Evaluate(Util::FSmallUIntArray& OutStartPadding, Util::FSmallUIntArray& OutEndPadding, TConstArrayView<uint32> Padding)
	{
		if (AutoPad == EAutoPad::NOTSET)
		{
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

		return true;
	}
};

/** 
* This is a base class that is used for Conv, ConvTranspose, Pool(both local and global) operators and MaxUnpool 
* NOTE: WindowSize needs to be set recomputed in the sub-class
*/
class FKernelArgs
{

public:

	uint32 GetNumDimensions() const
	{
		return NumDimensions;
	}

	TConstArrayView<uint32> GetStrides() const
	{
		check(!Strides.IsEmpty());
		return Strides;
	}

	TConstArrayView<uint32> GetDilations() const
	{
		check(!Dilations.IsEmpty());
		return Dilations;
	}

	TConstArrayView<uint32> GetStartPadding()
	{
		check(!StartPadding.IsEmpty());
		return StartPadding;
	}

	TConstArrayView<uint32> GetEndPadding()
	{
		check(!EndPadding.IsEmpty());
		return EndPadding;
	}

	TConstArrayView<uint32> GetOutputShape() const
	{
		check(!OutputShape.IsEmpty());
		return OutputShape;
	}

	bool GetCeilMode() const
	{
		return bCeilMode;
	}

private:

	FPaddingsHelper			Paddings;

protected:

	Util::FSmallUIntArray	StartPadding;
	Util::FSmallUIntArray	EndPadding;
	Util::FSmallUIntArray	OutPadding;
	Util::FSmallUIntArray	Dilations;
	Util::FSmallUIntArray	Strides;
	Util::FSmallUIntArray	InOutputShape;
	Util::FSmallUIntArray	WindowSize;
	Util::FSmallUIntArray	OutputShape;
	uint32					NumDimensions;
	bool					bIsGlobalKernel;
	bool					bIsTransposed;
	bool					bHasOutputShape;
	bool					bCeilMode;

protected:

	bool Init(const NNE::FAttributeMap& Attributes, int32 InputShapeRank, bool bInIsGlobalKernel, bool bInIsTransposed)
	{
		check(InputShapeRank > NonspatialDimensionCount);
		NumDimensions = InputShapeRank - NonspatialDimensionCount;
		bIsGlobalKernel = bInIsGlobalKernel;
		bIsTransposed = bInIsTransposed;
		bHasOutputShape = false;

		if (bIsGlobalKernel)
		{
			Strides.Init(1, NumDimensions);
			Dilations.Init(1, NumDimensions);
			StartPadding.Init(0, NumDimensions);
			EndPadding.Init(0, NumDimensions);
			OutPadding.Init(0, NumDimensions);
		}
		else
		{
			const FNNEAttributeValue* AttrStrides = Attributes.GetAttributeValue(TEXT("strides"));

			if (AttrStrides)
			{
				if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("strides")), Strides, TConstArrayView<uint32>(Strides)))
				{
					UE_LOG(LogNNE, Error, TEXT("Strides attribute cast led to overflow"));
					return false;
				}
			}
			else
			{
				Strides.Init(1, NumDimensions);
			}

			const FNNEAttributeValue* AttrDilations = Attributes.GetAttributeValue(TEXT("dilations"));

			if (AttrDilations)
			{
				if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("dilations")), Dilations, TConstArrayView<uint32>(Dilations)))
				{
					UE_LOG(LogNNE, Error, TEXT("Dilations attribute cast led to overflow"));
					return false;
				}
			}
			else
			{
				Dilations.Init(1, NumDimensions);
			}

			if (bIsTransposed)
			{
				const FNNEAttributeValue* AttrOutPadding = Attributes.GetAttributeValue(TEXT("output_padding"));

				if (AttrOutPadding)
				{
					if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("output_padding")), OutPadding, TConstArrayView<uint32>(OutPadding)))
					{
						UE_LOG(LogNNE, Error, TEXT("output_padding attribute cast led to overflow"));
						return false;
					}
				}			
				else
				{
					OutPadding.Init(0, NumDimensions);
				}

				const FNNEAttributeValue* AttrOutShape = Attributes.GetAttributeValue(TEXT("output_shape"));

				if (AttrOutShape)
				{
					if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("output_shape")), InOutputShape, TConstArrayView<uint32>(InOutputShape)))
					{
						UE_LOG(LogNNE, Error, TEXT("output_shape attribute cast led to overflow"));
						return false;
					}
				}

				bHasOutputShape = AttrOutShape != nullptr;
			}
			else
			{
				OutPadding.Init(0, NumDimensions);
			}

			if (!Paddings.Init(Attributes, InputShapeRank))
			{
				return false;
			}

			bCeilMode = (bool) Attributes.GetValueOrDefault<int32>(TEXT("ceil_mode"), 0);
		}

		return true;
	}

public:

	void Evaluate(TConstArrayView<uint32> InputShape, TConstArrayView<uint32> PaddingsValue = TConstArrayView<uint32>())
	{
		if (bIsGlobalKernel && WindowSize.IsEmpty())
		{
			for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
			{
				WindowSize.Add(uint32(InputShape[InputShape.Num() - NumDimensions + Dim]));
			}
		}

		check(!WindowSize.IsEmpty());

		if (!bIsGlobalKernel)
		{
			Paddings.Evaluate(StartPadding, EndPadding, PaddingsValue);
		}

		check(uint32(InputShape.Num()) >= NumDimensions);
		const uint32 DimOffset = InputShape.Num() - NumDimensions;
		
		OutputShape.Reset(InputShape.Num());
		
		if (!bIsTransposed)
		{
			OutputShape.Append(InputShape.GetData(), InputShape.Num());

			for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
			{
				uint32 InputLen = InputShape.GetData()[Dim + DimOffset];
				uint32 PaddedLen = InputLen + StartPadding[Dim] + EndPadding[Dim];
				uint32 KernelLen = 1 + (WindowSize[Dim] - 1) * Dilations[Dim];

				checkf(KernelLen <= PaddedLen, TEXT("KernelLen must < PaddedLen"));
				checkf(Strides[Dim] != 0, TEXT("Strides must be != 0"));

				uint32 StridableOutLen = PaddedLen - KernelLen;
				float OutLen = 1.0f + ((float) StridableOutLen / (float) Strides[Dim]);

				float(*const RoundingFunction)(float) = 
							bCeilMode?
								(float(*)(float)) &FMath::CeilToFloat
							:
								(float(*)(float)) &FMath::FloorToFloat
							;
				OutputShape[Dim + DimOffset] = (uint32) RoundingFunction(OutLen);
			}
		}
		else
		{
			if (bHasOutputShape)
			{
				OutputShape.Append(InOutputShape.GetData(), InOutputShape.Num());
			}
			else
			{
				OutputShape.Append(InputShape.GetData(), InputShape.Num());

				for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
				{
					uint32 Padding = StartPadding[Dim] + EndPadding[Dim];
					uint32 KernelLen = 1 + (WindowSize[Dim] - 1) * Dilations[Dim];

					OutputShape[Dim + DimOffset] = (InputShape[Dim + DimOffset] - 1) * Strides[Dim] + KernelLen + OutPadding[Dim] - Padding;
				}
			}
		}
	}
};

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

#endif // NNE_USE_DIRECTML
