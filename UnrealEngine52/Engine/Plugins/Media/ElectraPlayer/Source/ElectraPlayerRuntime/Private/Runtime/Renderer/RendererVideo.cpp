// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/Build.h"
#include <Core/MediaTypes.h>

#include "Renderer/RendererVideo.h"

#include "CoreGlobals.h"
#include "Misc/Timespan.h"
#include "HAL/UnrealMemory.h"

#include "ElectraPlayer.h"
#include "ElectraPlayerPrivate.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Electra::MEDIArendererVideoUE_NumUsedOutputVideoSamples"), STAT_ElectraPlayer_MEDIArendererVideoUE_NumUsedOutputVideoSamples, STATGROUP_ElectraPlayer);

// -----------------------------------------------------------------------------------------------------------------------

void FElectraRendererVideo::SampleReleasedToPool(IDecoderOutput* InDecoderOutput)
{
	FPlatformAtomics::InterlockedDecrement(&NumOutputTexturesInUse);

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> Parent = ParentRenderer.Pin();
	if (Parent.IsValid())
	{
		Parent->SampleReleasedToPool(InDecoderOutput);
	}
}


FElectraRendererVideo::SystemConfiguration::SystemConfiguration()
{
}


bool FElectraRendererVideo::Startup(const FElectraRendererVideo::SystemConfiguration& Configuration)
{
	return true;
}

void FElectraRendererVideo::Shutdown(void)
{
}



FElectraRendererVideo::FElectraRendererVideo(TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> InPlayer)
	: Player(InPlayer)
	, NumOutputTexturesInUse(0)
	, NumBuffers(0)
	, NumBuffersAcquiredForDecoder(0)
{
}

FElectraRendererVideo::~FElectraRendererVideo()
{
	// Manually delete all images in all queues. Well, the queues will are also deleted automatically.
	QueueTickedAndWaitingForDecoder.Empty();
}


FElectraRendererVideo::OpenError FElectraRendererVideo::Open(const FElectraRendererVideo::InstanceConfiguration& Config)
{
	return OpenError::eOpen_Ok;
}


void FElectraRendererVideo::Close(void)
{
}


void FElectraRendererVideo::DetachPlayer()
{
	Player.Reset();
}


void FElectraRendererVideo::PrepareForDecoderShutdown()
{
	DecoderOutputPool.PrepareForDecoderShutdown();
}


const Electra::FParamDict& FElectraRendererVideo::GetBufferPoolProperties() const
{
	return BufferPoolProperties;
}


/**
 * Get some image from the media Pool.
 * Please note that this "pool" is only used for reusing images. There is no upper limit in this pool.
 * For this we have the separate counter
 */
void FElectraRendererVideo::AcquireFromPool(FVideoDecoderOutputPtr& DelayedImage)
{
	// Acquire a decoder output item
	DelayedImage = DecoderOutputPool.AcquireShared();
	DelayedImage->SetOwner(SharedThis(this));

	FPlatformAtomics::InterlockedIncrement(&NumOutputTexturesInUse);
}

/**
 *
 */
UEMediaError FElectraRendererVideo::CreateBufferPool(const Electra::FParamDict& Parameters)
{
	const FVariantValue& variantNumBuffers = Parameters.GetValue("num_buffers");
	if (!variantNumBuffers.IsType(FVariantValue::EDataType::TypeInt64))
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	int32 RequestedNumBuffers = (int32)variantNumBuffers.GetInt64();

	// Update dict for later query via GetBufferPoolProperties
	BufferPoolProperties.Set("max_buffers", Electra::FVariantValue((int64)RequestedNumBuffers));

	// Currently, we only handle enlargement of the buffer pool. If the size should shrink,
	if (RequestedNumBuffers != NumBuffers)
	{
		// Preallocate the buffer structures, but do not do the actual memory allocation!
		// This allows any AcquireBuffer command to immediately use a buffer from the queue here.
		for (int32 i = NumBuffers; i < RequestedNumBuffers; ++i)
		{
			FVideoDecoderOutputPtr DelayedImage;
			AcquireFromPool(DelayedImage);
			QueueTickedAndWaitingForDecoder.Enqueue(DelayedImage);

		}
		NumBuffers = RequestedNumBuffers;
	}
	return UEMEDIA_ERROR_OK;
}


/**
 * Asks for a sample buffer from the buffer pool created previously through CreateBufferPool
 */
