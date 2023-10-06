// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureMediaPlayer.h"

#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "ITextureMediaPlayerModule.h"
#include "MediaSamples.h"
#include "MediaPlayerOptions.h"
#include "RenderTargetPool.h"
#include "TextureMediaPlayerVideoDecoderOutput.h"

DEFINE_LOG_CATEGORY(LogTextureMediaPlayer);

//-----------------------------------------------------------------------------
/**
 *	Construction of new player
 */
FTextureMediaPlayer::FTextureMediaPlayer(IMediaEventSink& InEventSink)
	: EventSink(InEventSink)
	, OptionInterface(nullptr)
	, State(EMediaState::Closed)
	, Status(EMediaStatus::None)
	, bWasClosedOnError(false)
	, bAllowKillAfterCloseEvent(false)
	, NumTracksVideo(1)
	, CurrentTime(FTimespan::Zero())
	, TimeSinceLastFrameReceived(FTimespan::Zero())
	, CurrentFrameCount(0)
	, MostRecentFrameCount(0)
{
	MediaSamples.Reset(new FMediaSamples);
}

//-----------------------------------------------------------------------------
/**
 *	Cleanup destructor
 */
FTextureMediaPlayer::~FTextureMediaPlayer()
{
	CloseInternal(false);
}


// IMediaPlayer interface

FGuid FTextureMediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x16137521, 0x26364e57, 0xa5b66211, 0x821b9819);
	return PlayerPluginGUID;
}

FString FTextureMediaPlayer::GetInfo() const
{
	return TEXT("No information available");
}

IMediaSamples& FTextureMediaPlayer::GetSamples()
{
	// NOTE: This lock here is not really protecting the media samples in that we have no control what the caller is doing with the return value.
	//       All this can do is to temporarily guard the thing for clearing its contents.
	FScopeLock Lock(&MediaSamplesAccessLock);
	return *MediaSamples;
}

FString FTextureMediaPlayer::GetStats() const
{
	return TEXT("TextureMediaPlayer: GetStats: <empty>?");
}

IMediaTracks& FTextureMediaPlayer::GetTracks()
{
	return *this;
}

bool FTextureMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	return Open(Url, Options, nullptr);
}

bool FTextureMediaPlayer::Open(const FString& _InUrl, const IMediaOptions* Options, const FMediaPlayerOptions* InPlayerOptions)
{
	FString InUrl(_InUrl);
	FString Scheme;
	FString Location;

	if (!InUrl.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
	{
		UE_LOG(LogTextureMediaPlayer, Error, TEXT("Invalid URL, cannot parse the scheme: %s"), *InUrl);
		return false;
	}

	if (Scheme != TEXT("texture"))
	{
		UE_LOG(LogTextureMediaPlayer, Error, TEXT("Invalid URL scheme (%s), only 'texture' is supported"), *Scheme);
		return false;
	}

	// Clear out our work variables
	bWasClosedOnError = false;

	// Remember the option interface to poll for changes during playback.
	OptionInterface = Options;

	// Get a writable copy of the URL so we can override it for debugging.
	MediaUrl = InUrl;

	Status = EMediaStatus::Connecting;
	State = EMediaState::Preparing;
	DeferredEvents.Enqueue(EMediaEvent::MediaConnecting);

	State = EMediaState::Stopped;
	DeferredEvents.Enqueue(EMediaEvent::MediaOpened);
	DeferredEvents.Enqueue(EMediaEvent::TracksChanged);

	// Let the module know about our video sink.
	ITextureMediaPlayerModule* TextureModule = FModuleManager::GetModulePtr<ITextureMediaPlayerModule>("TextureMediaPlayer");
	if (TextureModule != nullptr)
	{
		TextureModule->RegisterVideoSink(AsShared());
	}

	return true;
}

//-----------------------------------------------------------------------------
/**
 *
 */
bool FTextureMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* /*Options*/)
{
	// we support playback only from an external file, not from a "resource" (e.g. a packaged asset)
	UE_LOG(LogTextureMediaPlayer, Error, TEXT("[%p] TextureMediaPlayer::Open"), this);
	unimplemented();
	return false;
}

//-----------------------------------------------------------------------------
/**
 *	Close / Shutdown player
 */
