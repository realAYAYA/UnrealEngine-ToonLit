// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaPlayer.h"
#include "ImgMediaPrivate.h"
#include "ImgMediaSource.h"

#include "Async/Async.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

#include "ImgMediaLoader.h"
#include "ImgMediaMipMapInfo.h"
#include "ImgMediaScheduler.h"
#include "ImgMediaSettings.h"
#include "ImgMediaTextureSample.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaTexture.h"
#include "MediaTextureTracker.h"

#define IMG_MEDIA_PLAYER_VERSION 2
#define LOCTEXT_NAMESPACE "FImgMediaPlayer"


/** Time spent closing image media players. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Player Close"), STAT_ImgMedia_PlayerClose, STATGROUP_Media);

/** Time spent in image media player input tick. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Player TickInput"), STAT_ImgMedia_PlayerTickInput, STATGROUP_Media);


const FTimespan HackDeltaTimeOffset(1);

namespace {
	/** Convenience function to process all media textures corresponding the specified player. */
	void ApplyToPlayerMediaTextures(FImgMediaPlayer* InPlayer, TFunctionRef<void(UMediaTexture*)> TextureCallbackFn)
	{
		TArray<UMediaTexture*> PlayerTextures;

		FMediaTextureTracker& TextureTracker = FMediaTextureTracker::Get();

		// Look through all the media textures we know about.
		for (TWeakObjectPtr<UMediaTexture> TexturePtr : TextureTracker.GetTextures())
		{
			UMediaTexture* Texture = TexturePtr.Get();
			if (Texture != nullptr)
			{
				// Does this match the player?
				UMediaPlayer* MediaPlayer = Texture->GetMediaPlayer();
				if (MediaPlayer != nullptr)
				{
					TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> Player = MediaPlayer->GetPlayerFacade()->GetPlayer();
					if (Player.IsValid())
					{
						if (Player.Get() == InPlayer)
						{
							TextureCallbackFn(Texture);
						}
					}
				}
			}
		}
	}
}


/* FImgMediaPlayer structors
 *****************************************************************************/

FImgMediaPlayer::FImgMediaPlayer(IMediaEventSink& InEventSink, const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
	const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache)
	: CurrentDuration(FTimespan::Zero())
	, CurrentRate(0.0f)
	, CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, DeltaTimeHackApplied(false)
	, EventSink(InEventSink)
	, LastFetchTime(FTimespan::MinValue())
	, PlaybackRestarted(false)
	, Scheduler(InScheduler)
	, SelectedVideoTrack(INDEX_NONE)
	, ShouldLoop(false)
	, GlobalCache(InGlobalCache)
	, RequestFrameHasRun(true)
	, PlaybackIsBlocking(false)
{ }


FImgMediaPlayer::~FImgMediaPlayer()
{
	Close();
}


/* IMediaPlayer interface
 *****************************************************************************/

void FImgMediaPlayer::Close()
{
	SCOPE_CYCLE_COUNTER(STAT_ImgMedia_PlayerClose);

	if (!Loader.IsValid())
	{
		return;
	}

	{
		const TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe>& MipMapInfo = Loader->GetMipMapInfo();
		if (MipMapInfo.IsValid())
		{
			ApplyToPlayerMediaTextures(this, [&MipMapInfo](UMediaTexture* Texture)
				{
					MipMapInfo->RemoveObjectsUsingThisMediaTexture(Texture);
				});
		}
	}

	Scheduler->UnregisterLoader(Loader.ToSharedRef());
	Loader.Reset();

	CurrentDuration = FTimespan::Zero();
	CurrentUrl.Empty();
	CurrentRate = 0.0f;
	CurrentState = EMediaState::Closed;
	CurrentTime = FTimespan::Zero();
	DeltaTimeHackApplied = false;
	LastFetchTime = FTimespan::MinValue();
	PlaybackRestarted = false;
	SelectedVideoTrack = INDEX_NONE;
	PlaybackIsBlocking = false;

	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}


