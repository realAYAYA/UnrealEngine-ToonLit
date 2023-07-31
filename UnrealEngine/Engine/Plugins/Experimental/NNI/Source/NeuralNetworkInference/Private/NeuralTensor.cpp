// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralTensor.h"
#include "Algo/Accumulate.h"
#include "Containers/ResourceArray.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"
#include "NeuralTensorResourceArray.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"



/* FPrivateNeuralTensor auxiliary functions
 *****************************************************************************/

class FPrivateNeuralTensor
{
public:
	template <typename T>
	static FString SanitizeFloat(const T InValue);
	static void ArrayToSanitizedString(FString& InOutTensorString, const int64 InIndexStart, const int64 InIndexEnd, const int64 InOffset,
		const ENeuralDataType InDataType, const FNeuralTensor& InTensor);

	static void NDTensorIndexesPlus1(TArray<int32>& InOutImageAreaIndexes, const TArray<int32>& InSizes);

	template <typename T>
	struct TMultiplies
	{
		constexpr T operator()(const T& InLeft, const T& InRight) const { return InLeft * InRight; }
	};


	/**
	 * It gets the data type from its ETensorProtoDataType analog. E.g.,
	 * const ENeuralDataType NeuralDataType = FModelProto::GetDataTypeFromTensorProtoDataType(TensorProto.DataType);
	 */
	static ENeuralDataType GetDataTypeFromTensorProtoDataType(const ETensorProtoDataType InTensorProtoDataType);
};

template <typename T>
FString FPrivateNeuralTensor::SanitizeFloat(const T InValue)
{
	return (FMath::IsFinite(InValue) ? FString::SanitizeFloat(InValue) : FString(TEXT("NaNInf")));
}

#define FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(TensorString, InTensor, IndexStart, IndexEnd, Offset, FNumberToStringFunction, DataType) \
	for (int64 Index = IndexStart; Index < IndexEnd; ++Index) \
	{ \
		TensorString += FNumberToStringFunction(InTensor.At<DataType>(Offset + Index)) + TEXT(" "); \
	}

void FPrivateNeuralTensor::ArrayToSanitizedString(FString& InOutTensorString, const int64 InIndexStart, const int64 InIndexEnd, const int64 InOffset,
	const ENeuralDataType InDataType, const FNeuralTensor& InTensor)
{
	if (InDataType == ENeuralDataType::Float)
	{
		FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(InOutTensorString, InTensor, InIndexStart, InIndexEnd, InOffset, FPrivateNeuralTensor::SanitizeFloat,
			float);
	}
	else if (InDataType == ENeuralDataType::Int32)
	{
		FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(InOutTensorString, InTensor, InIndexStart, InIndexEnd, InOffset, FString::FromInt, int32);
	}
	else if (InDataType == ENeuralDataType::Int64)
	{
		FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(InOutTensorString, InTensor, InIndexStart, InIndexEnd, InOffset, FString::FromInt, int64);
	}
	else if (InDataType == ENeuralDataType::UInt32)
	{
		FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(InOutTensorString, InTensor, InIndexStart, InIndexEnd, InOffset, FString::FromInt, uint32);
	}
	else if (InDataType == ENeuralDataType::UInt64)
	{
		FOR_LOOP_FLOAT_TYPE_TO_SANITIZED_STRING(InOutTensorString, InTensor, InIndexStart, InIndexEnd, InOffset, FString::FromInt, uint64);
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::ArrayToSanitizedString(): Unknown InDataType = %d used."),
			(int32)InDataType);
	}
}

void FPrivateNeuralTensor::NDTensorIndexesPlus1(TArray<int32>& InOutImageAreaIndexes, const TArray<int32>& InSizes)
{
	int64 ImageAreaIndexesIndex = InSizes.Num() - 1;
	while (ImageAreaIndexesIndex > -1)
	{
		++InOutImageAreaIndexes[ImageAreaIndexesIndex];
		if (InOutImageAreaIndexes[ImageAreaIndexesIndex] == InSizes[ImageAreaIndexesIndex])
		{
			InOutImageAreaIndexes[ImageAreaIndexesIndex] = 0;
			--ImageAreaIndexesIndex;
		}
		else
		{
			break;
		}
	}
}

ENeuralDataType FPrivateNeuralTensor::GetDataTypeFromTensorProtoDataType(const ETensorProtoDataType InTensorProtoDataType)
{
	if (InTensorProtoDataType == ETensorProtoDataType::FLOAT)
	{
		return ENeuralDataType::Float;
	}
	else if (InTensorProtoDataType == ETensorProtoDataType::INT32)
	{
		return ENeuralDataType::Int32;
	}
	else if (InTensorProtoDataType == ETensorProtoDataType::INT64)
	{
		return ENeuralDataType::Int64;
	}
	else if (InTensorProtoDataType == ETensorProtoDataType::UINT32)
	{
		return ENeuralDataType::UInt32;
	}
	else if (InTensorProtoDataType == ETensorProtoDataType::UINT64)
	{
		return ENeuralDataType::UInt64;
	}
	else if (InTensorProtoDataType == ETensorProtoDataType::UNDEFINED)
	{
		return ENeuralDataType::None;
	}
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::GetDataTypeFromTensorProtoDataType(): Unknown InTensorProtoDataType = %d used."),
		(int32)InTensorProtoDataType);
	return ENeuralDataType::None;
}