void FTextureMediaPlayer::CloseInternal(bool bKillAfterClose)
{
// TODO: check that it's not already closed.

	UE_LOG(LogTextureMediaPlayer, Log, TEXT("[%p] TextureMediaPlayer::Close()"), this);

	// For all intents and purposes the player can be considered closed here now already.
	State = EMediaState::Closed;
	MediaUrl = FString();

	DeferredEvents.Enqueue(EMediaEvent::TracksChanged);
	DeferredEvents.Enqueue(EMediaEvent::MediaClosed);

	bAllowKillAfterCloseEvent = bKillAfterClose;
}

//-----------------------------------------------------------------------------
/**
*	Internal Close / Shutdown player
*/
void FTextureMediaPlayer::Close()
{
	CloseInternal(true);
	CurrentTime = FTimespan::Zero();
	CurrentFrameCount = 0;
	MostRecentFrameCount = 0;
}

//-----------------------------------------------------------------------------
/**
 *	TODO: This would be a better clock. This is the actual time of the audio renderer.
 */
void FTextureMediaPlayer::SetLastAudioRenderedSampleTime(FTimespan SampleTime)
{
	// No-op.
}

//-----------------------------------------------------------------------------
/**
 *	Tick the player from the game thread
 */
void FTextureMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	if (State == EMediaState::Error && !bWasClosedOnError)
	{
		CloseInternal(true);
		// Close has set the state to close but we want it to stick at error.
		State = EMediaState::Error;
		bWasClosedOnError = true;
	}

	// Handle state changes as we receive frames.
	if (State != EMediaState::Error)	
	{
		// Has a new frame arrived?
		if (MostRecentFrameCount != CurrentFrameCount)
		{
			TimeSinceLastFrameReceived = FTimespan::Zero();
			CurrentFrameCount = MostRecentFrameCount;
			if (State != EMediaState::Playing)
			{
				State = EMediaState::Playing;
				DeferredEvents.Enqueue(EMediaEvent::PlaybackResumed);
				UE_LOG(LogTextureMediaPlayer, Log, TEXT("[%p] TextureMediaPlayer: received new video frame, switching to playing"), this);
			}
		}
		else
		{
			TimeSinceLastFrameReceived += DeltaTime;
			const int32 kReceiveTimeoutMillis = 10000;
			int64 timeSinceMillis = TimeSinceLastFrameReceived.GetTotalMilliseconds();
			if (timeSinceMillis > kReceiveTimeoutMillis)
			{
				if (State == EMediaState::Playing)
				{
					State = EMediaState::Paused;
					DeferredEvents.Enqueue(EMediaEvent::PlaybackSuspended);
					UE_LOG(LogTextureMediaPlayer, Log, TEXT("[%p] TextureMediaPlayer: did not receive new video frame for %d ms, switching to paused"), this, (int32)timeSinceMillis);
				}
			}
		}
	}

	if (State == EMediaState::Playing)
	{
		CurrentTime = Timecode;
	}

	// Forward enqueued session events. We do this even with no current internal player to ensure all pending events are sent and none are lost.
	EMediaEvent Event;
	while (DeferredEvents.Dequeue(Event))
	{
		EventSink.ReceiveMediaEvent(Event);
	}
}

//-----------------------------------------------------------------------------
/**
	Get special feature flags states
*/
bool FTextureMediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
{
	switch(flag)
	{
		case EFeatureFlag::AllowShutdownOnClose:
			return bAllowKillAfterCloseEvent;
		case EFeatureFlag::UsePlaybackTimingV2:
			return true;
		case EFeatureFlag::AlwaysPullNewestVideoFrame:
			return true;
		default:
			break;
	}
	return IMediaPlayer::GetPlayerFeatureFlag(flag);
}

//////////////////////////////////////////////////////////////////////////
// IMediaControl impl

//-----------------------------------------------------------------------------
/**
 *	Currently, we cannot control anything. We play a Live video.
 */
bool FTextureMediaPlayer::CanControl(EMediaControl Control) const
{
	return false;
}

//-----------------------------------------------------------------------------
/**
 *	Rate is only real-time when playing.
 */
float FTextureMediaPlayer::GetRate() const
{
	return (GetState() == EMediaState::Playing) ? 1.0f : 0.0f;
}

//-----------------------------------------------------------------------------
/**
 *	Expose player state
 */
