// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

template <bool bWithWaterSelectionSupport>
class TWaterInstanceDataBuffers
{
public:
	static constexpr int32 NumBuffers = bWithWaterSelectionSupport ? 3 : 2;

	TWaterInstanceDataBuffers(int32 InInstanceCount)
	{
		ENQUEUE_RENDER_COMMAND(AllocateWaterInstanceDataBuffer)
		(
			[this, InInstanceCount](FRHICommandListImmediate& RHICmdList)
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("WaterInstanceDataBuffers"));

				int32 SizeInBytes = Align<int32>(InInstanceCount * sizeof(FVector4f), 4 * 1024);

				for (int32 i = 0; i < NumBuffers; ++i)
				{
					Buffer[i] = RHICreateVertexBuffer(SizeInBytes, BUF_Dynamic, CreateInfo);
					BufferMemory[i] = TArrayView<FVector4f>();
				}
			}
		);
	}

	~TWaterInstanceDataBuffers()
	{
		for (int32 i = 0; i < NumBuffers; ++i)
		{
			Buffer[i].SafeRelease();
		}
	}

	void Lock(int32 InInstanceCount)
	{
		for (int32 i = 0; i < NumBuffers; ++i)
		{
			BufferMemory[i] = Lock(InInstanceCount, i);
		}
	}

	void Unlock()
	{
		for (int32 i = 0; i < NumBuffers; ++i)
		{
			Unlock(i);
			BufferMemory[i] = TArrayView<FVector4f>();
		}
	}

	FBufferRHIRef GetBuffer(int32 InBufferID) const
	{
		return Buffer[InBufferID];
	}

	TArrayView<FVector4f> GetBufferMemory(int32 InBufferID) const
	{
		check(!BufferMemory[InBufferID].IsEmpty());
		return BufferMemory[InBufferID];
	}

private:
	TArrayView<FVector4f> Lock(int32 InInstanceCount, int32 InBufferID)
	{
		check(IsInRenderingThread());

		uint32 SizeInBytes = InInstanceCount * sizeof(FVector4f);

		if (SizeInBytes > Buffer[InBufferID]->GetSize())
		{
			Buffer[InBufferID].SafeRelease();

			FRHIResourceCreateInfo CreateInfo(TEXT("WaterInstanceDataBuffers"));

			// Align size in to avoid reallocating for a few differences of instance count
			uint32 AlignedSizeInBytes = Align<uint32>(SizeInBytes, 4 * 1024);

			Buffer[InBufferID] = RHICreateVertexBuffer(AlignedSizeInBytes, BUF_Dynamic, CreateInfo);
		}

		FVector4f* Data = reinterpret_cast<FVector4f*>(RHILockBuffer(Buffer[InBufferID], 0, SizeInBytes, RLM_WriteOnly));
		return TArrayView<FVector4f>(Data, InInstanceCount);
	}

	void Unlock(int32 InBufferID)
	{
		RHIUnlockBuffer(Buffer[InBufferID]);
	}

	FBufferRHIRef Buffer[NumBuffers];
	TArrayView<FVector4f> BufferMemory[NumBuffers];
};