/* FNeuralTensor costructors
 *****************************************************************************/

FNeuralTensor::FNeuralTensor(const ENeuralDataType InDataType, const TArray<int64>& InSizes, const FString& InName,
	const ENeuralTensorType InTensorType)
	: DataType(ENeuralDataType::None)
	, Name(InName)
	, TensorType(InTensorType)
	, bEnableGPU(false)
{
	// Memory allocation
	SetNumUninitialized(InDataType, InSizes);
}

FNeuralTensor::FNeuralTensor(const ENeuralDataType InDataType, const int64 InVolume, const FString& InName, const ENeuralTensorType InTensorType)
	: FNeuralTensor(InDataType, InVolume > 0 ? TArray<int64>({ InVolume }) : TArray<int64>({}), InName, InTensorType)
{
}



/* FNeuralTensor private costructor
 *****************************************************************************/

FNeuralTensor::FNeuralTensor(const ENeuralDataType InDataType, const void* const InValues, const int64 InSizeOfT, const int64 InValueNum,
	const TArray<int64>& InSizes, const FString& InName, const ENeuralTensorType InTensorType)
	: FNeuralTensor(InDataType, InSizes, InName, InTensorType)
{
	// Sanity check
	if (IsEmpty())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensor(): GetVolume() = 0, skipping array copy. Use a different constructor if the volume is 0 to avoid this warning."));
	}
	// Memory filling
	else
	{
		SetFromVoidPointerCopy(InValues, InSizeOfT, InValueNum);
	}
}



/* FNeuralTensor copy/move structors/assignment
 *****************************************************************************/

FNeuralTensor::FNeuralTensor(const FNeuralTensor& InTensor)
	: FNeuralTensor(InTensor.DataType, InTensor.Sizes, InTensor.Name)
{
	SetTensorType(InTensor.TensorType);
	UnderlyingUInt8ArrayData = InTensor.UnderlyingUInt8ArrayData;
	bEnableGPU = InTensor.bEnableGPU;
	PooledBuffer = InTensor.PooledBuffer;
	BufferSRVRef = InTensor.BufferSRVRef;
	BufferUAVRef = InTensor.BufferUAVRef;
}

FNeuralTensor& FNeuralTensor::operator=(const FNeuralTensor& InTensor)
{
	Name = InTensor.Name;
	SetNumUninitialized(InTensor.DataType, InTensor.Sizes);
	SetTensorType(InTensor.TensorType);
	UnderlyingUInt8ArrayData = InTensor.UnderlyingUInt8ArrayData;
	bEnableGPU = InTensor.bEnableGPU;
	PooledBuffer = InTensor.PooledBuffer;
	BufferSRVRef = InTensor.BufferSRVRef;
	BufferUAVRef = InTensor.BufferUAVRef;

	return *this;
}

FNeuralTensor::FNeuralTensor(FNeuralTensor&& InTensor)
	: FNeuralTensor(InTensor.DataType, InTensor.Sizes, TEXT(""))
{
	Swap(Name, InTensor.Name);
	SetTensorType(InTensor.TensorType);
	Swap(UnderlyingUInt8ArrayData, InTensor.UnderlyingUInt8ArrayData);
	bEnableGPU = InTensor.bEnableGPU;
	Swap(PooledBuffer, InTensor.PooledBuffer);
	Swap(BufferSRVRef, InTensor.BufferSRVRef);
	Swap(BufferUAVRef, InTensor.BufferUAVRef);
}

FNeuralTensor& FNeuralTensor::operator=(FNeuralTensor&& InTensor)
{
	Swap(Name, InTensor.Name);
	SetNumUninitialized(InTensor.DataType, InTensor.Sizes);
	SetTensorType(InTensor.TensorType);
	Swap(UnderlyingUInt8ArrayData, InTensor.UnderlyingUInt8ArrayData);
	bEnableGPU = InTensor.bEnableGPU;
	Swap(PooledBuffer, InTensor.PooledBuffer);
	Swap(BufferSRVRef, InTensor.BufferSRVRef);
	Swap(BufferUAVRef, InTensor.BufferUAVRef);

	return *this;
}



/* FNeuralTensor public operators
 *****************************************************************************/

bool FNeuralTensor::operator==(const FNeuralTensor& InTensor) const
{
	// Dimensions, sizes, and scalar type match --> Check if data does
	if (DataType == InTensor.DataType && Num() == InTensor.Num() && GetSizes() == InTensor.GetSizes())
	{
		return UnderlyingUInt8ArrayData == InTensor.UnderlyingUInt8ArrayData;
	}
	// Dimensions or sizes or scalar type do not match
	return false;
}



/* FNeuralTensor public functions
 *****************************************************************************/

