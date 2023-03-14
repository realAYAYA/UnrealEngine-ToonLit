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
	// We need to track this. Otherwise the audio buffers in flight may cause corruption...
//	check(NumOutputAudioBuffersInUse == 0);
//TRUE? THE POOL GONE IS CHECKED. THE RENDERER GONE, TOO... SOOO?
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

	const FVariantValue& variantNumBuffers = Parameters.GetValue("num_buffers");
	if (!variantNumBuffers.IsType(FVariantValue::EDataType::TypeInt64))
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}
	NumBuffers = (int32)variantNumBuffers.GetInt64();

	// Update dic for later query via GetBufferPoolProperties
	BufferPoolProperties.Set("max_buffers", Electra::FVariantValue((int64)NumBuffers));

	const FVariantValue& variantMaxBufferSize = Parameters.GetValue("max_buffer_size");
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
	MediaBufferSharedPtrWrapper->BufferProperties.Set("size", FVariantValue((int64)DecoderOutput->GetReservedBufferBytes()));
	MediaBufferSharedPtrWrapper->BufferProperties.Set("address", FVariantValue(const_cast<void*>(DecoderOutput->GetBuffer())));
	OutBuffer = MediaBufferSharedPtrWrapper;

	return UEMEDIA_ERROR_OK;
}



UEMediaError FElectraRendererAudio::ReturnBuffer(IBuffer* Buffer, bool bRender, const FParamDict& InSampleProperties)
{
	if (Buffer == nullptr)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = static_cast<FMediaBufferSharedPtrWrapper*>(Buffer);

	if (bRender)
	{
		const FVariantValue& variantNumChannels = InSampleProperties.GetValue("num_channels");
		if (!variantNumChannels.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantSampleRate = InSampleProperties.GetValue("sample_rate");
		if (!variantSampleRate.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantBufferUsedBytes = InSampleProperties.GetValue("byte_size");
		if (!variantBufferUsedBytes.IsType(FVariantValue::EDataType::TypeInt64))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantPts = InSampleProperties.GetValue("pts");
		if (!variantPts.IsType(FVariantValue::EDataType::TypeTimeValue))
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		const FVariantValue& variantDuration = InSampleProperties.GetValue("duration");
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

		DecoderOutput->GetMutablePropertyDictionary() = InSampleProperties;
		DecoderOutput->Initialize(IAudioDecoderOutput::ESampleFormat::Int16, InNumChannels, InSampleRate, InDuration, FDecoderTimeStamp(InPts, InSequenceIndex), InUsedBufferBytes);

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
