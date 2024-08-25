// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "ID3D12DynamicRHI.h"

#include "Algo/Compare.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

namespace Util
{

inline bool IsSameShape(TConstArrayView<uint32> Left, TConstArrayView<uint32> Right)
{
	return Algo::Compare(Left, Right);
}

inline bool IsSameShape(const FTensorDescDml& Left, const FTensorDescDml& Right)
{
	return IsSameShape(Left.GetSizes(), Right.GetSizes());
}

DML_TENSOR_DATA_TYPE GetTensorDataType(ENNETensorDataType DataType)
{
	switch (DataType)
	{
	case ENNETensorDataType::Double:
		return DML_TENSOR_DATA_TYPE_FLOAT64;

	case ENNETensorDataType::Float:
		return DML_TENSOR_DATA_TYPE_FLOAT32;

	case ENNETensorDataType::Half:
		return DML_TENSOR_DATA_TYPE_FLOAT16;

	case ENNETensorDataType::UInt64:
		return DML_TENSOR_DATA_TYPE_UINT64;

	case ENNETensorDataType::UInt32:
		return DML_TENSOR_DATA_TYPE_UINT32;

	case ENNETensorDataType::UInt16:
		return DML_TENSOR_DATA_TYPE_UINT16;

	case ENNETensorDataType::UInt8:
		return DML_TENSOR_DATA_TYPE_UINT8;

	case ENNETensorDataType::Int64:
		return DML_TENSOR_DATA_TYPE_INT64;

	case ENNETensorDataType::Int32:
		return DML_TENSOR_DATA_TYPE_INT32;

	case ENNETensorDataType::Int16:
		return DML_TENSOR_DATA_TYPE_INT16;

	case ENNETensorDataType::Int8:
		return DML_TENSOR_DATA_TYPE_INT8;

	default:
		return DML_TENSOR_DATA_TYPE_UNKNOWN;
	}
}

} // namespace Util 

FTensorDescDml::FTensorDescDml()
	: MinTensorRank(1)
	, MaxTensorRank(DML_TENSOR_DIMENSION_COUNT_MAX1)
	, bIsValidated(false)
{
	Sizes.Reset();
	Strides.Reset();
	BuffDesc = DML_BUFFER_TENSOR_DESC{};
	Desc = DML_TENSOR_DESC{};
}

FTensorDescDml& FTensorDescDml::SetTensorRank(int32 MinRank, int32 MaxRank)
{
	MinTensorRank = MinRank;
	MaxTensorRank = MaxRank;
	return *this;
}

FTensorDescDml& FTensorDescDml::SetShape(TConstArrayView<uint32> Shape)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		Sizes.Reset();
		
		if (Shape.Num() > 0)
		{
			Sizes.Append(Shape.GetData(), Shape.Num());
		}
		// Handle scalar tensors
		else if (Shape.Num() == 0)
		{
			Sizes.Add(1);
		}
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetShape(TConstArrayView<uint32> Shape, int32 Rank)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		Sizes.Reset();
		
		if (Shape.Num() > 0)
		{
			const int32 Offset = Rank - Shape.Num();

			for (int32 Idx = 0; Idx < Offset; ++Idx)
			{
				Sizes.Add(1);
			}

			Sizes.Append(Shape.GetData(), Shape.Num());
		}
		// Handle scalar tensors
		else if (Shape.Num() == 0)
		{
			for (int32 Idx = 0; Idx < Rank; ++Idx)
			{
				Sizes.Add(1);
			}
		}
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetShape(TConstArrayView<uint32> Shape, TConstArrayView<uint32> BroadcastShape)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		const uint32 TargetDimension = BroadcastShape.Num() ? BroadcastShape.Num() : Shape.Num();
		checkf(BroadcastShape.Num() >= Shape.Num(), TEXT("Can't broadcast tensor from rank %d to rank %d, should be inferior or equal."), Shape.Num(), TargetDimension);

		Sizes.SetNum(TargetDimension);
		Strides.SetNum(TargetDimension);

		const int32 DimensionOffset = int32(TargetDimension - Shape.Num());

		for (int32 i = 0; i < (int32)TargetDimension; ++i)
		{
			Sizes[i] = i < DimensionOffset ? 1 : Shape[i - DimensionOffset];
		}

		uint32 CurrStride = 1;

		for (int32 i = TargetDimension - 1; i >= 0; --i)
		{
			const bool bBroadcast = Sizes[i] < BroadcastShape[i];

			Strides[i] = bBroadcast ? 0 : CurrStride;
			CurrStride *= Sizes[i];

			Sizes[i] = BroadcastShape[i];
		}
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetShape1D(uint32 Dimension, int32 Rank)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		Sizes.Reset();

		// Handle scalar tensors
		if (Dimension == 0)
		{
			Dimension = 1;
		}

		Sizes.Add(1);
		Sizes.Add(Dimension);
		Sizes.Add(1);

		for (int32 Dim = 3; Dim < Rank; ++Dim)
		{
			Sizes.Add(1);
		}
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetStridesFromShape(TConstArrayView<uint32> Shape)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		uint32 CurrStride = 1;
		Strides.SetNum(Shape.Num());

		for (int32 i = Shape.Num() - 1; i >= 0; --i)
		{
			Strides[i] = CurrStride;
			CurrStride *= Shape[i];
		}
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetStrides(TConstArrayView<uint32> InStrides)
{
	check(!bIsValidated);

	if (!bIsValidated)
	{
		Strides = InStrides;
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetDataType(ENNETensorDataType DataType)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		DML_TENSOR_DATA_TYPE DmlDataType = Util::GetTensorDataType(DataType);
		check(DmlDataType != DML_TENSOR_DATA_TYPE_UNKNOWN);
	
		uint64 ElemByteSize = UE::NNE::GetTensorDataTypeSizeInBytes(DataType);
		check(ElemByteSize > 0)

		BuffDesc.DataType = DmlDataType;
		BuffDesc.TotalTensorSizeInBytes = CalculateBufferSize(ElemByteSize);
	}

	return *this;
}

