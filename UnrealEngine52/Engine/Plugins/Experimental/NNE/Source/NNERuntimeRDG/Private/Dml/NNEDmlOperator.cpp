// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "ID3D12DynamicRHI.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

namespace DmlUtil
{

	bool FTensorDesc::InitFromTensor(const NNECore::Internal::FTensor& Tensor, int32 MinTensorRank, TConstArrayView<uint32> BroadcastShape, TConstArrayView<uint32> CustomShape)
	{
		Reset();

		DML_TENSOR_DATA_TYPE DmlDataType = DmlUtil::GetTensorDataType(Tensor.GetDataType());

		if (DmlDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
		{
			return false;
		}

		TConstArrayView<uint32> InShape = CustomShape.IsEmpty() ? Tensor.GetShape().GetData() : CustomShape;

		ElemSizeInBytes = Tensor.GetElemByteSize();

		if (BroadcastShape.IsEmpty())
		{
			SetShape(InShape, MinTensorRank);
		}
		else
		{
			SetShapeAndStrides(InShape, BroadcastShape);
		}

		//Note: We should support tensor padding using strides defined in FTensorDesc
		//DmlUtil::SetTensorStrides(DmlTensorDesc, TensorDesc.Strides);

		Update(DmlDataType, Tensor.HasPreparedData());

		return true;
	}

	bool FTensorDesc::InitFromTensor1D(const NNECore::Internal::FTensor& Tensor, int32 Rank)
	{
		Reset();

		if (Tensor.GetShape().Rank() != 1)
		{
			return false;
		}

		DML_TENSOR_DATA_TYPE DmlDataType = DmlUtil::GetTensorDataType(Tensor.GetDataType());

		if (DmlDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
		{
			return false;
		}
		
		ElemSizeInBytes = Tensor.GetElemByteSize();

		SetShape1D(Tensor.GetShape().GetData()[0], Rank);
		Update(DmlDataType, Tensor.HasPreparedData());

		return true;
	}

	void FTensorDesc::UpdateShapeAndStrides(TConstArrayView<uint32> InShape, TConstArrayView<uint32> InStrides)
	{
		check(!InShape.IsEmpty());

		Sizes = InShape;
		BuffDesc.Sizes = Sizes.GetData();
		BuffDesc.DimensionCount = Sizes.Num();

		if (!InStrides.IsEmpty())
		{
			check(InStrides.Num() == InShape.Num());
			Strides = InStrides;
			BuffDesc.Strides = Strides.GetData();
		}
		else
		{
			Strides.Reset();
			BuffDesc.Strides = nullptr;
		}
	}

	void FTensorDesc::SetStridesFromTensor(const NNECore::Internal::FTensor& InputDesc)
	{
		uint32 CurrStride = 1;

		Strides.SetNum(InputDesc.GetShape().Rank());
		
		for (int32 i = InputDesc.GetShape().Rank() - 1; i >= 0; --i)
		{
			Strides[i] = CurrStride;
			CurrStride *= InputDesc.GetShape().GetData()[i];
		}
	}

	void FTensorDesc::Reset()
	{
		BuffDesc = DML_BUFFER_TENSOR_DESC{};
		Desc = DML_TENSOR_DESC{};
		Sizes.Reset();
		Strides.Reset();
		ElemSizeInBytes = 0;
	}

	void FTensorDesc::SetShape(TConstArrayView<uint32> Shape, int32 MinTensorRank)
	{
		Sizes = FSmallUIntArray(Shape.GetData(), Shape.Num());
		const int32 DimOffset = MinTensorRank - Sizes.Num();

		for (int Dim = 0; Dim < DimOffset; ++Dim)
		{
			Sizes.Insert(1, 0);
		}

	}

	void FTensorDesc::SetShapeAndStrides(TConstArrayView<uint32> Shape, TConstArrayView<uint32> BroadcastShape)
	{
		const uint32 TargetDimension = BroadcastShape.Num() != -1 ? BroadcastShape.Num() : Shape.Num();
		checkf(BroadcastShape.Num() >= Shape.Num(), TEXT("Can't broadcast tensor from rank %d to rank %d, should be inferior or equal."), Shape.Num(), TargetDimension);
		
		Sizes.SetNum(TargetDimension);
		Strides.SetNum(TargetDimension);

		const int32 DimensionOffset = int32(TargetDimension - Shape.Num());
		
		for (int32 i = 0; i < (int32) TargetDimension; ++i)
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

	void FTensorDesc::SetShape1D(uint32 Dimension, int32 Rank)
	{
		Sizes.Reset();

		Sizes.Add(1);
		Sizes.Add(Dimension);
		Sizes.Add(1);

		for (int32 Dim = 3; Dim < Rank; ++Dim)
		{
			Sizes.Add(1);
		}
	}

	void FTensorDesc::Update(DML_TENSOR_DATA_TYPE DataType, bool bHasWeightData)
	{
		BuffDesc.DataType = DataType;
		BuffDesc.Flags = bHasWeightData ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		BuffDesc.DimensionCount = Sizes.Num();
		BuffDesc.Sizes = Sizes.GetData();
		BuffDesc.Strides = Strides.IsEmpty() ? nullptr : Strides.GetData();
		BuffDesc.TotalTensorSizeInBytes = CalculateBufferSize();

		Desc = DML_TENSOR_DESC{ DML_TENSOR_TYPE_BUFFER, &BuffDesc };
	}

	uint64 FTensorDesc::CalculateBufferSize()
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
		MinSizeInBytes = (MinSizeInBytes + 3) & ~3ull;

		return MinSizeInBytes;
	}



	void SetTensorStrides(FTensorDesc& TensorDesc, const NNECore::Internal::FTensor& InputDesc)
	{
		uint32 CurrStride = 1;

		TensorDesc.Strides.SetNum(InputDesc.GetShape().Rank());
		
		for (int32 i = InputDesc.GetShape().Rank() - 1; i >= 0; --i)
		{
			TensorDesc.Strides[i] = CurrStride;
			CurrStride *= InputDesc.GetShape().GetData()[i];
		}
	}

	void SetTensorSizesAndStridesForBroadcast(FTensorDesc& TensorDesc, const NNECore::Internal::FTensor& InputDesc, const NNECore::Internal::FTensor& TargetDesc)
	{
		static_assert(NNECore::FTensorShape::MaxRank <= 8);
		
		const uint32 TargetDimension = TargetDesc.GetShape().Rank() != -1 ? TargetDesc.GetShape().Rank() : InputDesc.GetShape().Rank();
		checkf(TargetDesc.GetShape().Rank() >= InputDesc.GetShape().Rank(), TEXT("Can't broadcast tensor from rank %d to rank %d, should be inferior or equal."), InputDesc.GetShape().Rank(), TargetDimension);
		
		TensorDesc.Sizes.SetNum(TargetDimension);
		TensorDesc.Strides.SetNum(TargetDimension);

		const int32 DimensionOffset = int32(TargetDimension - InputDesc.GetShape().Rank());
		
		for (int32 i = 0; i < (int32) TargetDimension; ++i)
		{
			TensorDesc.Sizes[i] = i < DimensionOffset ? 1 : InputDesc.GetShape().GetData()[i - DimensionOffset];
		}

		uint32 CurrStride = 1;

		for (int32 i = TargetDimension - 1; i >= 0; --i)
		{
			const bool bBroadcast = TensorDesc.Sizes[i] < TargetDesc.GetShape().GetData()[i];

			TensorDesc.Strides[i] = bBroadcast ? 0 : CurrStride;
			CurrStride *= TensorDesc.Sizes[i];

			TensorDesc.Sizes[i] = TargetDesc.GetShape().GetData()[i];
		}
	}

	bool IsSameShape(const NNECore::Internal::FTensor& Left, const NNECore::Internal::FTensor& Right)
	{
		if (Left.GetShape().Rank() != Right.GetShape().Rank())
		{
			return false;
		}
		
		for (int32 Idx = 0; Idx < Left.GetShape().Rank(); ++Idx)
		{
			if (Left.GetShape().GetData()[Idx] != Right.GetShape().GetData()[Idx])
			{
				return false;
			}
		}

		return true;
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

	uint64 CalculateBufferSize(const FTensorDesc& DmlTensor, const NNECore::Internal::FTensor& Desc)
	{
		uint64 ElemSizeInBytes = Desc.GetElemByteSize();
		
		if (ElemSizeInBytes == 0)
		{
			return 0;
		}

		uint64 MinSizeInBytes = 0;		
		uint32 IndexOfLastElement = 0;

		for (int32 i = 0; i < DmlTensor.Sizes.Num(); ++i)
		{
			IndexOfLastElement += (DmlTensor.Sizes[i] - 1) * DmlTensor.Strides[i];
		}

		MinSizeInBytes = (static_cast<uint64>(IndexOfLastElement) + 1) * ElemSizeInBytes;
		
		// Round up to the nearest 4 bytes.
		MinSizeInBytes = (MinSizeInBytes + 3) & ~3ull;

		return MinSizeInBytes;
	}
}

//
// DirectML operator base class
//
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

bool FOperatorDml::InitDmlTensorDesc(DmlUtil::FTensorDesc& DmlTensorDesc, const NNECore::Internal::FTensor& Tensor)
{
	DML_TENSOR_DATA_TYPE DmlDataType = DmlUtil::GetTensorDataType(Tensor.GetDataType());

	if (DmlDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
	{
		DmlTensorDesc.BuffDesc = DML_BUFFER_TENSOR_DESC{};
		DmlTensorDesc.Desc = DML_TENSOR_DESC{};

		return false;
	}

	DmlTensorDesc.Sizes = Tensor.GetShape().GetData();
	//Note: We should support tensor padding using strides defined in FTensorDesc
	//DmlUtil::SetTensorStrides(DmlTensorDesc, TensorDesc.Strides);
		
	DML_BUFFER_TENSOR_DESC& BuffDesc = DmlTensorDesc.BuffDesc;

	BuffDesc = DML_BUFFER_TENSOR_DESC{};
	BuffDesc.DataType = DmlDataType;
	BuffDesc.Flags = Tensor.HasPreparedData() ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
	BuffDesc.DimensionCount = Tensor.GetShape().Rank();
	BuffDesc.Sizes = DmlTensorDesc.Sizes.GetData();
	BuffDesc.Strides = nullptr;
	BuffDesc.TotalTensorSizeInBytes = Tensor.GetDataSize(); // DmlUtil::CalculateBufferSize(DmlTensorDesc, Tensor);

	static DmlUtil::FSmallUIntArray ScalarShape({ 1 });

	//Handle scalar tensors
	if (Tensor.GetShape().Rank() == 0)
	{
		BuffDesc.DimensionCount = ScalarShape.Num();
		DmlTensorDesc.Sizes = ScalarShape;
		BuffDesc.Sizes = ScalarShape.GetData();
	}

	DmlTensorDesc.Desc = DML_TENSOR_DESC{ DML_TENSOR_TYPE_BUFFER, &DmlTensorDesc.BuffDesc };

	return true;
}

bool FOperatorDml::InitDmlTensorDesc(DmlUtil::FTensorDesc& DmlTensorDesc, const NNECore::Internal::FTensor& Tensor, const NNECore::Internal::FTensor& Broadcast)
{
	DML_TENSOR_DATA_TYPE DmlDataType = DmlUtil::GetTensorDataType(Tensor.GetDataType());

	if (DmlDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
	{
		DmlTensorDesc.BuffDesc = DML_BUFFER_TENSOR_DESC{};
		DmlTensorDesc.Desc = DML_TENSOR_DESC{};

		return false;
	}

	if (DmlUtil::IsSameShape(Tensor, Broadcast))
	{
		DmlTensorDesc.Sizes = Tensor.GetShape().GetData();
		DmlUtil::SetTensorStrides(DmlTensorDesc, Tensor);
	}
	else if (Tensor.GetShape().Rank() > Broadcast.GetShape().Rank())
	{
		return false;
	}
	else
	{
		DmlUtil::SetTensorSizesAndStridesForBroadcast(DmlTensorDesc, Tensor, Broadcast);
	}

	//UE_LOG(LogNNE, Warning, TEXT("DmlTensorDesc:%d,%d,%d -> %d,%d,%d"),
	//	Tensor.Sizes[0],
	//	Tensor.Dimension > 1 ? Tensor.Sizes[1] : 0,
	//	Tensor.Dimension > 2 ? Tensor.Sizes[2] : 0,
	//	DmlTensor.Sizes[0],
	//	DmlTensor.Dimension > 1 ? DmlTensor.Sizes[1] : 0,
	//	DmlTensor.Dimension > 2 ? DmlTensor.Sizes[2] : 0
	//);

	//UE_LOG(LogNNE, Warning, TEXT("DmlTensorStrides:%d,%d,%d"),
	//	DmlTensorDesc.Strides[0], DmlTensorDesc.Strides[1], DmlTensorDesc.Strides[2]
	//);

	check(DmlTensorDesc.Strides.Num() == DmlTensorDesc.Sizes.Num());
		
	DML_BUFFER_TENSOR_DESC& BuffDesc = DmlTensorDesc.BuffDesc;
		
	BuffDesc = DML_BUFFER_TENSOR_DESC{};

	BuffDesc.DataType = DmlDataType;
	BuffDesc.Flags = Tensor.HasPreparedData() ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
	BuffDesc.DimensionCount = DmlTensorDesc.Sizes.Num();
	BuffDesc.Sizes = DmlTensorDesc.Sizes.GetData();
	BuffDesc.Strides = DmlTensorDesc.Strides.GetData();
	BuffDesc.TotalTensorSizeInBytes = DmlUtil::CalculateBufferSize(DmlTensorDesc, Tensor);
		
	DmlTensorDesc.Desc = DML_TENSOR_DESC{ DML_TENSOR_TYPE_BUFFER, &DmlTensorDesc.BuffDesc };

	return true;
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