int64 FNeuralTensor::GetSize(const int32 InDimension) const
{
	return (InDimension < GetNumberDimensions() ? Sizes[InDimension] : 1u);
}

void FNeuralTensor::SetTensorType(const ENeuralTensorType InTensorType)
{
	// Sanity check
	if (PooledBuffer.IsValid() || BufferSRVRef.IsValid() || BufferUAVRef.IsValid())
	{
		PooledBuffer.Reset();
		BufferSRVRef.Reset();
		BufferUAVRef.Reset();
	}

	// Update TensorType
	TensorType = InTensorType;
}

void FNeuralTensor::SetNumUninitialized(const ENeuralDataType InDataType, const TArray<int64>& InSizes, const bool bInAllowShrinking)
{
	// Update DataType
	if (InDataType != ENeuralDataType::None)
	{
		DataType = InDataType;
	}
	// Update Sizes
	Sizes = InSizes;
	// Re-initialize UnderlyingUInt8ArrayData
	if (Sizes.Num() > 0)
	{
		Volume = FNeuralNetworkInferenceUtils::Product<int64>(Sizes);
		const int64 VolumeInBytes = Volume * FNeuralDataTypeUtils::GetByteSize(DataType);
		if (VolumeInBytes != UnderlyingUInt8ArrayData.Num())
		{
			UnderlyingUInt8ArrayData.SetNumUninitialized(VolumeInBytes, bInAllowShrinking); // Pre-allocate TArray
		}
	}
	else
	{
		Volume = 0;
		UnderlyingUInt8ArrayData.Empty();
	}
}

void FNeuralTensor::SetFromUnderlyingUInt8ArrayCopy(const TArray<uint8>& InArray)
{
	if (NumInBytes() != InArray.Num())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromUnderlyingUInt8ArrayCopy():"
			" NumInBytes() == InArray.Num() failed, %d != %d."), NumInBytes(), InArray.Num());
		return;
	}
	UnderlyingUInt8ArrayData = InArray;
}

void FNeuralTensor::SetFromVoidPointerCopy(const void* const InVoidPtr)
{
	if (InVoidPtr)
	{
		FMemory::Memcpy(UnderlyingUInt8ArrayData.GetData(), InVoidPtr, NumInBytes());
	}
}

bool FNeuralTensor::SetFromTensorProto(const FTensorProto* const InTensorProto, const FString& InTensorName, const ENeuralTensorType InTensorType)
{
	if (!InTensorProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromTensorProto(): InTensorProto was a nullptr."));
		return false;
	}

	// const FString& InExternalDataDirectory = TEXT(""); // InExternalDataDirectory only required if InTensorProto->ExternalData is being used.

	// Create Tensor
	Name = InTensorName;
	TensorType = InTensorType;
	// Memory allocation
	SetNumUninitialized(FPrivateNeuralTensor::GetDataTypeFromTensorProtoDataType(InTensorProto->DataType), InTensorProto->Dimensions);

	// RawData
	if (!InTensorProto->RawData.IsEmpty())
	{
		SetFromUnderlyingUInt8ArrayCopy(InTensorProto->RawData);
	}

	// InTensorProto->ExternalData
	else if (!InTensorProto->ExternalData.IsEmpty())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromTensorProto(): DEPRECATED CODE, OTXT no longer functional."));
		// // Sanity check
		// if (InTensorProto->ExternalData[0].Key != TEXT("location") || InTensorProto->ExternalData[0].Value.Len() < 1)
		// {
		// 	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromTensorProto():"
		//      " InTensorProto->ExternalData[0].Key = %s != \"location\" || InTensorProto->ExternalData[0].Value.Len() = %d (should be > 0)."),
		//      *InTensorProto->ExternalData[0].Key, InTensorProto->ExternalData[0].Value.Len());
		// 	return false;
		// }
		// // Read neural tensor from binary data
		// const FString BinaryWeightFilePath = InExternalDataDirectory / InTensorProto->ExternalData[0].Value;
		// if (!FModelProtoFileReader::ReadWeightsFromOtxtBinaryFile((char*)GetData(), Num() * FNeuralDataTypeUtils::GetByteSize(NeuralDataType),
		//     BinaryWeightFilePath))
		// {
		// 	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromTensorProto(): Could not read binary file: %s."),
		//      *BinaryWeightFilePath);
		// 	return false;
		// }
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::SetFromTensorProto(): InTensorProto was empty (RawData and ExternalData)."));
		return false;
	}

	// No issues --> Read successfully
	return true;
}

