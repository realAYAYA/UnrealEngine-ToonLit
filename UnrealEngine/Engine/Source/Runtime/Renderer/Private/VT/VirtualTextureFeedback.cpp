// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureFeedback.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#if PLATFORM_WINDOWS
// Use Query objects until RHI has a good fence on D3D11
#define USE_RHI_FENCES 0
#else
#define USE_RHI_FENCES 1
#endif

#if USE_RHI_FENCES

/** Container for GPU fences. */
class FFeedbackGPUFencePool
{
public:
	TArray<FGPUFenceRHIRef> Fences;

	FFeedbackGPUFencePool(int32 NumFences)
	{
		Fences.AddDefaulted(NumFences);
	}

	void InitRHI(FRHICommandListBase& RHICmdList)
	{
	}

	void ReleaseRHI()
	{
		for (int i = 0; i < Fences.Num(); ++i)
		{
			Fences[i].SafeRelease();
		}
	}

	void Allocate(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		if (!Fences[Index])
		{
			Fences[Index] = RHICreateGPUFence(FName(""));
		}
		Fences[Index]->Clear();
	}

	void Write(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		RHICmdList.WriteGPUFence(Fences[Index]);
	}

	bool Poll(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		return Fences[Index]->Poll(RHICmdList.GetGPUMask());
	}

	FGPUFenceRHIRef GetMapFence(int32 Index)
	{
		return Fences[Index];
	}

	void Release(int32 Index)
	{
		Fences[Index].SafeRelease();
	}
};

#else // USE_RHI_FENCES

/** Container for GPU fences. Implemented as GPU Queries. */
class FFeedbackGPUFencePool
{
public:
	FGPUFenceRHIRef DummyFence;
	TArray<FRenderQueryRHIRef> Fences;
	bool bDummyFenceWritten = false;

	FFeedbackGPUFencePool(int32 InSize)
	{
		Fences.AddDefaulted(InSize);
	}

	void InitRHI(FRHICommandListBase&)
	{
		if (!DummyFence.IsValid())
		{
			DummyFence = RHICreateGPUFence(FName());
			bDummyFenceWritten = false;
		}
	}

	void ReleaseRHI()
	{
		for (int i = 0; i < Fences.Num(); ++i)
		{
			if (Fences[i].IsValid())
			{
				Fences[i].SafeRelease();
			}
		}

		DummyFence.SafeRelease();
		bDummyFenceWritten = false;
	}

	void Allocate(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		if (Fences[Index].IsValid())
		{
			Fences[Index].SafeRelease();
		}
		Fences[Index] = GDynamicRHI->RHICreateRenderQuery(RQT_AbsoluteTime);

		if (!bDummyFenceWritten && DummyFence.IsValid())
		{
			// Write dummy fence one time on first Allocate
			// After that we want it to always Poll() true
			RHICmdList.WriteGPUFence(DummyFence);
			bDummyFenceWritten = true;
		}
	}
	
	void Write(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		RHICmdList.EndRenderQuery(Fences[Index]);
	}

	bool Poll(FRHICommandListImmediate& RHICmdList, int32 Index)
	{
		uint64 Dummy;
		return RHIGetRenderQueryResult(Fences[Index], Dummy, false, RHICmdList.GetGPUMask().ToIndex());
	}

	FGPUFenceRHIRef GetMapFence(int32 Index)
	{
		return DummyFence;
	}

	void Release(int32 Index)
	{
		Fences[Index].SafeRelease();
	}
};

#endif // USE_RHI_FENCES


FVirtualTextureFeedback::FVirtualTextureFeedback()
	: NumPending(0)
	, WriteIndex(0)
	, ReadIndex(0)
{
	Fences = new FFeedbackGPUFencePool(MaxTransfers);
}

FVirtualTextureFeedback::~FVirtualTextureFeedback()
{
	delete Fences;
}

void FVirtualTextureFeedback::InitRHI(FRHICommandListBase& RHICmdList)
{
	for (int32 Index = 0; Index < MaxTransfers; ++Index)
	{
		FeedbackItems[Index].StagingBuffer = RHICreateStagingBuffer();
	}

	Fences->InitRHI(RHICmdList);
}

void FVirtualTextureFeedback::ReleaseRHI()
{
	for (int32 Index = 0; Index < MaxTransfers; ++Index)
	{
		FeedbackItems[Index].StagingBuffer.SafeRelease();
	}

	Fences->ReleaseRHI();
}