EMediaState FTextureMediaPlayer::GetState() const
{
	return State;
}

//-----------------------------------------------------------------------------
/**
 *	Expose player status
 */
EMediaStatus FTextureMediaPlayer::GetStatus() const
{
	return Status;
}

//-----------------------------------------------------------------------------
/**
 *	Only return real-time playback for the moment..
 */
TRangeSet<float> FTextureMediaPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	TRangeSet<float> Res;
	Res.Add(TRange<float>{1.0f}); // only normal (real-time) playback rate
	Res.Add(TRange<float>{0.0f}); // and pause
	return Res;
}

FTimespan FTextureMediaPlayer::GetTime() const
{
	return CurrentTime;
}

FTimespan FTextureMediaPlayer::GetDuration() const
{
	// A Live feed has infinite duration.
	return FTimespan::MaxValue();
}

bool FTextureMediaPlayer::SetRate(float Rate)
{
	// This has no effect at all, but we say it has succeeded anyway.
	return true;
}

bool FTextureMediaPlayer::Seek(const FTimespan& Time)
{
	return false;
}

bool FTextureMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	return false;
}

int32 FTextureMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if (TrackType == EMediaTrackType::Video)
	{
		return NumTracksVideo;
	}
	return 0;
}

int32 FTextureMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return 0;
}

int32 FTextureMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	return TrackType == EMediaTrackType::Video ? 0 : -1;
}

FText FTextureMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FText();
}

int32 FTextureMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// Right now we only have a single format per track so we return format index 0 at all times.
	return 0;
}

FString FTextureMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TEXT("");
}

FString FTextureMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TEXT("");
}

bool FTextureMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	return false;
}

bool FTextureMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	return false;
}

bool FTextureMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

bool FTextureMediaPlayer::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	return false;
}

void FTextureMediaPlayer::OnFrame(const TArray<uint8>& TextureBuffer, FIntPoint Size)
{
	if (TextureBuffer.Num() > 0)
	{
		int32 Width = Size.X;
		int32 SampleHeight = Size.Y;
		int32 Height = (SampleHeight * 2) / 3;

		FVideoDecoderOutputPtr NewVideoDecoderOutput = MakeShared<TextureMediaPlayerVideoDecoderOutput, ESPMode::ThreadSafe>();
		TextureMediaPlayerVideoDecoderOutput* VideoDecoderOutput = static_cast<TextureMediaPlayerVideoDecoderOutput*>(NewVideoDecoderOutput.Get());

		// Set up parameters.
		TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> OutputBufferSampleProperties(new Electra::FParamDict());
		Electra::FTimeValue TimeStamp;
		TimeStamp.SetFromHNS((int64)1);	// Duration as HNS; should not be 0 but is unknown. Use a small value instead.
		OutputBufferSampleProperties->Set("duration", Electra::FVariantValue(TimeStamp));
		OutputBufferSampleProperties->Set("pixelfmt", Electra::FVariantValue((int64)EPixelFormat::PF_NV12));
		OutputBufferSampleProperties->Set("pts", Electra::FVariantValue(TimeStamp));
		OutputBufferSampleProperties->Set("width", Electra::FVariantValue((int64)Width));
		OutputBufferSampleProperties->Set("height", Electra::FVariantValue((int64)Height));

		VideoDecoderOutput->Initialize(MoveTemp(OutputBufferSampleProperties), TextureBuffer, Size);

		// We are dealing with real time video here, so it is important we show the most recent frame received.
		// Older frames that may have accumulated in the queue past a certain point are not relevant any more.
		// If the application does not get ticked for some reason frames might pile up in the queue because they
		// do not get removed fast enough. Even though the application does only make use of the most recent
		// frame when ticked the older frames are only removed then and will have consumed lots of memory
		// in the meantime.
		// To prevent this from happening we remove old frames from the queue ourselves right here.
		int32 NumFramesInQueue = MediaSamples->NumVideoSamples();
		if (NumFramesInQueue > 2)
		{
			// This will cause the queue to get flushed, but only on the next game tick.
			// Anything we enqueue in the meantime will get flushed as well.
			// NOTE: We cannot use Internal_PurgeVideoSamplesHint here because if the game loop is not
			//       being ticked the time it uses for comparison will not have advanced and nothing
			//       will get purged.
			MediaSamples->PurgeOutdatedVideoSamples(FMediaTimeStamp(FTimespan::MaxValue()), false, FTimespan::Zero());
			EventSink.ReceiveMediaEvent(EMediaEvent::Internal_ResetForDiscontinuity);
		}

		FElectraTextureSampleRef TextureSample = OutputTexturePool.AcquireShared();
		// The texture sample needs a POD pointer for some reason but will internally attach to a threadsafe TSharedPtr via AsShared().
		// Ownership of the decoded frame is thereby transfered to the texture sample.
		TextureSample->Initialize(VideoDecoderOutput);
		++MostRecentFrameCount;
		MediaSamples->AddVideo(TextureSample);		
	}
}

