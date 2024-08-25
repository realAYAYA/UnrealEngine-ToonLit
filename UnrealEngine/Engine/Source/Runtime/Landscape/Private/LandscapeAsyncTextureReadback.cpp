// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeAsyncTextureReadback.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

void FLandscapeAsyncTextureReadback::StartReadback_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef RDGTexture)
{
	check(!bStartedOnRenderThread && !AsyncReadback);
	check(RDGTexture->Desc.Format == PF_B8G8R8A8);
	AsyncReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("LandscapeGrassReadback"));
	AddEnqueueCopyPass(GraphBuilder, AsyncReadback.Get(), RDGTexture);
	FIntVector Size = RDGTexture->Desc.GetSize();
	TextureWidth = Size.X;
	TextureHeight = Size.Y;
	check(Size.Z == 1);

	bStartedOnRenderThread = true;
}

void FLandscapeAsyncTextureReadback::FinishReadback_RenderThread()
{
	check(bStartedOnRenderThread && AsyncReadback.IsValid());
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	void* SrcData = AsyncReadback->Lock(RowPitchInPixels, &BufferHeight);	// this will block if the readback is not yet ready
	check(SrcData);
	check(RowPitchInPixels >= TextureWidth);
	check(BufferHeight >= TextureHeight);

	if (!bCancel)	// skip the copy work if we're cancelling
	{
		// copy into ReadbackResults
		ReadbackResults.SetNumUninitialized(TextureWidth * TextureHeight);

		// OpenGL does not really support BGRA images and uses channnel swizzling to emulate them
		// so when we read them back we get internal RGBA representation
		const bool bSwapRBChannels = IsOpenGLPlatform(GMaxRHIShaderPlatform);

		if (!bSwapRBChannels && TextureWidth == RowPitchInPixels)
		{
			memcpy(ReadbackResults.GetData(), SrcData, TextureWidth * TextureHeight * sizeof(FColor));
		}
		else
		{
			// copy row by row
			FColor* Dst = ReadbackResults.GetData();
			FColor* Src = (FColor*)SrcData;
			if (bSwapRBChannels)
			{
				for (int y = 0; y < TextureHeight; y++)
				{
					for (int x = 0; x < TextureWidth; x++)
					{
						// swap B and R channels when copying
						Dst->B = Src->R;
						Dst->G = Src->G;
						Dst->R = Src->B;
						Dst->A = Src->A;
						Dst++;
						Src++;
					}
					Src += RowPitchInPixels - TextureWidth;
				}
			}
			else
			{
				for (int y = 0; y < TextureHeight; y++)
				{
					memcpy(Dst, Src, TextureWidth * sizeof(FColor));
					Dst += TextureWidth;
					Src += RowPitchInPixels;
				}
			}
		}
	}

	AsyncReadback->Unlock();
	AsyncReadback.Reset();

	FPlatformMisc::MemoryBarrier();
	bFinishedOnRenderThread = true;
}

bool FLandscapeAsyncTextureReadback::CheckAndUpdate(bool& bOutFinishCommandQueued, const bool bInForceFinish)
{
	// if we already queued the finish commands to render thread, then we're just waiting on it signaling readback complete	
	if (bFinishQueuedFromGameThread)
	{
		return bFinishedOnRenderThread;
	}
	
	// if we haven't started, or if the readback is not yet ready, then we have nothing to do but wait
	if (!bStartedOnRenderThread || (!bInForceFinish && !AsyncReadback->IsReady()))
	{
		return false;
	}

	// the readback was started and it is ready, but we have not yet queued the finish command.
	// queue it to make the data available to the game thread
	FLandscapeAsyncTextureReadback* Readback = this;
	ENQUEUE_RENDER_COMMAND(FLandscapeAsyncTextureReadback_FinishReadback)(
		[Readback, bInForceFinish](FRHICommandListImmediate& RHICmdList)
		{
			// sanity check the state 
			check(Readback->bStartedOnRenderThread && !Readback->bFinishedOnRenderThread);
			check(Readback->AsyncReadback.IsValid() && (bInForceFinish || Readback->AsyncReadback->IsReady()));
			Readback->FinishReadback_RenderThread();
		});

	bFinishQueuedFromGameThread = true;
	bOutFinishCommandQueued = true;

	return false;
}

void FLandscapeAsyncTextureReadback::CancelAndSelfDestruct()
{
	// set the cancel flag, which will reduce work done by the finish command
	bCancel = true;

	// run a render thread finish command if it hasn't been queued  yet
	bool bNeedsFinish = !bFinishQueuedFromGameThread;
	bFinishQueuedFromGameThread = true;

	FLandscapeAsyncTextureReadback* Readback = this;
	ENQUEUE_RENDER_COMMAND(FCancelAndSelfDestructCommand)(
		[Readback, bNeedsFinish](FRHICommandListImmediate& RHICmdList)
		{
			check(Readback->bStartedOnRenderThread);
			check(Readback->bCancel);

			// if not yet finished, run the finish command
			if (bNeedsFinish)
			{
				Readback->FinishReadback_RenderThread();
			}

			check(Readback->bFinishedOnRenderThread);

			// self destruct
			delete Readback;
		});
}


void FLandscapeAsyncTextureReadback::QueueDeletionFromGameThread()
{
	check(IsInGameThread());
	check(bFinishedOnRenderThread);

	FLandscapeAsyncTextureReadback* Readback = this;
	ENQUEUE_RENDER_COMMAND(FLandscapeAsyncTextureReadback_CheckAndUpdate)(
		[Readback](FRHICommandListImmediate& RHICmdList)
		{
			delete Readback;
		});
}