IMediaCache& FImgMediaPlayer::GetCache()
{
	return *this;
}


IMediaControls& FImgMediaPlayer::GetControls()
{
	return *this;
}


FString FImgMediaPlayer::GetInfo() const
{
	return Loader.IsValid() ? Loader->GetInfo() : FString();
}


FVariant FImgMediaPlayer::GetMediaInfo(FName InfoName) const
{
	FVariant Variant;

	// Source num mips?
	if (InfoName == UMediaPlayer::MediaInfoNameSourceNumMips.Resolve())
	{
		if (Loader.IsValid())
		{
			int32 NumMips = Loader->GetNumMipLevels();
			Variant = NumMips;
		}
	}
	// Source num tiles?
	else if (InfoName == UMediaPlayer::MediaInfoNameSourceNumTiles.Resolve())
	{
		if (Loader.IsValid())
		{
			FIntPoint TileNum(Loader->GetNumTilesX(), Loader->GetNumTilesY());
			Variant = TileNum;
		}
	}

	return Variant;
}


FGuid FImgMediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x0e4a60c0, 0x2c5947ea, 0xb233562a, 0x57e5761c);
	return PlayerPluginGUID;
}


IMediaSamples& FImgMediaPlayer::GetSamples()
{
	return *this;
}


FString FImgMediaPlayer::GetStats() const
{
	FString StatsString;
	{
		StatsString += TEXT("not implemented yet");
		StatsString += TEXT("\n");
	}

	return StatsString;
}


IMediaTracks& FImgMediaPlayer::GetTracks()
{
	return *this;
}


FString FImgMediaPlayer::GetUrl() const
{
	return CurrentUrl;
}


IMediaView& FImgMediaPlayer::GetView()
{
	return *this;
}


bool FImgMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if (Url.IsEmpty() || !Url.StartsWith(TEXT("img://")))
	{
		return false;
	}

	CurrentState = EMediaState::Preparing;
	CurrentUrl = Url;

	// determine image sequence proxy, if any
	FString Proxy;
	
	if (Options != nullptr)
	{
		Proxy = Options->GetMediaOption(ImgMedia::ProxyOverrideOption, FString());
	}

	if (Proxy.IsEmpty())
	{
		Proxy = GetDefault<UImgMediaSettings>()->GetDefaultProxy();
	}

	// get frame rate override, if any
	FFrameRate FrameRateOverride(0, 0);
	TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe> MipMapInfo;
	bool bFillGapsInSequence = true;
	bool bIsSmartCacheEnabled = false;
	float SmartCacheTimeToLookAhead = 0.0f;
	if (Options != nullptr)
	{
		FrameRateOverride.Denominator = Options->GetMediaOption(ImgMedia::FrameRateOverrideDenonimatorOption, 0LL);
		FrameRateOverride.Numerator = Options->GetMediaOption(ImgMedia::FrameRateOverrideNumeratorOption, 0LL);
		bFillGapsInSequence = Options->GetMediaOption(ImgMedia::FillGapsInSequenceOption, true);
		bIsSmartCacheEnabled = Options->GetMediaOption(ImgMedia::SmartCacheEnabled, false);
		SmartCacheTimeToLookAhead = Options->GetMediaOption(ImgMedia::SmartCacheTimeToLookAhead, 0.0f);
		TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> DefaultValue;
		TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> DataContainer = Options->GetMediaOption(ImgMedia::MipMapInfoOption, DefaultValue);
		if (DataContainer.IsValid())
		{
			MipMapInfo = StaticCastSharedPtr<FImgMediaMipMapInfo, IMediaOptions::FDataContainer, ESPMode::ThreadSafe>(DataContainer);
			if (MipMapInfo.IsValid())
			{
				ApplyToPlayerMediaTextures(this, [&MipMapInfo](UMediaTexture* Texture)
					{
						MipMapInfo->AddObjectsUsingThisMediaTexture(Texture);
					});
			}
		}
	}

	// initialize image loader on a separate thread
	FImgMediaLoaderSmartCacheSettings SmartCacheSettings(bIsSmartCacheEnabled, SmartCacheTimeToLookAhead);
	Loader = MakeShared<FImgMediaLoader, ESPMode::ThreadSafe>(Scheduler.ToSharedRef(),
		GlobalCache.ToSharedRef(), MipMapInfo, bFillGapsInSequence, SmartCacheSettings);
	Scheduler->RegisterLoader(Loader.ToSharedRef());

	const FString SequencePath = Url.RightChop(6);

	Async(EAsyncExecution::ThreadPool, [FrameRateOverride, LoaderPtr = TWeakPtr<FImgMediaLoader, ESPMode::ThreadSafe>(Loader), Proxy, SequencePath, Loop = ShouldLoop]()
	{
		TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> PinnedLoader = LoaderPtr.Pin();

		if (PinnedLoader.IsValid())
		{
			FString ProxyPath = FPaths::Combine(SequencePath, Proxy);

			if (!FPaths::DirectoryExists(ProxyPath))
			{
				ProxyPath = SequencePath; // fall back to root folder
			}

			PinnedLoader->Initialize(ProxyPath, FrameRateOverride, Loop);
		}
	});

	return true;
}


