// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderGraphUtils.h"

class FTileTexCoordVertexBuffer : public FVertexBuffer
{
public:
	FTileTexCoordVertexBuffer(int32 InNumTileQuadsInBuffer)
		: NumTileQuadsInBuffer(InNumTileQuadsInBuffer)
	{
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const uint32 Size = sizeof(FVector2f) * 4 * NumTileQuadsInBuffer;
		FRHIResourceCreateInfo CreateInfo(TEXT("FTileTexCoordVertexBuffer"));
		VertexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector2f* Vertices = (FVector2f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, Size, RLM_WriteOnly);
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Vertices[SpriteIndex * 4 + 0] = FVector2f(0.0f, 0.0f);
			Vertices[SpriteIndex * 4 + 1] = FVector2f(0.0f, 1.0f);
			Vertices[SpriteIndex * 4 + 2] = FVector2f(1.0f, 1.0f);
			Vertices[SpriteIndex * 4 + 3] = FVector2f(1.0f, 0.0f);
		}
		RHICmdList.UnlockBuffer(VertexBufferRHI);
	}

	const uint32 NumTileQuadsInBuffer;
};

class FTileIndexBuffer : public FIndexBuffer
{
public:
	FTileIndexBuffer(int32 InNumTileQuadsInBuffer)
		: NumTileQuadsInBuffer(InNumTileQuadsInBuffer)
	{
	}

	/** Initialize the RHI for this rendering resource */
	void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const uint32 Size = sizeof(uint16) * 6 * NumTileQuadsInBuffer;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo(TEXT("FTileIndexBuffer"));
		IndexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint16* Indices = (uint16*)RHICmdList.LockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Indices[SpriteIndex * 6 + 0] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 1] = SpriteIndex * 4 + 1;
			Indices[SpriteIndex * 6 + 2] = SpriteIndex * 4 + 2;
			Indices[SpriteIndex * 6 + 3] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 4] = SpriteIndex * 4 + 2;
			Indices[SpriteIndex * 6 + 5] = SpriteIndex * 4 + 3;
		}
		RHICmdList.UnlockBuffer(IndexBufferRHI);
	}

	const uint32 NumTileQuadsInBuffer;
};

/** One Tile Quad Vertex Buffer*/
RENDERER_API FBufferRHIRef& GetOneTileQuadVertexBuffer();
/** One Tile Quad Index Buffer*/
RENDERER_API FBufferRHIRef& GetOneTileQuadIndexBuffer();

template <typename ReadbackProcessingLambdaType>
void AddBufferLockReadbackPass(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer> SourceBuffer, uint32 NumBytes, ReadbackProcessingLambdaType &&ReadbackProcessingLambda)
{
	FRHIBuffer* SourceBufferRHI = SourceBuffer->GetRHI();
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("BufferLockReadbackPass"),
		ERDGPassFlags::None,
		[ReadbackProcessingLambdaType=MoveTemp(ReadbackProcessingLambda), SourceBufferRHI, NumBytes](FRHICommandListImmediate& RHICmdList)
	{
		const void *GPUData = (uint32*)RHICmdList.LockBuffer(SourceBufferRHI, 0, NumBytes, RLM_ReadOnly);
		ReadbackProcessingLambdaType(GPUData);
		RHICmdList.UnlockBuffer(SourceBufferRHI);
	});
}

class FBufferScatterUploader
{
public:
	// Used to capture GPU scatter buffer and num, to use for example to chain a compute shader doing some per-updated-thing work
	struct FScatterInfo
	{
		FRDGBuffer *ScatterOffsetsRDG;
		int32 NumScatters;
	};

	void UploadTo(FRDGBuilder& GraphBuilder, FRDGBuffer *DestBuffer, FRDGBuffer *ScatterOffsets, FRDGBuffer *Values, uint32 NumScatters, uint32 NumBytesPerElement, int32 NumValuesPerScatter);
};

