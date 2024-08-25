// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ImgMediaPlayer.h"
#include "ImgMediaPrivate.h"
#include "ImgMediaSource.h"

#include "Async/Async.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

#include "Loader/ImgMediaLoader.h"
#include "Assets/ImgMediaMipMapInfo.h"
#include "Scheduler/ImgMediaScheduler.h"
#include "ImgMediaSettings.h"
#include "Player/ImgMediaTextureSample.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaTexture.h"
#include "MediaTextureTracker.h"

#define LOCTEXT_NAMESPACE "FImgMediaPlayer"


/** Time spent closing image media players. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Player Close"), STAT_ImgMedia_PlayerClose, STATGROUP_Media);

/** Time spent in image media player input tick. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Player TickInput"), STAT_ImgMedia_PlayerTickInput, STATGROUP_Media);


const FTimespan HackDeltaTimeOffset(1);


/* FImgMediaPlayer structors
 *****************************************************************************/

FImgMediaPlayer::FImgMediaPlayer(IMediaEventSink& InEventSink, const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
	const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache)
	: CurrentDuration(FTimespan::Zero())
	, CurrentRate(0.0f)
	, LastNonZeroRate(0.0f)
	, CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, CurrentSeekIndex(0)
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

	const TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe>& MipMapInfo = Loader->GetMipMapInfo();
	if (MipMapInfo.IsValid() && MediaTextureWeakPtr.IsValid())
	{
		MipMapInfo->RemoveObjectsUsingThisMediaTexture(MediaTextureWeakPtr.Get());
	}

	Scheduler->UnregisterLoader(Loader.ToSharedRef());
	Loader.Reset();

	CurrentDuration = FTimespan::Zero();
	CurrentUrl.Empty();
	CurrentRate = 0.0f;
	LastNonZeroRate = 0.0f;
	CurrentState = EMediaState::Closed;
	CurrentTime = FTimespan::Zero();
	CurrentSeekIndex = 0;
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
				// Look through all the media textures we know about to find the one tied to this player
				for (TWeakObjectPtr<UMediaTexture> TexturePtr : FMediaTextureTracker::Get().GetTextures())
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
								if (Player.Get() == this)
								{
									MediaTextureWeakPtr = TexturePtr;

									MipMapInfo->AddObjectsUsingThisMediaTexture(MediaTextureWeakPtr.Get());
								}
							}
						}
					}
				}
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

	// Tick the scheduler an extra time in addition to its hookup as media clock sink, so we also get it moving forward during blocked playback
	Scheduler->TickInput(FTimespan::Zero(), FTimespan::MinValue());
}

bool FImgMediaPlayer::FlushOnSeekStarted() const
{
	return true;
}

bool FImgMediaPlayer::FlushOnSeekCompleted() const
{
	return false;
}

void FImgMediaPlayer::ProcessVideoSamples()
{
}

//-----------------------------------------------------------------------------
/**
	Get special feature flags states
*/
bool FImgMediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
{
	switch (flag)
	{
	case EFeatureFlag::UsePlaybackTimingV2:
	case EFeatureFlag::PlayerUsesInternalFlushOnSeek:
		return true;
	default:
		break;
	}
	
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

	CurrentTime = Time;
	CurrentSeekIndex += (CurrentRate >= 0.0f) ? 1 : -1;
	if (Loader.IsValid())
	{
		Loader->Seek(FMediaTimeStamp(CurrentTime, FMediaTimeStamp::MakeSequenceIndex(CurrentSeekIndex, 0)), LastNonZeroRate < 0.0f);
	}

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
			PlaybackRestarted = true;
		}

		CurrentRate = Rate;
		LastNonZeroRate = Rate;
		CurrentState = EMediaState::Playing;

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);

		return true;
	}

	// handle pausing
	if ((CurrentRate != 0.0f) && (Rate == 0.0f))
	{
		LastNonZeroRate = CurrentRate;
		CurrentRate = Rate;
		CurrentState = EMediaState::Paused;

		if (Loader.IsValid())
		{
			Loader->HandlePause();
		}

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		return true;
	}

	if (Rate != 0.0f)
	{
		LastNonZeroRate = Rate;
	}
	CurrentRate = Rate;

	return true;
}

void FImgMediaPlayer::SetBlockingPlaybackHint(bool FacadeWillUseBlockingPlayback)
{
	if (PlaybackIsBlocking != FacadeWillUseBlockingPlayback)
	{
		PlaybackIsBlocking = FacadeWillUseBlockingPlayback;
		if (Loader.IsValid() && IsInitialized())
		{
			Loader->SetIsPlaybackBlocking(PlaybackIsBlocking);
		}
	}
}

/* IMediaSamples interface
 *****************************************************************************/

bool FImgMediaPlayer::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	return false;
}


void FImgMediaPlayer::FlushSamples()
{
	if (IsInitialized())
	{
		CurrentSeekIndex = 0;
		LastFetchTime = FTimespan::MinValue();
		Loader->Flush();
	}
}


bool FImgMediaPlayer::DiscardVideoSamples(const TRange<FMediaTimeStamp>& TimeRange, bool bReverse)
{
	if (Loader.IsValid() && IsInitialized())
	{
		return Loader->DiscardVideoSamples(TimeRange, ShouldLoop, CurrentRate);
	}
	return false;
}


IMediaSamples::EFetchBestSampleResult FImgMediaPlayer::FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bReverse, bool bConsistentResult)
{
	// note: the results produced by this player are "consistent" in respect to which frame is returned for a specific range, so we just disregard the bConsistentResult flag

	IMediaSamples::EFetchBestSampleResult SampleResult = EFetchBestSampleResult::NoSample;

	if (Loader.IsValid() && IsInitialized())
	{
		// See if we have any samples in the specified time range.
		SampleResult = Loader->FetchBestVideoSampleForTimeRange(TimeRange, OutSample, ShouldLoop, LastNonZeroRate);
		Scheduler->TickInput(FTimespan::Zero(), FTimespan::MinValue());
		if (SampleResult == IMediaSamples::EFetchBestSampleResult::Ok)
		{
			CurrentTime = OutSample->GetTime().Time;

			// Are we not looping?
			if (!ShouldLoop)
			{
				// Is this the last frame?
				if ((CurrentRate < 0.0f) ? Loader->IsFrameFirst(CurrentTime) : Loader->IsFrameLast(CurrentTime))
				{
					// Yes. Stop the player...
					EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
					CurrentState = EMediaState::Stopped;
					CurrentRate = 0.0f;
					LastNonZeroRate = 0.0f;
					EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
				}
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
		return Loader->PeekVideoSampleTime(TimeStamp, ShouldLoop, LastNonZeroRate, GetTime());
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