#if PLATFORM_WINDOWS
void FTextureMediaPlayer::OnFrame(FTextureRHIRef TextureRHIRef, TRefCountPtr<ID3D12Fence> D3DFence, uint64 FenceValue)
{
	if (TextureRHIRef.IsValid())
	{
		FIntVector SizeXYZ = TextureRHIRef->GetSizeXYZ();
		int32 Width = SizeXYZ.X;
		int32 SampleHeight = SizeXYZ.Y;
		int32 Height = (SampleHeight * 2) / 3;
		
		FVideoDecoderOutputPtr NewVideoDecoderOutput = MakeShared<TextureMediaPlayerVideoDecoderOutput, ESPMode::ThreadSafe>();
		TextureMediaPlayerVideoDecoderOutput* VideoDecoderOutput = static_cast<TextureMediaPlayerVideoDecoderOutput*>(NewVideoDecoderOutput.Get());

		// Set up parameters.
		TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> OutputBufferSampleProperties(new Electra::FParamDict());
		Electra::FTimeValue TimeStamp;
		TimeStamp.SetFromHNS((int64)1);	// Duration as HNS; should not be 0 but is unknown. Use a small value instead.
		OutputBufferSampleProperties->Set("duration", Electra::FVariantValue(TimeStamp));
		OutputBufferSampleProperties->Set("pixelfmt", Electra::FVariantValue((int64)EPixelFormat::PF_NV12));
		OutputBufferSampleProperties->Set("pts", Electra::FVariantValue(TimeStamp));
		OutputBufferSampleProperties->Set("width", Electra::FVariantValue((int64)Width));
		OutputBufferSampleProperties->Set("height", Electra::FVariantValue((int64)Height));

		VideoDecoderOutput->Initialize(MoveTemp(OutputBufferSampleProperties), TextureRHIRef, FIntPoint(Width, SampleHeight), D3DFence, FenceValue);

		// We are dealing with real time video here, so it is important we show the most recent frame received.
		// Older frames that may have accumulated in the queue past a certain point are not relevant any more.
		// If the application does not get ticked for some reason frames might pile up in the queue because they
		// do not get removed fast enough. Even though the application does only make use of the most recent
		// frame when ticked the older frames are only removed then and will have consumed lots of memory
		// in the meantime.
		// To prevent this from happening we remove old frames from the queue ourselves right here.
		int32 NumFramesInQueue = MediaSamples->NumVideoSamples();
		if (NumFramesInQueue > 2)
		{
			// This will cause the queue to get flushed, but only on the next game tick.
			// Anything we enqueue in the meantime will get flushed as well.
			// NOTE: We cannot use Internal_PurgeVideoSamplesHint here because if the game loop is not
			//       being ticked the time it uses for comparison will not have advanced and nothing
			//       will get purged.
			MediaSamples->PurgeOutdatedVideoSamples(FMediaTimeStamp(FTimespan::MaxValue()), false, FTimespan::Zero());
			EventSink.ReceiveMediaEvent(EMediaEvent::Internal_ResetForDiscontinuity);
		}

		FElectraTextureSampleRef TextureSample = OutputTexturePool.AcquireShared();
		// The texture sample needs a POD pointer for some reason but will internally attach to a threadsafe TSharedPtr via AsShared().
		// Ownership of the decoded frame is thereby transfered to the texture sample.
		TextureSample->Initialize(VideoDecoderOutput);
		++MostRecentFrameCount;
		MediaSamples->AddVideo(TextureSample);
	}
}
#endif