namespace UE::RendererPrivateUtils::Implementation
{

/** 
 * Helper class to manage a persistent buffer.
 */
class FPersistentBuffer
{
public:
	FPersistentBuffer(int32 InMinimumNumElementsReserved, const TCHAR *InName, bool bInRoundUpToPOT = true);

	FRDGBuffer* Register(FRDGBuilder& GraphBuilder) const;

	void Empty();

protected:

	FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, const FRDGBufferDesc& BufferDesc);

	int32 MinimumNumElementsReserved = 0;
	const TCHAR *Name = nullptr;
	bool bRoundUpToPOT = true;
	TRefCountPtr<FRDGPooledBuffer> PooledBuffer;
};

struct FStructuredBufferTraits
{
	template <typename ElementType, typename AllocatorType>
	static FRDGBuffer *CreateUploadBuffer(FRDGBuilder& GraphBuilder, const TCHAR* Name, const TArray<ElementType, AllocatorType> &InitialData)
	{
		return CreateStructuredUploadBuffer(GraphBuilder, Name, InitialData, ERDGInitialDataFlags::NoCopy);
	}

	static FRDGBufferDesc CreateDesc(uint32 BytesPerElement, uint32 NumElements)
	{
		return FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, NumElements);	
	}

	static constexpr bool bAutoValuesPerScatter = false;
};

struct FByteAddressBufferTraits
{
	template <typename ElementType, typename AllocatorType>
	static FRDGBuffer *CreateUploadBuffer(FRDGBuilder& GraphBuilder, const TCHAR* Name, const TArray<ElementType, AllocatorType> &InitialData)
	{
		return CreateByteAddressUploadBuffer(GraphBuilder, Name, InitialData, ERDGInitialDataFlags::NoCopy);
	}

	static FRDGBufferDesc CreateDesc(uint32 BytesPerElement, uint32 NumElements)
	{
		uint32 NumBytes = BytesPerElement * NumElements;
		// Needs to be aligned to 16 bytes to MemcpyResource to work correctly (otherwise it skips last unaligned elements of the buffer during resize)
		check((NumBytes & 15) == 0);
		return FRDGBufferDesc::CreateByteAddressDesc(NumBytes);	
	}

	static constexpr bool bAutoValuesPerScatter = true;
};


/**
 * Typed version of FPersistentStructuredBuffer 
 */
template <typename InValueType, typename InBufferTraits>
class TPersistentBuffer : public FPersistentBuffer
{
public:
	using ValueType = InValueType;
	using BufferTraits = InBufferTraits;

	static constexpr uint32 BytesPerElement = sizeof(ValueType);

	TPersistentBuffer(int32 InMinimumNumElementsReserved, const TCHAR *InName, bool bInRoundUpToPOT = true)
	: FPersistentBuffer(InMinimumNumElementsReserved, InName, bInRoundUpToPOT)
	{
	}

	FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, int32 InNewMinNumElements)
	{
		int32 NewMinNumElements = FMath::Max(MinimumNumElementsReserved, bRoundUpToPOT ? int32(FMath::RoundUpToPowerOfTwo(InNewMinNumElements)) : InNewMinNumElements);
		return FPersistentBuffer::ResizeBufferIfNeeded(GraphBuilder, BufferTraits::CreateDesc(BytesPerElement, NewMinNumElements));
	}	

	template <typename ValueCheckFuncType>
	void ValidateGPUData(FRDGBuilder& GraphBuilder, TConstArrayView<ValueType> HostValues, ValueCheckFuncType &&ValueCheckFunc)
	{
#if DO_CHECK
		check(HostValues.Num() == 0 || PooledBuffer.IsValid() && HostValues.Num() <= int32(PooledBuffer->Desc.NumElements));

		if (!HostValues.IsEmpty())
		{
			// TODO: should perhaps copy the host values? A lot of overhead but very useful to be sure they are alive...
			AddBufferLockReadbackPass(GraphBuilder, PooledBuffer, uint32(HostValues.GetTypeSize() * HostValues.Num()), [ValueCheckFunc=MoveTemp(ValueCheckFunc), HostValues](const void *LockedData)
			{
				const ValueType *GPUValuePtr = reinterpret_cast<const ValueType*>(LockedData);
				for (int32 Index = 0; Index < HostValues.Num(); ++Index)
				{
					ValueCheckFunc(Index, HostValues[Index], GPUValuePtr[Index]);
				}
			});
		}
#endif
	}

	TRefCountPtr<FRDGPooledBuffer> &GetPooledBuffer() { return PooledBuffer; }
};