bool FImgMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	return false; // not supported
}


void FImgMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	SCOPE_CYCLE_COUNTER(STAT_ImgMedia_PlayerTickInput);

	if (!Loader.IsValid() || (CurrentState == EMediaState::Error))
	{
		return;
	}

	// finalize loader initialization
	if ((CurrentState == EMediaState::Preparing) && Loader->IsInitialized())
	{
		if (Loader->GetSequenceDim().GetMin() == 0)
		{
			CurrentState = EMediaState::Error;

			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		}
		else
		{
			CurrentDuration = Loader->GetSequenceDuration();
			CurrentState = EMediaState::Stopped;

			EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
		}
	}

#if IMG_MEDIA_PLAYER_VERSION == 1

	if ((CurrentState != EMediaState::Playing) || (CurrentDuration == FTimespan::Zero()))
	{
		return; // nothing to play
	}

	// update clock
	if (PlaybackRestarted)
	{
		PlaybackRestarted = false;
	}
	else
	{
		CurrentTime += DeltaTime * CurrentRate;
	}

	// The following is a hack to accommodate for frame time rounding errors. The problem is
	// that frame delta times can be one tick more or less each frame, depending on how the
	// frame time is rounded. This presents a problem when driving media playback from Sequencer,
	// because even both Sequencer and Media Framework clocks are running at the same rate, they
	// may not be in phase with regards to rounding. This can cause some frames to be skipped.
	// FFrameTime support in Media Framework is required to fix this properly.

	if (!DeltaTimeHackApplied)
	{
		CurrentTime += HackDeltaTimeOffset;
		DeltaTimeHackApplied = true;
	}

	// handle looping
	if ((CurrentTime >= CurrentDuration) || (CurrentTime < FTimespan::Zero()))
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);

		if (ShouldLoop)
		{
			CurrentTime %= CurrentDuration;

			if (CurrentTime < FTimespan::Zero())
			{
				CurrentTime += CurrentDuration;
			}
		}
		else
		{
			CurrentState = EMediaState::Stopped;
			CurrentTime = FTimespan::Zero();
			CurrentRate = 0.0f;
			DeltaTimeHackApplied = false;

			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		}
	}

	UE_LOG(LogImgMedia, VeryVerbose, TEXT("Player %p: CurrentTime %s, Delta %s, CurrentRate %f"),
		this,
		*CurrentTime.ToString(TEXT("%h:%m:%s.%t")),
		*DeltaTime.ToString(TEXT("%h:%m:%s.%t")),
		CurrentRate
	);

	// update image loader
	if (SelectedVideoTrack == 0)
	{
		Loader->RequestFrame(CurrentTime, CurrentRate, ShouldLoop);
	}
	RequestFrameHasRun = true;