bool FNeuralTensor::Flip(const int32 InDimension)
{
	// Sanity check
	if (InDimension >= GetNumberDimensions())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensor-%s::Transpose(): InDimension < GetNumberDimensions() failed, %d >= %d."), *Name, InDimension, GetNumberDimensions());
		return false;
	}

	// Find offset created for all dimensions > InDimension
	const int32 NumberDimensions = GetNumberDimensions();
	int64 DimensionOffset = 1;
	for (int32 DimensionIndex = InDimension + 1; DimensionIndex < NumberDimensions; ++DimensionIndex)
	{
		DimensionOffset *= Sizes[DimensionIndex];
	}
	const int64 BytesPerIndex = FNeuralDataTypeUtils::GetByteSize(DataType);
	const int64 DimensionOffsetInBytes = DimensionOffset * BytesPerIndex;

	// Fill TensorNDIndexes and TensorNDSizes
	TArray<int32> TensorNDIndexes;
	TArray<int32> TensorNDSizes;
	{
		const int32 TensorNDSize = InDimension + 1; // NumberDimensions
		TensorNDIndexes.Init(0, TensorNDSize);
		TensorNDSizes.Init(1, TensorNDSize);
		for (int32 NDTensorSizeIndex = 0; NDTensorSizeIndex < TensorNDSize; ++NDTensorSizeIndex)
		{
			TensorNDSizes[NDTensorSizeIndex] *= Sizes[NDTensorSizeIndex]; // E.g., Sizes=[1,2,3,4] and InDimension=2 --> TensorNDSizes = [1, 2x3x4]
		}
	}

	// Flip each value
	TArray<uint8> NewArrayOnCPU;
	NewArrayOnCPU.SetNumUninitialized(NumInBytes());
	for (int64 TensorIndex = 0; TensorIndex < Num(); TensorIndex += DimensionOffset)
	{
		// Get FlippedTensorIndex
		int64 FlippedTensorIndex = TensorNDIndexes[0];
		for (int64 DimensionIndex = 1; DimensionIndex < TensorNDIndexes.Num(); ++DimensionIndex)
		{
			//FlippedTensorIndex = (TensorNDIndexes[0] * Sizes[1] + TensorNDIndexes[1]) * Sizes[2] + TensorNDIndexes[2] ...;
			FlippedTensorIndex = FlippedTensorIndex * Sizes[DimensionIndex] + TensorNDIndexes[DimensionIndex];
		}
		// Remove last index (that makes it a normal index) and replace with its flipped equivalent
		FlippedTensorIndex = FlippedTensorIndex + Sizes[InDimension] - 1 - 2 * TensorNDIndexes.Last();
		// Flip TensorIndex value
		FMemory::Memcpy(&NewArrayOnCPU.GetData()[TensorIndex * BytesPerIndex],
			&UnderlyingUInt8ArrayData.GetData()[FlippedTensorIndex * DimensionOffsetInBytes], DimensionOffsetInBytes);
		// Increase TensorNDIndexes
		FPrivateNeuralTensor::NDTensorIndexesPlus1(TensorNDIndexes, TensorNDSizes);
	}
	Swap(NewArrayOnCPU, UnderlyingUInt8ArrayData);
	return true;
}

bool FNeuralTensor::Flip(const int32 InDimensionFirst, const int32 InDimensionLast)
{
	// Sanity checks
	if (InDimensionFirst >= InDimensionLast)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::Flip(): InDimensionFirst < InDimensionLast failed, %d >= %d."),
			*Name, InDimensionFirst, InDimensionLast);
		return false;
	}
	else if (InDimensionLast > GetNumberDimensions())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::Flip(): InDimensionLast < GetNumberDimensions() failed, %d >= %d."),
			*Name, InDimensionLast, GetNumberDimensions());
		return false;
	}
	// Flip
	for (int32 DimensionIndex = InDimensionFirst; DimensionIndex < InDimensionLast; ++DimensionIndex)
	{
		if (!Flip(DimensionIndex))
		{
			return false;
		}
	}
	return true;
}

bool FNeuralTensor::Transpose()
{
	const int32 NumberDimensions = GetNumberDimensions();
	// The transpose of a 0D tensor is itself
	if (NumberDimensions > 0)
	{
		// 1-D tensors
		if (NumberDimensions == 1)
		{
			Sizes.Push(1);
		}
		// 2-D tensors
		else if (NumberDimensions == 2)
		{
			// Fill NewArrayOnCPU
			TArray<uint8> NewArrayOnCPU;
			NewArrayOnCPU.SetNumUninitialized(NumInBytes());
			uint8* NewArrayOnCPUPtr = NewArrayOnCPU.GetData();
			const uint8* const ArrayOnCPUPtr = UnderlyingUInt8ArrayData.GetData();
			const int64 Height = Sizes[0];
			const int64 Width = Sizes[1];
			const int64 Bytes = FNeuralDataTypeUtils::GetByteSize(DataType);
			for (int64 Y = 0; Y < Height; ++Y)
			{
				for (int64 X = 0; X < Width; ++X)
				{
					for (int64 B = 0; B < Bytes; ++B)
					{
						NewArrayOnCPU/*Ptr*/[(X * Height + Y)*Bytes + B] = UnderlyingUInt8ArrayData/*Ptr*/[(Y * Width + X)*Bytes + B];
					}
				}
			}
			Swap(NewArrayOnCPU, UnderlyingUInt8ArrayData);
		}
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::Transpose(): Unexpected case NumberDimensions = %d != 1 || 2."),
				*Name, NumberDimensions);
			return false;
		}
		// Swap W <-> H
		Swap(Sizes[0], Sizes[1]);
	}
	return true;
}

