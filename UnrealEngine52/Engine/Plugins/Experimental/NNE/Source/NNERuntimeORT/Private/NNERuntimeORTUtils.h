// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/UnrealMemory.h"
#include "NNECore.h"
#include "NNECoreRuntimeGPU.h"
#include "NNECoreTypes.h"
#include "NNECoreTensor.h"
#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

DECLARE_STATS_GROUP(TEXT("NNE"), STATGROUP_NNE, STATCAT_Advanced);

namespace UE::NNERuntimeORT::Private
{

	using TypeInfoORT = std::pair<ENNETensorDataType, uint64>;

	inline TypeInfoORT TranslateTensorTypeORTToNNE(unsigned int OrtDataType)
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

	inline void BindTensorsToORT(
		TConstArrayView<NNECore::FTensorBindingGPU> InBindingTensors,
		TConstArrayView<NNECore::Internal::FTensor> InTensors,
		TConstArrayView<ONNXTensorElementDataType> InTensorsORTType,
		const Ort::MemoryInfo* InAllocatorInfo,
		TArray<Ort::Value>& OutOrtTensors
	)
	{
		const uint32 NumBinding = InBindingTensors.Num();
		const uint32 NumDescriptors = InTensors.Num();

		if (NumBinding != NumDescriptors)
		{
			UE_LOG(LogNNE, Error, TEXT("BindTensorsToORT: Number of Bindings is different of Descriptors."));
			return;
		}

		for (uint32 Index = 0; Index < NumBinding; ++Index)
		{
			const NNECore::FTensorBindingGPU& Binding = InBindingTensors[Index];
			const NNECore::Internal::FTensor& Tensor = InTensors[Index];
			const ONNXTensorElementDataType CurrentORTType = InTensorsORTType[Index];

			TUniquePtr<int64_t[]> SizesInt64t;
			SizesInt64t = MakeUnique<int64_t[]>(Tensor.GetShape().Rank());
			for (int32 DimIndex = 0; DimIndex < Tensor.GetShape().Rank(); ++DimIndex)
			{
				SizesInt64t.Get()[DimIndex] = Tensor.GetShape().GetData()[DimIndex];
			}

			const uint64 ByteCount{ InTensors[Index].GetDataSize() };
			const uint32 ArrayDimensions{ (uint32)Tensor.GetShape().Rank() };
			OutOrtTensors.Emplace(
				Ort::Value::CreateTensor(
					*InAllocatorInfo,
					Binding.Data,
					ByteCount,
					SizesInt64t.Get(),
					ArrayDimensions,
					CurrentORTType
				)
			);
		}
	}

	inline void CopyFromORTToBindings(
		TConstArrayView<Ort::Value> InOrtTensors,
		TConstArrayView<NNECore::FTensorBindingGPU> InBindingTensors,
		TConstArrayView<NNECore::FTensorDesc> InTensorDescs,
		TArray<NNECore::Internal::FTensor>& OutTensors
	)
	{
		const uint32 NumBinding = InOrtTensors.Num();
		const uint32 NumDescriptors = InTensorDescs.Num();

		if (NumBinding != NumDescriptors)
		{
			UE_LOG(LogNNE, Error, TEXT("CopyFromORTToBindings: Number of Bindings is different of Descriptors."));
			return;
		}

		for (uint32 Index = 0; Index < NumBinding; ++Index)
		{
			const NNECore::FTensorBindingGPU& Binding = InBindingTensors[Index];
			const NNECore::FTensorDesc& TensorDesc = InTensorDescs[Index];
			const Ort::Value& OrtTensor = InOrtTensors[Index];
			const std::vector<int64_t>& OrtShape = OrtTensor.GetTensorTypeAndShapeInfo().GetShape();

			TArray<uint32> ShapeData;
			for (int32 DimIndex = 0; DimIndex < OrtShape.size(); ++DimIndex)
			{
				check(OrtShape[DimIndex] >= 0);
				ShapeData.Add(OrtShape[DimIndex]);
			}

			NNECore::FTensorShape Shape = NNECore::FTensorShape::Make(ShapeData);
			NNECore::Internal::FTensor Tensor = NNECore::Internal::FTensor::Make(TensorDesc.GetName(), Shape, TensorDesc.GetDataType());
			OutTensors.Emplace(Tensor);

			const void* OrtTensorData = OrtTensor.GetTensorData<void>();
			const uint64 DataSize = FMath::Min(Tensor.GetDataSize(), Binding.SizeInBytes);

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