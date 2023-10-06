// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.cpp: Stream in helper for 2D textures using texture streaming files.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_IO.h"
#include "HAL/PlatformFile.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "ContentStreaming.h"
#include "Rendering/Texture2DResource.h"
#include "Streaming/Texture2DStreamIn.h"
#include "Streaming/Texture2DUpdate.h"

#if PLATFORM_ANDROID
#include "EngineLogs.h"
#include "RenderUtils.h"
#endif

FTexture2DStreamIn_IO::FTexture2DStreamIn_IO(UTexture2D* InTexture, bool InPrioritizedIORequest)
	: FTexture2DStreamIn(InTexture)
	, bPrioritizedIORequest(InPrioritizedIORequest)

{
	IORequests.AddZeroed(ResourceState.MaxNumLODs);
}

FTexture2DStreamIn_IO::~FTexture2DStreamIn_IO()
{
#if DO_CHECK
	for (const IBulkDataIORequest* IORequest : IORequests)
	{
		check(!IORequest);
	}
#endif
}

static void ValidateMipBulkDataSize(const UTexture2D& Texture, int32 MipSizeX, int32 MipSizeY, int32 MipIndex, int64& BulkDataSize)
{
	// why is this not done on all platforms?
#if PLATFORM_ANDROID
	const int64 ExpectedMipSize = CalcTextureMipMapSize((uint32)MipSizeX, (uint32)MipSizeY, Texture.GetPixelFormat(), 0);
	if (BulkDataSize != ExpectedMipSize)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTexture, Warning, TEXT("Mip (%d) %dx%d has an unexpected size %lld, expected size %lld. %s, Pixel format %s"), 
			MipIndex, MipSizeX, MipSizeY, BulkDataSize, ExpectedMipSize, *Texture.GetFullName(), GPixelFormats[Texture.GetPixelFormat()].Name);
#endif
		// Make sure we don't overrun buffer allocated for this mip
		BulkDataSize = FMath::Min(BulkDataSize, ExpectedMipSize);
	}
#endif // PLATFORM_ANDROID
}

void FTexture2DStreamIn_IO::SetIORequests(const FContext& Context)
{
	SetAsyncFileCallback();
	
	for (int32 MipIndex = PendingFirstLODIdx; MipIndex < CurrentFirstLODIdx && !IsCancelled(); ++MipIndex)
	{
		const FTexture2DMipMap& MipMap = *Context.MipsView[MipIndex];
		check(MipData[MipIndex].Data != nullptr);

		int64 BulkDataSize = MipMap.BulkData.GetBulkDataSize();
		if (BulkDataSize > 0)
		{
			// Increment as we push the requests. If a requests complete immediately, then it will call the callback
			// but that won't do anything because the tick would not try to acquire the lock since it is already locked.
			TaskSynchronization.Increment();

			// Validate buffer size for the mip, so we don't overrun it on streaming
			// note: MipData[] should have size
			// ValidateMipBulkDataSize only does anything on Android
			ValidateMipBulkDataSize(*Context.Texture, MipMap.SizeX, MipMap.SizeY, MipIndex, BulkDataSize);
			
			// reads directly into MipData[] , doesn't respect Pitch
			// we do get a completion callback at AsyncFileCallBack
			// so in theory could fix Pitch there
			uint32 DestPitch = MipData[MipIndex].Pitch;
			FTexture2DResource::WarnRequiresTightPackedMip(MipMap.SizeX, MipMap.SizeY, Context.Resource->GetPixelFormat(), DestPitch);

			EAsyncIOPriorityAndFlags Priority = AIOP_Low;
			if (bPrioritizedIORequest)
			{
				static IConsoleVariable* CVarAsyncLoadingPrecachePriority = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingPrecachePriority"));
				const bool bLoadBeforeAsyncPrecache = CVarStreamingLowResHandlingMode.GetValueOnAnyThread() == (int32)FRenderAssetStreamingSettings::LRHM_LoadBeforeAsyncPrecache;

				if (CVarAsyncLoadingPrecachePriority && bLoadBeforeAsyncPrecache)
				{
					const int32 AsyncIOPriority = CVarAsyncLoadingPrecachePriority->GetInt();
					// Higher priority than regular requests but don't go over max
					Priority = (EAsyncIOPriorityAndFlags)FMath::Clamp<int32>(AsyncIOPriority + 1, AIOP_BelowNormal, AIOP_MAX);
				}
				else
				{
					Priority = AIOP_BelowNormal;
				}
			}

			IORequests[MipIndex] = MipMap.BulkData.CreateStreamingRequest(
				0,
				BulkDataSize,
				Priority | AIOP_FLAG_DONTCACHE,
				&AsyncFileCallBack,
				(uint8*)MipData[MipIndex].Data);
		}
		else // Bulk data size can only be 0 when not available, in which case, we need to recache the file state.
		{
			bFailedOnIOError = true;
			MarkAsCancelled();
			break;
		}
	}
}

