// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "NNETypes.h"

namespace UE::NNE::Internal
{
	/** Concrete tensor with data accessible by graph scheduling */
	class FTensor
	{
	protected:
		FString				Name;
		ENNETensorDataType	DataType;
		FTensorShape		Shape;
		TArray<uint8>		PreparedData;
		uint64				DataSize = 0;
		uint32				Volume = 0;

		FTensor() = default;
		
	public:
		const FString& GetName() const
		{
			return Name;
		}

		ENNETensorDataType GetDataType() const
		{
			return DataType;
		}

		uint32 GetElementByteSize() const
		{
			return GetTensorDataTypeSizeInBytes(DataType);
		}
		
		const FTensorShape& GetShape() const
		{
			return Shape;
		}

		template <typename T> TConstArrayView<T> GetPreparedData() const
		{
			const T* DataPtr = reinterpret_cast<const T*>(PreparedData.GetData());
			const int32 ElemSize = sizeof(T);

			check(PreparedData.Num() % ElemSize == 0);
			return MakeArrayView(DataPtr, PreparedData.Num() / ElemSize);
		}

		void SetShape(const FTensorShape& InShape)
		{
			checkf(!HasPreparedData(), TEXT("Shape cannot be changed once data as been set."));
			check(InShape.Volume() <= TNumericLimits<uint32>::Max());
			Shape = InShape;
			Volume = InShape.Volume();
			DataSize = (uint64)GetTensorDataTypeSizeInBytes(DataType) * Volume;
		}

		template <typename T> void SetPreparedData(TConstArrayView<T> Data)
		{
			const uint8* DataPtr = reinterpret_cast<const uint8*>(Data.GetData());
			TConstArrayView<uint8> DataAsByte = MakeArrayView(DataPtr, Data.Num() * sizeof(T));
			
			checkf(DataAsByte.Num() == DataSize, TEXT("Incorrect data size, it should match tensor shape and data type."));
			PreparedData.Reset();
			PreparedData.Append(DataAsByte);
		}

		bool HasPreparedData() const
		{
			return !PreparedData.IsEmpty();
		}

		bool IsConstant() const
		{
			return Volume == 0 || HasPreparedData();
		}

		uint32 GetVolume() const
		{
			return Volume;
		}

		uint64 GetDataSize() const
		{
			return DataSize;
		}

		static FTensor Make(const FString& Name, const FTensorShape& Shape, ENNETensorDataType DataType)
		{
			FTensor Tensor;
			Tensor.Name = Name;
			Tensor.DataType = DataType;
			Tensor.SetShape(Shape);
			return Tensor;
		}

		static FTensor Make(const FTensorDesc& TensorDesc, const FTensorShape& Shape)
		{
			check(Shape.IsCompatibleWith(TensorDesc.GetShape()));
			return Make(TensorDesc.GetName(), Shape, TensorDesc.GetDataType());
		}

		static FTensor MakeFromSymbolicDesc(const FTensorDesc& TensorDesc)
		{
			return Make(TensorDesc.GetName(), FTensorShape::MakeFromSymbolic(TensorDesc.GetShape()), TensorDesc.GetDataType());
		}
	};

	using FTensorRef = FTensor*;

} // namespace UE::NNE::Internal