FTensorDescDml& FTensorDescDml::SetDataOwnedByDml(bool bSetOwnedByDml)
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		BuffDesc.Flags = bSetOwnedByDml ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
	}

	return *this;
}

bool FTensorDescDml::Validate()
{
	check(!bIsValidated);
	
	if (!bIsValidated)
	{
		if (BuffDesc.DataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid DML tensor data type"));
			return false;
		}

		if (BuffDesc.TotalTensorSizeInBytes == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid DML tensor size, it's 0"));
			return false;
		}

		// Match minimum tensor rank
		const int32 DimOffset = MinTensorRank - Sizes.Num();

		for (int Dim = 0; Dim < DimOffset; ++Dim)
		{
			Sizes.Insert(1, 0);
		}

		const int32 Rank = Sizes.Num();

		if (Rank < MinTensorRank || Rank > MaxTensorRank)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid DML tensor rank:%d [%d,%d]"), Rank, MinTensorRank, MaxTensorRank);
			return false;
		}

		if (!Strides.IsEmpty() && Strides.Num() != Rank)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid DML tensor stride rank:%d [size:%d]"), Rank, Strides.Num());
			return false;
		}

		BuffDesc.DimensionCount = Sizes.Num();
		BuffDesc.Sizes = Sizes.GetData();
		BuffDesc.Strides = Strides.IsEmpty() ? nullptr : Strides.GetData();
	
		Desc.Type = DML_TENSOR_TYPE_BUFFER;
		Desc.Desc = &BuffDesc;

		bIsValidated = true;
	}

	return bIsValidated;
}

uint64 FTensorDescDml::CalculateBufferSize(uint64 ElemSizeInBytes)
{
	if (ElemSizeInBytes == 0)
	{
		return 0;
	}

	uint64 MinSizeInBytes = 0;
		
	if (Strides.IsEmpty())
	{
		MinSizeInBytes = Sizes[0];

		for (int32 i = 1; i < Sizes.Num(); ++i)
		{
			MinSizeInBytes *= Sizes[i];
		}

		MinSizeInBytes *= ElemSizeInBytes;
	}
	else
	{
		uint32 IndexOfLastElement = 0;

		for (int32 i = 0; i < Sizes.Num(); ++i)
		{
			IndexOfLastElement += (Sizes[i] - 1) * Strides[i];
		}

		MinSizeInBytes = (static_cast<uint64>(IndexOfLastElement) + 1) * ElemSizeInBytes;
	}

	// Round up to the nearest 4 bytes
	MinSizeInBytes = Util::AlignBufferSize(MinSizeInBytes);

	return MinSizeInBytes;
}

TConstArrayView<int32> FOperatorDml::GetConstantCPUInputs() const
{
	return ConstantCPUInputs;
}

TConstArrayView<int32> FOperatorDml::GetRemappedInputs() const
{
	return RemappedInputs;
}

IDMLOperator* FOperatorDml::GetOperator()
{
	return DmlOp;
}

// Create DirectML operator	
bool FOperatorDml::CreateOperator(IDMLDevice* Device, const DML_OPERATOR_DESC& DmlOpDesc)
{
	IDMLOperator* Op = nullptr;

	HRESULT Res;

	Res = Device->CreateOperator(&DmlOpDesc, DML_PPV_ARGS(&Op));
	if (!Op)
	{
		UE_LOG(LogNNE, Warning, TEXT("Error:Failed to create DML operator, hres:%d"), Res);
		return false;
	}

	DmlOp = Op;

	return DmlOp.IsValid();
}

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