bool FNeuralTensor::Reshape(const TArray<int64>& InSizes)
{
	TArray<int64> NewSizes = InSizes;
	return ReshapeMove(NewSizes);
}

bool FNeuralTensor::ReshapeMove(TArray<int64>& InSizes)
{
	const int64 NewVolume = Algo::Accumulate(InSizes, int64(1), FPrivateNeuralTensor::TMultiplies<int64>());
	if (Volume == NewVolume)
	{
		Swap(Sizes, InSizes);
		return true;
	}
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::ReshapeMove(): Volume == NewVolume failed, %d != %d."), *Name, Volume,
		NewVolume);
	return false;
}

void FNeuralTensor::CPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
// @todo: Volatile or not adding EBufferUsageFlags::ShaderResource causes this error:
//Fatal error: [File:D:/P4/private_dh_research_pitt/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Util.cpp] [Line: 258] 
//Result failed 
// at D:/P4/private_dh_research_pitt/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11VertexBuffer.cpp:109 
// with error E_INVALIDARG
	// Idea:
	// - EBufferUsageFlags::Volatile: Updated multiple times in a frame, but does not imply a lifetime of 1 frame. E.g. a vertex buffer you update
	//   every frame with new vertices.
	// - EBufferUsageFlags::KeepCPUAccessible: Not needed, I can just copy the final GPU memory back to RAM at the very end
	// - Input and Intermediate(Not)Initialized currently share the same attributes because input might become intermediate (e.g., if input tensor
	//   fed into a ReLU, which simply modifies the input FNeuralTensor). However, Intermediate(Not)Initialized and Output do not copy the memory
	//   from CPU to GPU but rather simply allocates it.
	// - Output might also become Intermediate(Not)Initialized (e.g., if Output -> ReLU -> Output), so it is kept as ReadWrite rather than written
	//   once to account for this.
	// Call ToGPU_RenderThread with the right flags
	if (TensorType == ENeuralTensorType::Generic)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, true);
	}
	else if (TensorType == ENeuralTensorType::Input)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, true);
	}
	else if (TensorType == ENeuralTensorType::IntermediateNotInitialized)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder,
			EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, false);
	}
	else if (TensorType == ENeuralTensorType::IntermediateInitialized)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, true);
	}
	else if (TensorType == ENeuralTensorType::Output)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, false);
	}
	else if (TensorType == ENeuralTensorType::Weight)
	{
		return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static, true);
	}
	UE_LOG(LogNeuralNetworkInference, Warning,
		TEXT("FNeuralTensor-%s::CPUToRDGBuilder_RenderThread(): Unimplemented TensorType = %d. Assuming ENeuralTensorType::Generic."),
		*Name, (int32)TensorType);
	return CPUToRDGBuilder_RenderThread(InOutGraphBuilder, EBufferUsageFlags::UnorderedAccess, true);
}

void FNeuralTensor::GPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	if (!bEnableGPU)
	{
		return;
	}
	// Sanity checks
	checkf(IsInRenderingThread(), TEXT("FNeuralTensor-%s::GPUToRDGBuilder_RenderThread(): IsInRenderingThread() must be true."), *Name);
	checkf(PooledBuffer.IsValid() && InOutGraphBuilder, TEXT("FNeuralTensor-%s::GPUToRDGBuilder_RenderThread(): IPooledBuffer and InOutGraphBuilder"
		" cannot be nullptrs."), *Name);
	// Register BufferRef
	FRDGBufferRef BufferRef = InOutGraphBuilder->RegisterExternalBuffer(*PooledBuffer);
	// Recreate BufferSRVRef
	if (BufferSRVRef.IsValid())
	{
		BufferSRVRef = MakeShared<FRDGBufferSRVRef>(InOutGraphBuilder->CreateSRV(BufferRef, FNeuralDataTypeUtils::GetPixelFormat(DataType)));
	}
	// Recreate BufferUAVRef
	if (BufferUAVRef.IsValid())
	{
		BufferUAVRef = MakeShared<FRDGBufferUAVRef>(InOutGraphBuilder->CreateUAV(BufferRef, FNeuralDataTypeUtils::GetPixelFormat(DataType)));
	}
}

void FNeuralTensor::RDGBuilderToCPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!bEnableGPU || IsEmpty())
	{
		return;
	}
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::RDGBuilderToCPU_RenderThread(): Sanity checks failed."), *Name);
		return;
	}
	// Read GPU memory back into CPU
	InOutGraphBuilder->AddPass(
		RDG_EVENT_NAME("FNeuralTensor(%s)::ToCPU()", *Name),
		ERDGPassFlags::None,
		[this](FRHICommandListImmediate& RHICmdList)
	{
		const int64 VolumeInBytes = NumInBytes();
		FRHIBuffer* VertexBuffer = (*PooledBuffer)->GetRHI();
		const void* const BufferData = RHICmdList.LockBuffer(VertexBuffer, 0, VolumeInBytes, RLM_ReadOnly);
		FMemory::Memcpy((void*)UnderlyingUInt8ArrayData.GetData(), BufferData, VolumeInBytes);
		RHICmdList.UnlockBuffer(VertexBuffer);
	});
}

