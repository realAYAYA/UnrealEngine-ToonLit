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


/** 
 * Helper class to manage a persistent structured buffer.
 */
class FPersistentStructuredBuffer
{
public:
	FPersistentStructuredBuffer(int32 InMinimumNumElementsReserved, const TCHAR *InName, bool bInRoundUpToPOT = true);

	FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, int32 InNewMinNumElements, int32 BytesPerElement);
	FRDGBuffer* Register(FRDGBuilder& GraphBuilder);

	void Empty();
protected:
	int32 MinimumNumElementsReserved = 0;
	const TCHAR *Name = nullptr;
	bool bRoundUpToPOT = true;
	TRefCountPtr<FRDGPooledBuffer> PooledBuffer;
};

/**
 * Typed version of FPersistentStructuredBuffer 
 */
template <typename InValueType>
class TPersistentStructuredBuffer : public FPersistentStructuredBuffer
{
public:
	using ValueType = InValueType;
	TPersistentStructuredBuffer(int32 InMinimumNumElementsReserved, const TCHAR *InName, bool bInRoundUpToPOT = true)
	: FPersistentStructuredBuffer(InMinimumNumElementsReserved, InName, bInRoundUpToPOT)
	{
	}

	FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, int32 InNewMinNumElements)
	{
		return FPersistentStructuredBuffer::ResizeBufferIfNeeded(GraphBuilder, InNewMinNumElements, sizeof(ValueType));
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

class FStructuredBufferScatterUploader
{
public:
	void UploadTo(FRDGBuilder& GraphBuilder, FRDGBuffer *DestBuffer, FRDGBuffer *ScatterOffsets, FRDGBuffer *Values, uint32 NumScatters, uint32 NumBytesPerElement, uint32 NumValuesPerScatter);
};

template <typename InValueType, int32 InNumValuesPerScatter = 1>
class TStructuredBufferScatterUploader : public FStructuredBufferScatterUploader
{
public:
	using ValueType = InValueType;
	static constexpr int32 BytesPerElement = sizeof(ValueType);
	static constexpr int32 NumValuesPerScatter = InNumValuesPerScatter;

	/**
	 * Optionally reserve space for NumScatters items.
	 */
	TStructuredBufferScatterUploader(int32 NumScatters = 0)
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

	int32 GetNumScatters() const { return UploadDataProxy != nullptr ? UploadDataProxy->ScatterOffsets.Num() : UploadData.ScatterOffsets.Num(); }

	/**
	 * Resize the destination persistent buffer (if needed) and upload & scatter the collected data to it.
	 * This locks the uploader to prevent accidental resize (and thus realloc) of the buffer by adding more elements.
	 */
	FRDGBuffer *ResizeAndUploadTo(FRDGBuilder& GraphBuilder, TPersistentStructuredBuffer<ValueType> &DestDataBuffer, int32 DestDataMinimumSize)
	{
		check(UploadDataProxy == nullptr);

		FRDGBuffer *DestBufferRDG = DestDataBuffer.ResizeBufferIfNeeded(GraphBuilder, DestDataMinimumSize);

		uint32 NumScatters = UploadData.ScatterOffsets.Num();
		if (NumScatters != 0u)
		{
			// Move the data arrays to a proxy object owned by RDG to guarantee life-times for upload.
			UploadDataProxy = GraphBuilder.AllocObject<FUploadData>(MoveTemp(UploadData));

			// upload the values & offsets
			FRDGBuffer *ScatterOffsetsRDG = CreateStructuredUploadBuffer(GraphBuilder, TEXT("ScatterUploader.Offsets"), UploadDataProxy->ScatterOffsets, ERDGInitialDataFlags::NoCopy);
			FRDGBuffer *ValuesRDG = CreateStructuredUploadBuffer(GraphBuilder, TEXT("ScatterUploader.Values"), UploadDataProxy->Values, ERDGInitialDataFlags::NoCopy);

			FStructuredBufferScatterUploader::UploadTo(GraphBuilder, DestBufferRDG, ScatterOffsetsRDG, ValuesRDG, NumScatters, sizeof(ValueType), NumValuesPerScatter);
		}

		return DestBufferRDG;
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