void FVirtualTextureFeedback::TransferGPUToCPU(FRHICommandListImmediate& RHICmdList, FBufferRHIRef const& Buffer, FVirtualTextureFeedbackBufferDesc const& Desc)
{
	if (NumPending >= MaxTransfers)
	{
		// If we have too many pending transfers, start throwing away the oldest in the ring buffer.
		// We will need to allocate a new fence, since the previous fence will still be set on the old CopyToResolveTarget command (which we will now ignore/discard).
		Fences->Release(ReadIndex);
		NumPending --;
		ReadIndex = (ReadIndex + 1) % MaxTransfers;
	}

	FFeedbackItem& FeedbackItem = FeedbackItems[WriteIndex];
	FeedbackItem.Desc = Desc;

	// We only need to transfer 1 copy of the data, so restrict mask to the first active GPU.
	FeedbackItem.GPUMask = FRHIGPUMask::FromIndex(RHICmdList.GetGPUMask().GetFirstIndex());
	SCOPED_GPU_MASK(RHICmdList, FeedbackItem.GPUMask);

	RHICmdList.CopyToStagingBuffer(Buffer, FeedbackItem.StagingBuffer, 0, Desc.BufferSize.X * Desc.BufferSize.Y * sizeof(uint32));

	Fences->Allocate(RHICmdList, WriteIndex);
	Fences->Write(RHICmdList, WriteIndex);

	// Increment the ring buffer write position.
	WriteIndex = (WriteIndex + 1) % MaxTransfers;
	++NumPending;
}

BEGIN_SHADER_PARAMETER_STRUCT(FVirtualTextureFeedbackCopyParameters, )
	RDG_BUFFER_ACCESS(Input, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

void FVirtualTextureFeedback::TransferGPUToCPU(FRDGBuilder& GraphBuilder, FRDGBuffer* Buffer, FVirtualTextureFeedbackBufferDesc const& Desc)
{
	FVirtualTextureFeedbackCopyParameters* Parameters = GraphBuilder.AllocParameters<FVirtualTextureFeedbackCopyParameters>();
	Parameters->Input = Buffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("VirtualTextureFeedbackCopy"),
		Parameters,
		ERDGPassFlags::Readback,
		[this, Buffer, Desc](FRHICommandListImmediate& InRHICmdList)
	{
		TransferGPUToCPU(InRHICmdList, Buffer->GetRHI(), Desc);
	});
}

bool FVirtualTextureFeedback::CanMap(FRHICommandListImmediate& RHICmdList)
{
	if (NumPending > 0u)
	{
		SCOPED_GPU_MASK(RHICmdList, FeedbackItems[ReadIndex].GPUMask);
		return Fences->Poll(RHICmdList, ReadIndex);
	}
	else
	{
		return false;
	}
}