#else
	// Tick the scheduler an extra time in addition to its hookup as media clock sink, so we also get it moving forward during blocked playback
	Scheduler->TickInput(FTimespan::Zero(), FTimespan::MinValue());
#endif // IMG_MEDIA_PLAYER_VERSION == 1
}

bool FImgMediaPlayer::FlushOnSeekStarted() const
{
#if IMG_MEDIA_PLAYER_VERSION == 1
	return false;
#else
	// Flush on start, otherwise if we already have the frames ready during a seek,
	// the frames we return will get flushed straight away.
	return true;
#endif
}

bool FImgMediaPlayer::FlushOnSeekCompleted() const
{
#if IMG_MEDIA_PLAYER_VERSION == 1
	return true;
#else
	return false;
#endif
}

void FImgMediaPlayer::ProcessVideoSamples()
{
#if IMG_MEDIA_PLAYER_VERSION == 1
	// Did we already run this frame?
	if (RequestFrameHasRun)
	{
		RequestFrameHasRun = false;
	}
	else
	{
		// We are blocked... run stuff here as it will not get run normally.
		if (Loader.IsValid())
		{
			if (SelectedVideoTrack == 0)
			{
				Loader->RequestFrame(CurrentTime, CurrentRate, ShouldLoop);
			}
		}
		if (Scheduler.IsValid())
		{
			Scheduler->TickFetch(FTimespan::Zero(), FTimespan::Zero());
		}
	}
#endif // IMG_MEDIA_PLAYER_VERSION == 1
}

//-----------------------------------------------------------------------------
/**
	Get special feature flags states
*/
bool FImgMediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
{
#if IMG_MEDIA_PLAYER_VERSION >= 2
	switch (flag)
	{
	case EFeatureFlag::UsePlaybackTimingV2:
		return true;
	default:
		break;
	}
#endif  // IMG_MEDIA_PLAYER_VERSION >= 2
	return IMediaPlayer::GetPlayerFeatureFlag(flag);
}


/* FImgMediaPlayer implementation
 *****************************************************************************/

bool FImgMediaPlayer::IsInitialized() const
{
	return
		(CurrentState != EMediaState::Closed) &&
		(CurrentState != EMediaState::Error) &&
		(CurrentState != EMediaState::Preparing);
}


/* IMediaCache interface
 *****************************************************************************/

bool FImgMediaPlayer::QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Loader.IsValid())
	{
		return false;
	}

	if (State == EMediaCacheState::Loading)
	{
		Loader->GetBusyTimeRanges(OutTimeRanges);
	}
	else if (State == EMediaCacheState::Loaded)
	{
		Loader->GetCompletedTimeRanges(OutTimeRanges);
	}
	else if (State == EMediaCacheState::Pending)
	{
		Loader->GetPendingTimeRanges(OutTimeRanges);
	}
	else
	{
		return false;
	}

	return true;
}


/* IMediaControls interface
 *****************************************************************************/

bool FImgMediaPlayer::CanControl(EMediaControl Control) const
{
	if (Control == EMediaControl::BlockOnFetch)
	{
		return true;
	}

	if (!IsInitialized())
	{
		return false;
	}

	if (Control == EMediaControl::Pause)
	{
		return (CurrentState == EMediaState::Playing);
	}

	if (Control == EMediaControl::Resume)
	{
		return (CurrentState != EMediaState::Playing);
	}

	if ((Control == EMediaControl::Scrub) || (Control == EMediaControl::Seek))
	{
		return true;
	}

	return false;
}


FTimespan FImgMediaPlayer::GetDuration() const
{
	return CurrentDuration;
}


float FImgMediaPlayer::GetRate() const
{
	return CurrentRate;
}


EMediaState FImgMediaPlayer::GetState() const
{
	return CurrentState;
}


