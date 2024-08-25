// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MediaEventSignal.h"
#include "Core/MediaInterlocked.h"

#include "Renderer/RendererAudio.h"

#include "Misc/Timespan.h"
#include "ElectraPlayer.h"
#include "ElectraPlayerPrivate.h"

void FElectraRendererAudio::SampleReleasedToPool(IDecoderOutput* InDecoderOutput)
{
	FPlatformAtomics::InterlockedDecrement(&NumOutputAudioBuffersInUse);
	check(NumOutputAudioBuffersInUse >= 0 && NumOutputAudioBuffersInUse <= NumBuffers)

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> Parent = ParentRenderer.Pin();
	if (Parent.IsValid())
	{
		Parent->SampleReleasedToPool(InDecoderOutput);
	}
}

bool FElectraRendererAudio::Startup(const FElectraRendererAudio::SystemConfiguration& Configuration)
{
	return true;
}

void FElectraRendererAudio::Shutdown(void)
{
}

FElectraRendererAudio::FElectraRendererAudio(TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> InPlayer)
	: Player(InPlayer)
	, MaxBufferSize(0)
	, NumOutputAudioBuffersInUse(0)
	, NumBuffers(0)
	, NumBuffersAcquiredForDecoder(0)
{
}

FElectraRendererAudio::~FElectraRendererAudio()
{
}

FElectraRendererAudio::OpenError FElectraRendererAudio::Open(const FElectraRendererAudio::InstanceConfiguration& Config)
{
	return OpenError::eOpen_Ok;
}


void FElectraRendererAudio::Close(void)
{
}


void FElectraRendererAudio::DetachPlayer()
{
	Player.Reset();
}


const Electra::FParamDict& FElectraRendererAudio::GetBufferPoolProperties() const
{
	return BufferPoolProperties;
}


UEMediaError FElectraRendererAudio::CreateBufferPool(const Electra::FParamDict& Parameters)
{
	// Create a new pool and overwrite all "pending" values...
	DecoderOutputPool.Reset();
	NumOutputAudioBuffersInUse = 0;

	const FVariantValue& variantNumBuffers = Parameters.GetValue(RenderOptionKeys::NumBuffers);
	if (!variantNumBuffers.IsType(FVariantValue::EDataType::TypeInt64))
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}
	NumBuffers = (int32)variantNumBuffers.GetInt64();

	// Update dic for later query via GetBufferPoolProperties
	BufferPoolProperties.Set(RenderOptionKeys::MaxBuffers, Electra::FVariantValue((int64)NumBuffers));

	const FVariantValue& variantMaxBufferSize = Parameters.GetValue(RenderOptionKeys::MaxBufferSize);
	if (!variantMaxBufferSize.IsType(FVariantValue::EDataType::TypeInt64))
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	MaxBufferSize = (uint32)variantMaxBufferSize.GetInt64();

	return UEMEDIA_ERROR_OK;
}


UEMediaError FElectraRendererAudio::AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const Electra::FParamDict& InParameters)
{
	if (TimeoutInMicroseconds != 0)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	// Check if we are inside the max buffer limit...
	if (NumOutputAudioBuffersInUse >= NumBuffers)
	{
		return UEMEDIA_ERROR_INSUFFICIENT_DATA;
	}

	IAudioDecoderOutputPtr DecoderOutput = DecoderOutputPool.AcquireShared();
	DecoderOutput->SetOwner(SharedThis(this));

	FPlatformAtomics::InterlockedIncrement(&NumBuffersAcquiredForDecoder);
	DecoderOutput->Reserve(MaxBufferSize);

	FPlatformAtomics::InterlockedIncrement(&NumOutputAudioBuffersInUse);

	// UE handles all with shared pointers...
	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = new FMediaBufferSharedPtrWrapper(DecoderOutput);
	MediaBufferSharedPtrWrapper->BufferProperties.Set(RenderOptionKeys::AllocatedSize, FVariantValue((int64)DecoderOutput->GetReservedBufferBytes()));
	MediaBufferSharedPtrWrapper->BufferProperties.Set(RenderOptionKeys::AllocatedAddress, FVariantValue(const_cast<void*>(DecoderOutput->GetBuffer())));
	OutBuffer = MediaBufferSharedPtrWrapper;

	return UEMEDIA_ERROR_OK;
}



