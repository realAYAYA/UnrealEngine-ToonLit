// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/UnrealMemory.h"
#include "NNE.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEUtilitiesORTIncludeHelper.h"

namespace UE::NNERuntimeORT::Private
{
	namespace OrtHelper
	{
		inline TArray<uint32> GetShape(const Ort::Value& OrtTensor)
		{
			OrtTensorTypeAndShapeInfo* TypeAndShapeInfoPtr = nullptr;
			size_t DimensionsCount = 0;

			Ort::ThrowOnError(Ort::GetApi().GetTensorTypeAndShape(OrtTensor, &TypeAndShapeInfoPtr));
			Ort::ThrowOnError(Ort::GetApi().GetDimensionsCount(TypeAndShapeInfoPtr, &DimensionsCount));

			TArray<int64_t> OrtShape;

			OrtShape.SetNumUninitialized(DimensionsCount);
			Ort::ThrowOnError(Ort::GetApi().GetDimensions(TypeAndShapeInfoPtr, OrtShape.GetData(), OrtShape.Num()));
			Ort::GetApi().ReleaseTensorTypeAndShapeInfo(TypeAndShapeInfoPtr);

			TArray<uint32> Result;

			Algo::Transform(OrtShape, Result, [](int64_t Value)
			{
				check(Value >= 0);
				return (uint32)Value;
			});

			return Result;
		}
	}

	struct TypeInfoORT
	{
		ENNETensorDataType DataType = ENNETensorDataType::None;
		uint64 ElementSize = 0;
	};

	inline TypeInfoORT TranslateTensorTypeORTToNNE(ONNXTensorElementDataType OrtDataType)
	{
		ENNETensorDataType DataType = ENNETensorDataType::None;
		uint64 ElementSize = 0;

		switch (OrtDataType) {
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
			DataType = ENNETensorDataType::None;
			ElementSize = 0;
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
			DataType = ENNETensorDataType::Float;
			ElementSize = sizeof(float);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
			DataType = ENNETensorDataType::UInt8;
			ElementSize = sizeof(uint8);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
			DataType = ENNETensorDataType::Int8;
			ElementSize = sizeof(int8);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
			DataType = ENNETensorDataType::UInt16;
			ElementSize = sizeof(uint16);
			break;
		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
			DataType = ENNETensorDataType::Int16;
			ElementSize = sizeof(int16);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
			DataType = ENNETensorDataType::Int32;
			ElementSize = sizeof(int32);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
			DataType = ENNETensorDataType::Int64;
			ElementSize = sizeof(int64);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING:
			DataType = ENNETensorDataType::Char;
			ElementSize = sizeof(char);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
			DataType = ENNETensorDataType::Boolean;
			ElementSize = sizeof(bool);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
			DataType = ENNETensorDataType::Half;
			ElementSize = 2;
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
			DataType = ENNETensorDataType::Double;
			ElementSize = sizeof(double);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
			DataType = ENNETensorDataType::UInt32;
			ElementSize = sizeof(uint32);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
			DataType = ENNETensorDataType::UInt64;
			ElementSize = sizeof(uint64);
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:
			DataType = ENNETensorDataType::Complex64;
			ElementSize = 8;
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128:
			DataType = ENNETensorDataType::Complex128;
			ElementSize = 16;
			break;

		case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
			DataType = ENNETensorDataType::BFloat16;
			ElementSize = 2;
			break;

		default:
			DataType = ENNETensorDataType::None;
			break;
		}

		return TypeInfoORT{ DataType, ElementSize };
	}

	template <class TensorBinding> void BindTensorsToORT(
		TConstArrayView<TensorBinding> InBindingTensors,
		TConstArrayView<NNE::Internal::FTensor> InTensors,
		TConstArrayView<ONNXTensorElementDataType> InTensorsORTType,
		const Ort::MemoryInfo& InAllocatorInfo,
		TArray<Ort::Value>& OutOrtTensors
	)
	{
		const uint32 NumBinding = InBindingTensors.Num();
		const uint32 NumTensors = InTensors.Num();

		if (NumBinding != NumTensors)
		{
			UE_LOG(LogNNE, Error, TEXT("BindTensorsToORT: Number of Bindings is different from Tensors."));
			return;
		}

		for (uint32 Index = 0; Index < NumBinding; ++Index)
		{
			const TensorBinding& Binding = InBindingTensors[Index];
			const NNE::Internal::FTensor& Tensor = InTensors[Index];
			const ONNXTensorElementDataType CurrentORTType = InTensorsORTType[Index];
			const uint64 ByteCount{ InTensors[Index].GetDataSize() };
			const uint32 ArrayDimensions{ (uint32)Tensor.GetShape().Rank() };

			TUniquePtr<int64_t[]> SizesInt64t = MakeUnique<int64_t[]>(Tensor.GetShape().Rank());
			for (int32 DimIndex = 0; DimIndex < Tensor.GetShape().Rank(); ++DimIndex)
			{
				SizesInt64t.Get()[DimIndex] = Tensor.GetShape().GetData()[DimIndex];
			}
			
			OutOrtTensors.Emplace(
				Ort::Value::CreateTensor(
					InAllocatorInfo,
					Binding.Data,
					ByteCount,
					SizesInt64t.Get(),
					ArrayDimensions,
					CurrentORTType
				)
			);
		}
	}

	template <class TensorBinding> void CopyFromORTToBindings(
		TConstArrayView<Ort::Value> InOrtTensors,
		TConstArrayView<TensorBinding> InBindingTensors,
		TConstArrayView<NNE::FTensorDesc> InTensorDescs,
		TArray<NNE::Internal::FTensor>& OutTensors
	)
	{
		const uint32 NumBinding = InOrtTensors.Num();
		const uint32 NumDescriptors = InTensorDescs.Num();

		if (NumBinding != NumDescriptors)
		{
			UE_LOG(LogNNE, Error, TEXT("CopyFromORTToBindings: Number of Bindings is different from Descriptors."));
			return;
		}

		for (uint32 Index = 0; Index < NumBinding; ++Index)
		{
			const TensorBinding& Binding = InBindingTensors[Index];
			const NNE::FTensorDesc& TensorDesc = InTensorDescs[Index];
			const Ort::Value& OrtTensor = InOrtTensors[Index];
			const TArray<uint32> OrtShape = OrtHelper::GetShape(OrtTensor);
			const void* OrtTensorData = OrtTensor.GetTensorData<void>();
			const NNE::FTensorShape Shape = NNE::FTensorShape::Make(OrtShape);
			const NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(TensorDesc.GetName(), Shape, TensorDesc.GetDataType());
			const uint64 DataSize = FMath::Min(Tensor.GetDataSize(), Binding.SizeInBytes);

			OutTensors.Emplace(Tensor);

			if (DataSize > 0)
			{
				FMemory::Memcpy(Binding.Data, OrtTensorData, DataSize);
			}

			if (Tensor.GetDataSize() > Binding.SizeInBytes)
			{
				UE_LOG(LogNNE, Error, TEXT("CopyFromORTToBindings: Binding buffer was not large enough to contain all of the data, only first %d bytes were copied."), DataSize);
			}
		}
	}

} // UE::NNERuntimeORT::Private