template <typename InValueType, typename InBufferTraits,  int32 InNumValuesPerScatter = 1>
class TBufferScatterUploader : public FBufferScatterUploader
{
public:
	using BufferTraits = InBufferTraits;
	using ValueType = InValueType;
	static constexpr int32 BytesPerElement = sizeof(ValueType);
	static constexpr int32 NumValuesPerScatter = InNumValuesPerScatter;

	static_assert((BytesPerElement % 4) == 0, "The struct used must be 4-byte aligned");

	/**
	 * Optionally reserve space for NumScatters items.
	 */
	TBufferScatterUploader(int32 NumScatters = 0)
	{
		Reserve(NumScatters);
	}

	/**
	 * Pre-size the value and scatter arrays, allocates space for InNumValuesPerScatter * NumScatters values & NumScatters scatter offsets.
	 */
	void Reserve(int32 NumScatters)
	{
		check(UploadDataProxy == nullptr);

		UploadData.Values.Reserve(NumScatters * NumValuesPerScatter);
		UploadData.ScatterOffsets.Reserve(NumScatters);
	}

	/**
	 * Add single value to scatter to the destination offset.
	 */
	void Add(const ValueType &Value, int32 ScatterOffset)
	{
		check(1 == NumValuesPerScatter);
		check(UploadDataProxy == nullptr);

		UploadData.Values.Add(Value);
		UploadData.ScatterOffsets.Add(ScatterOffset);
	}

	/**
	 * Add a number of values to scatter to a common destination start offset, 
	 * NOTE: the destination start offset is ScatterOffset * InNumValuesPerScatter (not ScatterOffset)
	 */
	void Add(const TConstArrayView<ValueType> &InValues, int32 ScatterOffset)
	{
		check(InValues.Num() == NumValuesPerScatter);
		check(UploadDataProxy == nullptr);

		UploadData.Values.Append(InValues);
		UploadData.ScatterOffsets.Add(ScatterOffset);
	}

	/**
	 * Add a number of values to scatter to consecutive destination offsets with a common start offset, 
	 * NOTE: this allocates a new scatter offset for each NumValuesPerScatter elements.
	 */
	void AddMultiple(const TConstArrayView<ValueType> &InValues, int32 FirstScatterOffset)
	{
		check(InValues.Num() % NumValuesPerScatter == 0);
		check(UploadDataProxy == nullptr);

		UploadData.Values.Append(InValues);
		for (int32 Index = 0; Index < InValues.Num() / NumValuesPerScatter; ++Index)
		{
			UploadData.ScatterOffsets.Add(FirstScatterOffset + Index);
		}
	}

	/**
	 * Add a number of values to scatter to consecutive destination offsets with a common start offset, 
	 * NOTE: this allocates a new scatter offset for each NumValuesPerScatter elements.
	 */
	TArrayView<ValueType> AddMultiple_GetRef(int32 FirstScatterOffset, int32 NumValues)
	{
		check(NumValues % NumValuesPerScatter == 0);
		check(UploadDataProxy == nullptr);

		int32 SrcOffset = UploadData.Values.Num();
		UploadData.Values.AddUninitialized(NumValues);
		for (int32 Index = 0; Index < NumValues / NumValuesPerScatter; ++Index)
		{
			UploadData.ScatterOffsets.Add(FirstScatterOffset + Index);
		}

		return MakeArrayView(&UploadData.Values[SrcOffset], NumValues);
	}

	int32 GetNumScatters() const { return UploadDataProxy != nullptr ? UploadDataProxy->ScatterOffsets.Num() : UploadData.ScatterOffsets.Num(); }