UEMediaError FElectraRendererVideo::AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const Electra::FParamDict& InParameters)
{
	if (TimeoutInMicroseconds != 0)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	// Trigger removal of any old frames in the presentation queue of the player so we have all buffers we can have available
	TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
	if (PinnedPlayer.IsValid())
	{
		PinnedPlayer->DropOldFramesFromPresentationQueue();
	}

	// Check if we have any buffer which we can hand out to the decoder...
	if (QueueTickedAndWaitingForDecoder.IsEmpty())
	{
		// Check GLOBAL number of output textures in flight...
		if (NumOutputTexturesInUse < NumBuffers)
		{
			// Allocate an image from the pool, but do NOT use it immediately because it COULD be still rendering...
			FVideoDecoderOutputPtr DelayedImage;
			// Acquire image and with information about how many render frames to wait before usage is possible
			AcquireFromPool(DelayedImage);
			QueueTickedAndWaitingForDecoder.Enqueue(DelayedImage);
		}

		if (QueueTickedAndWaitingForDecoder.IsEmpty())
		{
			return UEMEDIA_ERROR_INSUFFICIENT_DATA;
		}
	}

	FVideoDecoderOutputPtr DelayedImage;
	QueueTickedAndWaitingForDecoder.Dequeue(DelayedImage);
	check(DelayedImage.IsValid());

	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = new FMediaBufferSharedPtrWrapper(DelayedImage);
	check(MediaBufferSharedPtrWrapper);
	MediaBufferSharedPtrWrapper->BufferProperties.Set("texture", FVariantValue(DelayedImage));
	OutBuffer = MediaBufferSharedPtrWrapper;

	FPlatformAtomics::InterlockedIncrement(&NumBuffersAcquiredForDecoder);

	return UEMEDIA_ERROR_OK;
}

/**
 * Releases the buffer for rendering and subsequent return to the buffer pool
 */
UEMediaError FElectraRendererVideo::ReturnBuffer(IBuffer* Buffer, bool bRender, const FParamDict& InSampleProperties)
{
	if (Buffer == nullptr)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = static_cast<FMediaBufferSharedPtrWrapper*>(Buffer);
	MediaBufferSharedPtrWrapper->DecoderOutput->GetMutablePropertyDictionary() = InSampleProperties;

	if (bRender)
	{
		//OPT/CHANGE: Note that "MediaBufferSharedPtrWrapper->DecoderOutput->GetDict()" is the very same as InSampleProperties!
		bool bIsDummyBuffer = InSampleProperties.GetValue("is_dummy").SafeGetBool(false);

		// Put frame into output queue...
		if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
		{
			// This call pushes the image back to the game thread for processing...
			PinnedPlayer->OnVideoDecoded(MediaBufferSharedPtrWrapper->DecoderOutput, bIsDummyBuffer);
		}
	}

	// Render or not.. here we do not have any error. The decoder is done with this buffer
	FPlatformAtomics::InterlockedDecrement(&NumBuffersAcquiredForDecoder);

	// In all cases free the wrapper class...
	delete MediaBufferSharedPtrWrapper;

	return UEMEDIA_ERROR_OK;
}

UEMediaError FElectraRendererVideo::ReleaseBufferPool()
{
	DecoderOutputPool.Reset();
	return UEMEDIA_ERROR_OK;
}

/**
 *
 */
bool FElectraRendererVideo::CanReceiveOutputFrames(uint64 NumFrames) const
{
	TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
	if (!PinnedPlayer.IsValid())
	{
		return false;
	}

	return PinnedPlayer->CanPresentVideoFrames(NumFrames);
}

/**
 *
 */
void FElectraRendererVideo::SetRenderClock(TSharedPtr<Electra::IMediaRenderClock, ESPMode::ThreadSafe> InRenderClock)
{
	RenderClock = InRenderClock;
}

void FElectraRendererVideo::SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> InParentRenderer)
{
	ParentRenderer = MoveTemp(InParentRenderer);
}

/**
 *
 */
void FElectraRendererVideo::SetNextApproximatePresentationTime(const Electra::FTimeValue& NextApproxPTS)
{
}

/**
 * Flushes all pending buffers not yet rendered
 */
UEMediaError FElectraRendererVideo::Flush(const Electra::FParamDict& InOptions)
{
	// [...]If there are still frames a decoder has not returned yet UEMEDIA_ERROR_INTERNAL should be returned.[...]
	if (NumBuffersAcquiredForDecoder != 0)
	{
		return UEMEDIA_ERROR_INTERNAL;
	}
	// Tell the player to flush now as well.
	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnVideoFlush();
	}
	return UEMEDIA_ERROR_OK;
}

/**
 * Begins rendering of the first sample buffer
 */
void FElectraRendererVideo::StartRendering(const Electra::FParamDict& InOptions)
{
	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnVideoRenderingStarted();
	}
}

/**
 * Stops rendering of sample buffers
*/
void FElectraRendererVideo::StopRendering(const Electra::FParamDict& InOptions)
{
	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnVideoRenderingStopped();
	}
}

void FElectraRendererVideo::TickOutputBufferPool()
{
	DecoderOutputPool.Tick();
}

/**
 * Called from game thread to update buffer management
 */
void FElectraRendererVideo::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, MEDIArendererVideoUE_Tick);

	check(IsInGameThread() || IsInSlateThread());

	// Keep stats up to date
	SET_DWORD_STAT(STAT_ElectraPlayer_MEDIArendererVideoUE_NumUsedOutputVideoSamples, NumOutputTexturesInUse);
}