EMediaStatus FImgMediaPlayer::GetStatus() const
{
	return EMediaStatus::None;
}


TRangeSet<float> FImgMediaPlayer::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
	TRangeSet<float> Result;

	if (IsInitialized())
	{
		Result.Add(TRange<float>::Inclusive(-100000.0f, 100000.0f));
	}

	return Result;
}


FTimespan FImgMediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FImgMediaPlayer::IsLooping() const
{
	return ShouldLoop;
}


bool FImgMediaPlayer::Seek(const FTimespan& Time)
{
	// validate seek
	if (!IsInitialized())
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Cannot seek while player is not ready"));
		return false;
	}

	if ((Time < FTimespan::Zero()) || (Time >= CurrentDuration))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Invalid seek time %s (media duration is %s)"), *Time.ToString(), *CurrentDuration.ToString());
		return false;
	}

	// scrub to desired time if needed
	if (CurrentState == EMediaState::Stopped)
	{
		CurrentState = EMediaState::Paused;

		if (Loader.IsValid())
		{
			Loader->HandlePause();
		}
	}

#if IMG_MEDIA_PLAYER_VERSION == 1
	// more timing hacks for Sequencer
	CurrentTime = Time + HackDeltaTimeOffset;
	DeltaTimeHackApplied = true;

	if (CurrentTime == CurrentDuration)
	{
		CurrentTime -= HackDeltaTimeOffset;
	}
#else
	CurrentTime = Time;
#endif // IMG_MEDIA_PLAYER_VERSION == 1

	if (CurrentState == EMediaState::Paused)
	{
		Loader->RequestFrame(CurrentTime, CurrentRate, ShouldLoop);
	}

	LastFetchTime = FTimespan::MinValue();

	EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

	return true;
}


bool FImgMediaPlayer::SetLooping(bool Looping)
{
	ShouldLoop = Looping;

	return true;
}


bool FImgMediaPlayer::SetRate(float Rate)
{
	if (!IsInitialized())
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Cannot set play rate while player is not ready"));
		return false;
	}

	if (Rate == CurrentRate)
	{
		return true; // rate already set
	}

	if (CurrentDuration == FTimespan::Zero())
	{
		return false; // nothing to play
	}

	// handle restarting
	if ((CurrentRate == 0.0f) && (Rate != 0.0f))
	{
		if (CurrentState == EMediaState::Stopped)
		{
#if IMG_MEDIA_PLAYER_VERSION == 1
			if (Rate < 0.0f)
			{
				CurrentTime = CurrentDuration - FTimespan(1);
			}
#endif // IMG_MEDIA_PLAYER_VERSION == 1

			PlaybackRestarted = true;
		}

		CurrentRate = Rate;
		CurrentState = EMediaState::Playing;

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);

		return true;
	}
	
	// handle pausing
	if ((CurrentRate != 0.0f) && (Rate == 0.0f))
	{
		CurrentRate = Rate;
		CurrentState = EMediaState::Paused;

		if (Loader.IsValid())
		{
			Loader->HandlePause();
		}

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		return true;
	}

	CurrentRate = Rate;

	return true;
}

void FImgMediaPlayer::SetBlockingPlaybackHint(bool FacadeWillUseBlockingPlayback)
{
	PlaybackIsBlocking = FacadeWillUseBlockingPlayback;
}

/* IMediaSamples interface
 *****************************************************************************/

bool FImgMediaPlayer::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
#if IMG_MEDIA_PLAYER_VERSION == 1
	if ((CurrentState != EMediaState::Paused) && (CurrentState != EMediaState::Playing))
	{
		return false; // nothing to play
	}

	if (SelectedVideoTrack != 0)
	{
		return false; // no video track selected
	}

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample = Loader->GetFrameSample(CurrentTime);

	if (!Sample.IsValid())
	{
		return false; // sample not loaded yet
	}

	const FTimespan SampleTime = Sample->GetTime().Time;

	if (SampleTime == LastFetchTime)
	{
		return false; // sample already fetched
	}

	LastFetchTime = SampleTime;
	OutSample = Sample;

	return true;