FVirtualTextureFeedback::FMapResult FVirtualTextureFeedback::Map(FRHICommandListImmediate& RHICmdList, int32 MaxTransfersToMap)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_VirtualTextureFeedback_Map);

	FVirtualTextureFeedback::FMapResult MapResult;

	// Calculate number and size of available results.
	int32 NumResults = 0;
	int32 NumRects = 0;
	int32 TotalReadSize = 0;
	for (int32 ResultIndex = 0; ResultIndex < MaxTransfersToMap && ResultIndex < NumPending; ++ResultIndex)
	{
		const int32 FeedbackIndex = (ReadIndex + ResultIndex) % MaxTransfers;
		FVirtualTextureFeedbackBufferDesc const& FeedbackItemDesc = FeedbackItems[FeedbackIndex].Desc;

		SCOPED_GPU_MASK(RHICmdList, FeedbackItems[FeedbackIndex].GPUMask);
		if (!Fences->Poll(RHICmdList, FeedbackIndex))
		{
			break;
		}

		NumResults ++;
		NumRects += FeedbackItemDesc.NumRects;
		TotalReadSize += FeedbackItemDesc.TotalReadSize;
	}

	// Fetch the valid results.
	if (NumResults > 0)
	{
		// Get a FMapResources object to store anything that will need cleaning up on Unmap()
		MapResult.MapHandle = FreeMapResources.Num() ? FreeMapResources.Pop() : MapResources.AddDefaulted();

		if (NumResults == 1 && NumRects == 0)
		{
			// If only one target with no rectangles then fast path is to return the locked buffer.
			const int32 FeedbackIndex = ReadIndex;
			FVirtualTextureFeedbackBufferDesc const& FeedbackItemDesc = FeedbackItems[FeedbackIndex].Desc;
			FRHIGPUMask GPUMask = FeedbackItems[FeedbackIndex].GPUMask;
			FStagingBufferRHIRef StagingBuffer = FeedbackItems[FeedbackIndex].StagingBuffer;
			
			SCOPED_GPU_MASK(RHICmdList, GPUMask);
			const int32 BufferSize = FeedbackItemDesc.BufferSize.X * FeedbackItemDesc.BufferSize.Y;
			MapResult.Data = (uint32*)RHICmdList.LockStagingBuffer(StagingBuffer, Fences->GetMapFence(FeedbackIndex), 0, BufferSize * sizeof(uint32));
			MapResult.Size = BufferSize;

			// Store index so that we can unlock staging buffer when we call Unmap().
			MapResources[MapResult.MapHandle].FeedbackItemToUnlockIndex = FeedbackIndex;
		}
		else
		{
			// Concatenate the results to a single buffer (stored in the MapResources) and return that.
			MapResources[MapResult.MapHandle].ResultData.SetNumUninitialized(TotalReadSize, EAllowShrinking::No);
			MapResult.Data = MapResources[MapResult.MapHandle].ResultData.GetData();
			MapResult.Size = 0;

			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				const int32 FeedbackIndex = (ReadIndex + ResultIndex) % MaxTransfers;
				FVirtualTextureFeedbackBufferDesc const& FeedbackItemDesc = FeedbackItems[FeedbackIndex].Desc;
				FRHIGPUMask GPUMask = FeedbackItems[FeedbackIndex].GPUMask;
				FStagingBufferRHIRef StagingBuffer = FeedbackItems[FeedbackIndex].StagingBuffer;

				SCOPED_GPU_MASK(RHICmdList, GPUMask);
				const int32 BufferSize = FeedbackItemDesc.BufferSize.X * FeedbackItemDesc.BufferSize.Y;
				uint32 const* Data = (uint32*)RHICmdList.LockStagingBuffer(StagingBuffer, Fences->GetMapFence(FeedbackIndex), 0, BufferSize * sizeof(uint32));

				if (FeedbackItemDesc.NumRects == 0)
				{
					// Copy full buffer
					FMemory::Memcpy(MapResult.Data + MapResult.Size, Data, BufferSize * sizeof(uint32));
					MapResult.Size += BufferSize;
				}
				else
				{
					// Copy individual rectangles from the buffer
					const int32 BufferWidth = FeedbackItemDesc.BufferSize.X;
					for (int32 RectIndex = 0; RectIndex < FeedbackItemDesc.NumRects; ++RectIndex)
					{
						const FIntRect Rect = FeedbackItemDesc.Rects[RectIndex];
						const int32 RectWidth = Rect.Width();
						const int32 RectHeight = Rect.Height();

						uint32 const* Src = Data + Rect.Min.Y * BufferWidth + Rect.Min.X;
						uint32* Dst = MapResult.Data + MapResult.Size;

						for (int32 y = 0; y < RectHeight; ++y)
						{
							FMemory::Memcpy(Dst, Src, RectWidth * sizeof(uint32));

							Src += BufferWidth;
							Dst += RectWidth;
						}

						MapResult.Size += RectWidth * RectHeight;
					}
				}

				RHICmdList.UnlockStagingBuffer(StagingBuffer);
			}
		}

		check(MapResult.Size == TotalReadSize)
	
		// Increment the ring buffer read position.
		NumPending -= NumResults;
		ReadIndex = (ReadIndex + NumResults) % MaxTransfers;
	}

	return MapResult;
}

FVirtualTextureFeedback::FMapResult FVirtualTextureFeedback::Map(FRHICommandListImmediate& RHICmdList)
{
	return Map(RHICmdList, MaxTransfers);
}

void FVirtualTextureFeedback::Unmap(FRHICommandListImmediate& RHICmdList, int32 MapHandle)
{
	if (MapHandle >= 0)
	{
		FMapResources& Resources = MapResources[MapHandle];

		// Do any required buffer Unlock.
		if (Resources.FeedbackItemToUnlockIndex >= 0)
		{
			SCOPED_GPU_MASK(RHICmdList, FeedbackItems[Resources.FeedbackItemToUnlockIndex].GPUMask);
			RHICmdList.UnlockStagingBuffer(FeedbackItems[Resources.FeedbackItemToUnlockIndex].StagingBuffer);
			Resources.FeedbackItemToUnlockIndex = -1;
		}

		// Reset any allocated data buffer.
		Resources.ResultData.Reset();

		// Return to free list.
		FreeMapResources.Add(MapHandle);
	}
}

TGlobalResource< FVirtualTextureFeedback > GVirtualTextureFeedback;