UEMediaError FElectraRendererAudio::ReturnBuffer(IBuffer* Buffer, bool bRender, FParamDict& InOutSampleProperties)
{
	if (Buffer == nullptr)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = static_cast<FMediaBufferSharedPtrWrapper*>(Buffer);

	if (bRender)
	{
		const FVariantValue& variantNumChannels = InOutSampleProperties.GetValue(RenderOptionKeys::NumChannels);
		if (!variantNumChannels.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantSampleRate = InOutSampleProperties.GetValue(RenderOptionKeys::SampleRate);
		if (!variantSampleRate.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantBufferUsedBytes = InOutSampleProperties.GetValue(RenderOptionKeys::UsedByteSize);
		if (!variantBufferUsedBytes.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantPts = InOutSampleProperties.GetValue(RenderOptionKeys::PTS);
		if (!variantPts.IsType(FVariantValue::EDataType::TypeTimeValue))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantDuration = InOutSampleProperties.GetValue(RenderOptionKeys::Duration);
		if (!variantDuration.IsType(FVariantValue::EDataType::TypeTimeValue))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		IAudioDecoderOutputPtr DecoderOutput = MediaBufferSharedPtrWrapper->DecoderOutput;

		uint32 InNumChannels = (uint32)variantNumChannels.GetInt64();
		uint32 InSampleRate = (uint32)variantSampleRate.GetInt64();
		uint32 InUsedBufferBytes = (uint32)variantBufferUsedBytes.GetInt64();

		FTimespan InDuration = variantDuration.GetTimeValue().GetAsTimespan();
		FTimespan InPts = variantPts.GetTimeValue().GetAsTimespan();
		int64 InSequenceIndex = variantPts.GetTimeValue().GetSequenceIndex();

		//UE_LOG(LogElectraPlayer, VeryVerbose, TEXT("-- FElectraRendererAudio::ReturnBuffer: Audio sample for time %s"), *InPts.ToString(TEXT("%h:%m:%s.%f")));

		DecoderOutput->GetMutablePropertyDictionary() = InOutSampleProperties;
		DecoderOutput->Initialize(IAudioDecoderOutput::ESampleFormat::Float, InNumChannels, InSampleRate, InDuration, FDecoderTimeStamp(InPts, InSequenceIndex), InUsedBufferBytes);

		// Push buffer to output queue...
		if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
		{
			PinnedPlayer->OnAudioDecoded(DecoderOutput);
		}
	}

	// Render or not.. here we do not have any error. The decoder is done with this buffer
	FPlatformAtomics::InterlockedDecrement(&NumBuffersAcquiredForDecoder);

	// In all cases free the wrapper class...
	delete MediaBufferSharedPtrWrapper;

	return UEMEDIA_ERROR_OK;
}

UEMediaError FElectraRendererAudio::ReleaseBufferPool()
{
	DecoderOutputPool.Reset();
	return UEMEDIA_ERROR_OK;
}

bool FElectraRendererAudio::CanReceiveOutputFrames(uint64 NumFrames) const
{
	TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
	if (!PinnedPlayer.IsValid())
	{
		return false;
	}
	return PinnedPlayer->CanPresentAudioFrames(NumFrames);
}

bool FElectraRendererAudio::GetEnqueuedFrameInfo(int32& OutNumberOfEnqueuedFrames, Electra::FTimeValue& OutDurationOfEnqueuedFrames) const
{
	OutNumberOfEnqueuedFrames = 0;
	OutDurationOfEnqueuedFrames.SetToZero();
	return false;
}

/**
 *
 */
void FElectraRendererAudio::SetRenderClock(TSharedPtr<Electra::IMediaRenderClock, ESPMode::ThreadSafe> InRenderClock)
{
	RenderClock = InRenderClock;
}

void FElectraRendererAudio::SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> InParentRenderer)
{
	ParentRenderer = MoveTemp(InParentRenderer);
}


void FElectraRendererAudio::SetNextApproximatePresentationTime(const Electra::FTimeValue& NextApproxPTS)
{
}

// Flushes all pending buffers not yet rendered
UEMediaError FElectraRendererAudio::Flush(const Electra::FParamDict& InOptions)
{
	// [...]If there are still frames a decoder has not returned yet UEMEDIA_ERROR_INTERNAL should be returned.[...]
	if (NumBuffersAcquiredForDecoder != 0)
	{
		return UEMEDIA_ERROR_INTERNAL;
	}

	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnAudioFlush();
	}

	return UEMEDIA_ERROR_OK;
}

// Begins rendering of the first sample buffer
void FElectraRendererAudio::StartRendering(const Electra::FParamDict& InOptions)
{
	//UE_LOG(LogElectraPlayer, Log, TEXT("-- FElectraRendererAudio::StartRendering (NumBuffers = %d)"), NumBuffers);
	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnAudioRenderingStarted();
	}
}


// Stops rendering of sample buffers
void FElectraRendererAudio::StopRendering(const Electra::FParamDict& InOptions)
{
	//UE_LOG(LogElectraPlayer, Log, TEXT("-- FElectraRendererAudio::StopRendering (NumBuffers = %d)"), NumBuffers);
	if (TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->OnAudioRenderingStopped();
	}
}