#else // IMG_MEDIA_PLAYER_VERSION == 1
	return false;
#endif // IMG_MEDIA_PLAYER_VERSION == 1
}


void FImgMediaPlayer::FlushSamples()
{
	if (IsInitialized())
	{
		LastFetchTime = FTimespan::MinValue();
		Loader->ResetFetchLogic();
	}
}


IMediaSamples::EFetchBestSampleResult FImgMediaPlayer::FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bReverse)
{
	IMediaSamples::EFetchBestSampleResult SampleResult = EFetchBestSampleResult::NoSample;

	// The facade will keep on asking for frames, so don't do anything if we are stopped.
	if (Loader.IsValid() && IsInitialized() && (CurrentState != EMediaState::Stopped))
	{
		// See if we have any samples in the specified time range.
		SampleResult = Loader->FetchBestVideoSampleForTimeRange(TimeRange, OutSample, ShouldLoop, CurrentRate, PlaybackIsBlocking);
		Scheduler->TickInput(FTimespan::Zero(), FTimespan::MinValue());
		if (SampleResult == IMediaSamples::EFetchBestSampleResult::Ok)
		{
			CurrentTime = OutSample->GetTime().Time;
		}

		// Are we not looping?
		if (ShouldLoop == false)
		{
			// Are we at the end?
			bool bIsAtEnd = false;
			if (CurrentRate >= 0.0f)
			{
				bIsAtEnd = ((TimeRange.HasUpperBound()) &&
					(TimeRange.GetUpperBoundValue().Time >= CurrentDuration));
			}
			else
			{
				bIsAtEnd = ((TimeRange.HasLowerBound()) &&
					(TimeRange.GetLowerBoundValue().Time <= 0.0f));
			}
			if (bIsAtEnd)
			{
				// Stop the player.
				EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
				CurrentState = EMediaState::Stopped;
				CurrentRate = 0.0f;
				EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
			}
		}
	}
	return SampleResult;
}


bool FImgMediaPlayer::PeekVideoSampleTime(FMediaTimeStamp& TimeStamp)
{
	if (Loader.IsValid() && IsInitialized())
	{
		// Do we have the current frame?
		return Loader->PeekVideoSampleTime(TimeStamp, ShouldLoop, CurrentRate, GetTime());
	}
	return false;
}

/* IMediaTracks interface
 *****************************************************************************/

bool FImgMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	return false; // not supported
}


int32 FImgMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	return (Loader.IsValid() && (TrackType == EMediaTrackType::Video)) ? 1 : 0;
}


int32 FImgMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex == 0) && (GetNumTracks(TrackType) > 0)) ? 1 : 0;
}


int32 FImgMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video))
	{
		return INDEX_NONE;
	}

	return SelectedVideoTrack;
}


FText FImgMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FText::GetEmpty();
	}

	return LOCTEXT("DefaultVideoTrackName", "Video Track");
}


int32 FImgMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}


FString FImgMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FString();
	}

	return TEXT("und");
}


FString FImgMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FString();
	}

	return TEXT("VideoTrack");
}


bool FImgMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (!IsInitialized() || (TrackIndex != 0) || (FormatIndex != 0))
	{
		return false;
	}

	OutFormat.Dim = Loader->GetSequenceDim();
	OutFormat.FrameRate = Loader->GetSequenceFrameRate().AsDecimal();
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName = TEXT("Image"); // @todo gmp: fix me (should be image type)

	return true;
}


bool FImgMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video))
	{
		return false;
	}

	if ((TrackIndex != 0) && (TrackIndex != INDEX_NONE))
	{
		return false;
	}

	SelectedVideoTrack = TrackIndex;

	return true;
}


bool FImgMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return (IsInitialized() && (TrackIndex == 0) && (FormatIndex == 0));
}

#undef LOCTEXT_NAMESPACE