bool FNeuralTensor::InitPooledBufferForUEAndORTBackEnd(void** InOutNativeResource)
{
	if (!bEnableGPU)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor::InitPooledBufferForUEAndORTBackEnd(): bEnableGPU is false."));
		return false;
	}

	// Recreate PooledBuffer for future runs
	PooledBuffer = MakeShared<TRefCountPtr<FRDGPooledBuffer>>();

	// NOTE: SRV and UAV can be used by callers (for example MLDeformer), look at the GetPooledBuffer()
	BufferSRVRef.Reset();
	BufferUAVRef.Reset();

	// Make sure that we wait for Graph builder to finish
	std::atomic<bool> bIsGraphBuilderDone(false);

	ENQUEUE_RENDER_COMMAND(FNeuralTensor_InitPooledBuffer)(
		[this, &InOutNativeResource, &bIsGraphBuilderDone] (FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder Builder(RHICmdList);
			InitPooledBufferForUEAndORTBackEnd_RenderThread(Builder);
			Builder.Execute();

#ifdef PLATFORM_WIN64
			if (InOutNativeResource)
			{
				FRHIBuffer* Buffer = (*PooledBuffer)->GetRHI();
				InOutNativeResource[0] = FNeuralNetworkInferenceUtilsGPU::GetD3D12Resource(Buffer);
			}
#endif

			bIsGraphBuilderDone = true;
		}
	);

	// Wait until the graph builder is done
	while (!bIsGraphBuilderDone)
	{
		FPlatformProcess::Sleep(0.1e-3);
	}

	return true;
}

void FNeuralTensor::InitPooledBufferForUEAndORTBackEnd_RenderThread(FRDGBuilder& GraphBuilder)
{
	FRDGBufferDesc BufferDesc;
	BufferDesc.BytesPerElement = FNeuralDataTypeUtils::GetByteSize(DataType);
	BufferDesc.NumElements = Num();
	BufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::VertexBuffer;
	FRDGBufferRef BufferRef = GraphBuilder.CreateBuffer(BufferDesc, *GetName());

	// Extract buffer immediately. Need this so that we can potentially bind to DirectML.
	if (!PooledBuffer.IsValid())
	{
		PooledBuffer = MakeShared<TRefCountPtr<FRDGPooledBuffer>>();
	}
	*PooledBuffer = GraphBuilder.ConvertToExternalBuffer(BufferRef);
}

const TRefCountPtr<FRDGPooledBuffer>& FNeuralTensor::GetPooledBuffer() const
{
	// Sanity checks
	checkf(bEnableGPU, TEXT("FNeuralTensor-%s::GetPooledBuffer(): bEnableGPU must be true."), *Name);
	checkf(IsInRenderingThread(), TEXT("FNeuralTensor-%s::GetPooledBuffer(): IsInRenderingThread() must be true."), *Name);
	checkf(PooledBuffer.IsValid(), TEXT("FNeuralTensor-%s::GetPooledBuffer(): PooledBuffer cannot be nullptr."), *Name);
	// Return PooledBuffer
	return *PooledBuffer;
}

const FRDGBufferSRVRef FNeuralTensor::GetBufferSRVRef() const
{
	// Sanity checks
	checkf(bEnableGPU, TEXT("FNeuralTensor-%s::GetBufferSRVRef(): bEnableGPU must be true."), *Name);
	checkf(IsInRenderingThread(), TEXT("FNeuralTensor-%s::GetBufferSRVRef(): IsInRenderingThread() must be true."), *Name);
	checkf(BufferSRVRef.IsValid(), TEXT("FNeuralTensor-%s::GetBufferSRVRef(): BufferSRVRef was a nullptr, 2 possible causes:"
		" 1) FNeuralTensor::CPUToRDGBuilder_RenderThread() was not called. 2) The tensor was empty."), *Name);
	// Return BufferSRVRef
	return *BufferSRVRef;
}

FRDGBufferUAVRef FNeuralTensor::GetBufferUAVRef()
{
	// Sanity checks
	checkf(bEnableGPU, TEXT("FNeuralTensor-%s::GetBufferUAVRef(): bEnableGPU must be true."), *Name);
	checkf(IsInRenderingThread(), TEXT("FNeuralTensor-%s::GetBufferUAVRef(): IsInRenderingThread() must be true."), *Name);
	checkf(BufferUAVRef.IsValid(), TEXT("FNeuralTensor-%s::GetBufferUAVRef(): BufferUAVRef cannot be nullptr."), *Name);
	// Return BufferUAVRef
	return *BufferUAVRef;
}