void FTexture2DStreamIn_IO::CancelIORequests()
{
	for (int32 MipIndex = 0; MipIndex < IORequests.Num(); ++MipIndex)
	{
		IBulkDataIORequest* IORequest = IORequests[MipIndex];
		if (IORequest)
		{
			// Calling cancel will trigger the SetAsyncFileCallback() which will also try a tick but will fail.
			IORequest->Cancel();
		}
	}
}

void FTexture2DStreamIn_IO::ClearIORequests(const FContext& Context)
{
	for (int32 MipIndex = PendingFirstLODIdx; MipIndex < CurrentFirstLODIdx; ++MipIndex)
	{
		IBulkDataIORequest* IORequest = IORequests[MipIndex];
		IORequests[MipIndex] = nullptr;

		if (IORequest != nullptr)
		{
			// If clearing requests not yet completed, cancel and wait.
			if (!IORequest->PollCompletion())
			{
				IORequest->Cancel();
				IORequest->WaitCompletion();
			}
			delete IORequest;
		}
	}
}

void FTexture2DStreamIn_IO::ReportIOError(const FContext& Context)
{
	// Invalidate the cache state of all initial mips (note that when using FIoChunkId each mip has a different value).
	if (bFailedOnIOError && Context.Texture)
	{
		IRenderAssetStreamingManager& StreamingManager = IStreamingManager::Get().GetTextureStreamingManager();
		// Need to start at index 0 because the streamer only gets the hash for the first optional mip (and we don't know which one it is).
		for (int32 MipIndex = 0; MipIndex < CurrentFirstLODIdx; ++MipIndex)
		{
			StreamingManager.MarkMountedStateDirty(Context.Texture->GetMipIoFilenameHash(ResourceState.AssetLODBias + MipIndex));
		}
		UE_LOG(LogContentStreaming, Warning, TEXT("[%s] Texture stream in request failed due to IO error (Mip %d-%d)."), *Context.Texture->GetName(), ResourceState.AssetLODBias + PendingFirstLODIdx, ResourceState.AssetLODBias + CurrentFirstLODIdx - 1);
	}
}

void FTexture2DStreamIn_IO::SetAsyncFileCallback()
{
	AsyncFileCallBack = [this](bool bWasCancelled, IBulkDataIORequest*)
	{
		// At this point task synchronization would hold the number of pending requests.
		TaskSynchronization.Decrement();
		
		if (bWasCancelled)
		{
			// If IO requests was cancelled but the streaming request wasn't, this is an IO error.
			if (!bIsCancelled)
			{
				bFailedOnIOError = true;
			}

			MarkAsCancelled();
		}

#if !UE_BUILD_SHIPPING
		// On some platforms the IO is too fast to test cancelation requests timing issues.
		if (FRenderAssetStreamingSettings::ExtraIOLatency > 0 && TaskSynchronization.GetValue() == 0)
		{
			FPlatformProcess::Sleep(FRenderAssetStreamingSettings::ExtraIOLatency * .001f); // Slow down the streaming.
		}
#endif

		// The tick here is intended to schedule the success or cancel callback.
		// Using TT_None ensure gets which could create a dead lock.
		Tick(FTexture2DUpdate::TT_None);
	};
}

void FTexture2DStreamIn_IO::Abort()
{
	if (!IsCancelled() && !IsCompleted())
	{
		FTexture2DStreamIn::Abort();

		if (HasPendingIORequests())
		{
			// Prevent the update from being considered done before this is finished.
			// By checking that it was not already canceled, we make sure this doesn't get called twice.
			(new FAsyncCancelIORequestsTask(this))->StartBackgroundTask();
		}
	}
}

bool FTexture2DStreamIn_IO::HasPendingIORequests()
{
	for (IBulkDataIORequest* IORequest : IORequests)
	{
		if (IORequest != nullptr)
		{
			return true;
		}
	}

	return false;
}

void FTexture2DStreamIn_IO::FCancelIORequestsTask::DoWork()
{
	check(PendingUpdate);
	// Acquire the lock of this object in order to cancel any pending IO.
	// If the object is currently being ticked, wait.
	const ETaskState PreviousTaskState = PendingUpdate->DoLock();
	PendingUpdate->CancelIORequests();
	PendingUpdate->DoUnlock(PreviousTaskState);
}