	/**
	 * Resize the destination persistent buffer (if needed) and upload & scatter the collected data to it.
	 * This locks the uploader to prevent accidental resize (and thus realloc) of the buffer by adding more elements.
	 */
	FRDGBuffer *ResizeAndUploadTo(FRDGBuilder& GraphBuilder, TPersistentBuffer<ValueType, BufferTraits> &DestDataBuffer, int32 DestDataMinimumSize, FScatterInfo &OutScatterInfo)
	{
		check(UploadDataProxy == nullptr);

		FRDGBuffer *DestBufferRDG = DestDataBuffer.ResizeBufferIfNeeded(GraphBuilder, DestDataMinimumSize);

		OutScatterInfo.NumScatters = UploadData.ScatterOffsets.Num();
		OutScatterInfo.ScatterOffsetsRDG = nullptr;
		if (OutScatterInfo.NumScatters != 0u)
		{
			// Move the data arrays to a proxy object owned by RDG to guarantee life-times for upload.
			UploadDataProxy = GraphBuilder.AllocObject<FUploadData>(MoveTemp(UploadData));

			// upload the values & offsets
			OutScatterInfo.ScatterOffsetsRDG = BufferTraits::CreateUploadBuffer(GraphBuilder, TEXT("ScatterUploader.Offsets"), UploadDataProxy->ScatterOffsets);
			FRDGBuffer *ValuesRDG = BufferTraits::CreateUploadBuffer(GraphBuilder, TEXT("ScatterUploader.Values"), UploadDataProxy->Values);

			uint32 ElementSize = sizeof(ValueType);
			uint32 ElementsPerScatter = NumValuesPerScatter;
			if (BufferTraits::bAutoValuesPerScatter)
			{
				// Let the implementation determine the optimal way to divide up the scatter
				ElementSize *= ElementsPerScatter;
				ElementsPerScatter = INDEX_NONE;
			}
			FBufferScatterUploader::UploadTo(GraphBuilder, DestBufferRDG, OutScatterInfo.ScatterOffsetsRDG, ValuesRDG, OutScatterInfo.NumScatters, ElementSize, ElementsPerScatter);
		}

		return DestBufferRDG;
	}
	
	FRDGBuffer *ResizeAndUploadTo(FRDGBuilder& GraphBuilder, TPersistentBuffer<ValueType, BufferTraits> &DestDataBuffer, int32 DestDataMinimumSize)
	{
		FScatterInfo ScatterInfo = { nullptr, 0 };
		return ResizeAndUploadTo(GraphBuilder, DestDataBuffer, DestDataMinimumSize, ScatterInfo);
	}

private:
	struct FUploadData
	{
		TArray<ValueType> Values;
		TArray<uint32> ScatterOffsets;
	};
	FUploadData *UploadDataProxy = nullptr;
	FUploadData UploadData;
};

}

template <typename InValueType>
using TPersistentStructuredBuffer = UE::RendererPrivateUtils::Implementation::TPersistentBuffer<InValueType, UE::RendererPrivateUtils::Implementation::FStructuredBufferTraits>;

template <typename InValueType, int32 InNumValuesPerScatter = 1>
using TStructuredBufferScatterUploader = UE::RendererPrivateUtils::Implementation::TBufferScatterUploader<InValueType, UE::RendererPrivateUtils::Implementation::FStructuredBufferTraits, InNumValuesPerScatter>;

template <typename InValueType>
using TPersistentByteAddressBuffer = UE::RendererPrivateUtils::Implementation::TPersistentBuffer<InValueType, UE::RendererPrivateUtils::Implementation::FByteAddressBufferTraits>;

template <typename InValueType, int32 InNumValuesPerScatter = 1>
using TByteAddressBufferScatterUploader = UE::RendererPrivateUtils::Implementation::TBufferScatterUploader<InValueType, UE::RendererPrivateUtils::Implementation::FByteAddressBufferTraits, InNumValuesPerScatter>;