FString FNeuralTensor::ToString(const int64 InMaxNumberElementsToDisplay, const bool bInReturnOnlyData) const
{
	FString TensorString;
	if (!bInReturnOnlyData)
	{
		TensorString += (Name.Len() > 0 ? Name : FString(TEXT("Unnamed FNeuralTensor"))) + TEXT(": ")
			// DataType
			+ FNeuralDataTypeUtils::ToString(DataType) + TEXT(", ")
			+ (TensorType == ENeuralTensorType::Generic ? TEXT("Generic")
				: TensorType == ENeuralTensorType::Input ? TEXT("Input")
				: TensorType == ENeuralTensorType::IntermediateNotInitialized ? TEXT("IntermediateNotInitialized")
				: TensorType == ENeuralTensorType::IntermediateInitialized ? TEXT("IntermediateInitialized")
				: TensorType == ENeuralTensorType::Output ? TEXT("Output")
				: TensorType == ENeuralTensorType::Weight ? TEXT("Weight")
				: TEXT("Unknown"))
			// Volume and sizes
			+ TEXT(", volume=") + FString::FromInt(Num()) + TEXT(", sizes={");
		// Add sizes
		if (Sizes.Num() > 0)
		{
			for (const int64 Size : Sizes)
			{
				TensorString += FString::FromInt(Size) + TEXT(" ");
			}
			TensorString[TensorString.Len() - 1] = '}';
		}
		else
		{
			TensorString += TEXT("}");

		}
		TensorString += TEXT(", data=[");
	}
	else
	{
		TensorString += TEXT("[");
	}
	// Add tensor data
	if (Num() > 0)
	{
		if (InMaxNumberElementsToDisplay < 1 || Num() <= InMaxNumberElementsToDisplay)
		{
			// 1D
			if (GetNumberDimensions() == 1)
			{
				// Eg for sizes{ 1, 2 }: [20 10 9 2]
				FPrivateNeuralTensor::ArrayToSanitizedString(TensorString, 0, Num(), 0, DataType, *this);
				TensorString[TensorString.Len() - 1] = ']';
			}
			// Eg for sizes {1, 2}: [[20 10] [9 2]]
			else
			{
				// Add initial brackets '['
				for (int64 Index = 0; Index < GetNumberDimensions() - 1; ++Index)
				{
					TensorString += TEXT("[");
				}
				// Add text
				const int64 Stride = Sizes.Last();
				const int64 NumberRows = Num() / Stride;
				for (int64 StrideIndex = 0; StrideIndex < NumberRows; ++StrideIndex) //0, 1, 2
				{
					FPrivateNeuralTensor::ArrayToSanitizedString(TensorString, 0, Stride, StrideIndex * Stride, DataType, *this);
					// ']' for last dimension
					TensorString[TensorString.Len() - 1] = ']';
					int64 NumberBracketsClosed = 1;
					// Extra ']' for additional dimensions
					int64 Value = 1;
					const int64 NextStrideIndex = StrideIndex + 1;
					for (int32 RemainderIndex = Sizes.Num() - 2; RemainderIndex > -1; --RemainderIndex)
					{
						Value *= Sizes[RemainderIndex];
						if (NextStrideIndex % Value == 0)
						{
							++NumberBracketsClosed;
							TensorString += TEXT("]");
						}
						else
						{
							break;
						}
					}
					// Extra '[' for following dimensions (unless we are in the last element)
					if (NextStrideIndex < NumberRows)
					{
						TensorString += TEXT(", ");
						for (int64 BracketIndex = 0; BracketIndex < NumberBracketsClosed; ++BracketIndex)
						{
							TensorString += TEXT("[");
						}
					}
				}
			}
		}
		// Display exactly InMaxNumberElementsToDisplay components
		else
		{
			// Display first InMaxNumberElementsToDisplay/2 components
			FPrivateNeuralTensor::ArrayToSanitizedString(TensorString, 0, InMaxNumberElementsToDisplay / 2, 0, DataType, *this);
			TensorString += TEXT("... ");
			// Display last InMaxNumberElementsToDisplay/2 components
			FPrivateNeuralTensor::ArrayToSanitizedString(TensorString, Num() - InMaxNumberElementsToDisplay / 2, Num(), 0, DataType, *this);
			TensorString[TensorString.Len() - 1] = ']';
		}
	}
	else
	{
		TensorString += TEXT("]");
	}
	return TensorString;
}



/* FNeuralTensor private functions
 *****************************************************************************/

void FNeuralTensor::SetFromVoidPointerCopy(const void* const InVoidPtr, const int64 InSizeOfT, const int64 InDataSize)
{
	// Sanity checks
	if (Num() != InDataSize || NumInBytes() != InSizeOfT * InDataSize)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::SetFromVoidPointerCopy: Num() == InDataSize failed, %d vs. %d, or"
			" NumInBytes() == sizeof(T) x InDataSize failed, %d vs. %d. If you want to modify the dimensions of FNeuralTensor, call"
			" SetNumUninitialized() first."), *Name, Num(), InDataSize, NumInBytes(), InSizeOfT * InDataSize);
	}
	else
	{
		// Deep copy
		SetFromVoidPointerCopy(InVoidPtr);
	}
}

bool FNeuralTensor::CheckTAndDataTypeEquivalentAuxiliary(const bool bInCheckTAndDataTypeResult, const int64 InSizeOfT) const
{
	const int64 ByteSizeOfDataType = FNeuralDataTypeUtils::GetByteSize(DataType);
	if (!bInCheckTAndDataTypeResult)
	{
		const FString DataTypeString = FNeuralDataTypeUtils::ToString(DataType);
		// sizeof(T) and DataType do not match
		if (ByteSizeOfDataType != InSizeOfT)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::CheckTAndDataTypeEquivalentAuxiliary() failed: DataType = %s, but"
				" sizeof(%s) = %d != sizeof(T) = %d."), *Name, *DataTypeString, *DataTypeString, ByteSizeOfDataType, InSizeOfT);
		}
		// sizeof(T) matches, but not the expected DataType
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::CheckTAndDataTypeEquivalentAuxiliary() failed: DataType = %s, but"
				" used a different DataType with the same sizeof(%s) of %d."), *Name, *DataTypeString, *DataTypeString, InSizeOfT);
		}
	}
	return bInCheckTAndDataTypeResult;
}

void FNeuralTensor::CPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder, const EBufferUsageFlags InBufferUsageFlags,
	const bool bInShouldCopyFromCPU)
{
	// Sanity checks
	if (!bEnableGPU || IsEmpty())
	{
		return;
	}
	checkf(!Name.IsEmpty(), TEXT("FNeuralTensor::CPUToRDGBuilder_RenderThread(): Name cannot be empty."));
	checkf(IsInRenderingThread(), TEXT("FNeuralTensor-%s::CPUToRDGBuilder_RenderThread(): IsInRenderingThread() must be true."), *Name);
	checkf(InOutGraphBuilder, TEXT("FNeuralTensor-%s::CPUToRDGBuilder_RenderThread(): InOutGraphBuilder is nullptr."), *Name);
	// Not SRV-only and not UAV/SRV
	checkf(EnumHasAnyFlags(InBufferUsageFlags, EBufferUsageFlags::ShaderResource|EBufferUsageFlags::UnorderedAccess),
		TEXT("FNeuralTensor-%s::CPUToRDGBuilder_RenderThread(): Unexpected case InBufferUsageFlags = %d."), *Name, (uint32)InBufferUsageFlags);
	// If SRV-only and bInShouldCopyFromCPU == false
	if (!EnumHasAnyFlags(InBufferUsageFlags, EBufferUsageFlags::UnorderedAccess) && !bInShouldCopyFromCPU)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensor-%s::CPUToRDGBuilder_RenderThread(): bInShouldCopyFromCPU must be true for"
			" SRVs (because they cannot be edited). Assumed true."), *Name);
	}

	// Create BufferRef
	FRDGBufferRef BufferRef;
	if (!PooledBuffer)
	{
		FRDGBufferDesc BufferDesc;
		BufferDesc.BytesPerElement = FNeuralDataTypeUtils::GetByteSize(DataType);
		BufferDesc.NumElements = Num();
		BufferDesc.Usage = InBufferUsageFlags | EBufferUsageFlags::VertexBuffer;
		
		BufferRef = bInShouldCopyFromCPU 
			? CreateVertexBuffer(*InOutGraphBuilder, *Name, BufferDesc, UnderlyingUInt8ArrayData.GetData(), NumInBytes(),
				ERDGInitialDataFlags::NoCopy)
			: InOutGraphBuilder->CreateBuffer(BufferDesc, *Name);

		// Recreate PooledBuffer for future runs
		PooledBuffer = MakeShared<TRefCountPtr<FRDGPooledBuffer>>();
		*PooledBuffer = InOutGraphBuilder->ConvertToExternalBuffer(BufferRef);
	}
	else
	{
		BufferRef = InOutGraphBuilder->RegisterExternalBuffer(*PooledBuffer);
		InOutGraphBuilder->QueueBufferUpload(BufferRef, UnderlyingUInt8ArrayData.GetData(), NumInBytes(), ERDGInitialDataFlags::None);
	}

	// Recreate BufferSRVRef
	if (EnumHasAnyFlags(InBufferUsageFlags, EBufferUsageFlags::ShaderResource))
	{
		BufferSRVRef = MakeShared<FRDGBufferSRVRef>(InOutGraphBuilder->CreateSRV(BufferRef, FNeuralDataTypeUtils::GetPixelFormat(DataType)));
	}
	else
	{
		BufferSRVRef.Reset();
	}
	// Recreate BufferUAVRef
	if (EnumHasAnyFlags(InBufferUsageFlags, EBufferUsageFlags::UnorderedAccess))
	{
		BufferUAVRef = MakeShared<FRDGBufferUAVRef>(InOutGraphBuilder->CreateUAV(BufferRef, FNeuralDataTypeUtils::GetPixelFormat(DataType)));
	}
	else
	{
		BufferUAVRef.Reset();
	}
}
