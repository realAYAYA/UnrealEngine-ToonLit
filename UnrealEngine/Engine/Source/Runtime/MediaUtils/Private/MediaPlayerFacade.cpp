// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerFacade.h"
#include "MediaUtilsPrivate.h"

#include "HAL/PlatformMath.h"
#include "HAL/PlatformProcess.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaModule.h"
#include "IMediaOptions.h"
#include "IMediaPlayer.h"
#include "IMediaPlayerFactory.h"
#include "IMediaSamples.h"
#include "IMediaAudioSample.h"
#include "IMediaTextureSample.h"
#include "IMediaOverlaySample.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "IMediaTicker.h"
#include "MediaPlayerOptions.h"
#include "Math/NumericLimits.h"
#include "Misc/CoreMisc.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include "MediaHelpers.h"
#include "MediaSampleCache.h"
#include "MediaSampleQueueDepths.h"
#include "MediaSampleQueue.h"

#include "Async/Async.h"

#include <algorithm>

#define MEDIAPLAYERFACADE_DISABLE_BLOCKING 0
#define MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS 0


/** Time spent in media player facade closing media. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade Close"), STAT_MediaUtils_FacadeClose, STATGROUP_Media);

/** Time spent in media player facade opening media. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade Open"), STAT_MediaUtils_FacadeOpen, STATGROUP_Media);

/** Time spent in media player facade event processing. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade ProcessEvent"), STAT_MediaUtils_FacadeProcessEvent, STATGROUP_Media);

/** Time spent in media player facade fetch tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickFetch"), STAT_MediaUtils_FacadeTickFetch, STATGROUP_Media);

/** Time spent in media player facade input tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickInput"), STAT_MediaUtils_FacadeTickInput, STATGROUP_Media);

/** Time spent in media player facade output tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickOutput"), STAT_MediaUtils_FacadeTickOutput, STATGROUP_Media);

/** Time spent in media player facade high frequency tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickTickable"), STAT_MediaUtils_FacadeTickTickable, STATGROUP_Media);

/** Player time on main thread during last fetch tick. */
DECLARE_FLOAT_COUNTER_STAT(TEXT("MediaPlayerFacade PlaybackTime"), STAT_MediaUtils_FacadeTime, STATGROUP_Media);

/** Number of video samples currently in the queue. */
DECLARE_DWORD_COUNTER_STAT(TEXT("MediaPlayerFacade NumVideoSamples"), STAT_MediaUtils_FacadeNumVideoSamples, STATGROUP_Media);

/** Number of audio samples currently in the queue. */
DECLARE_DWORD_COUNTER_STAT(TEXT("MediaPlayerFacade NumAudioSamples"), STAT_MediaUtils_FacadeNumAudioSamples, STATGROUP_Media);

/** Number of purged video samples */
DECLARE_DWORD_COUNTER_STAT(TEXT("MediaPlayerFacade NumPurgedVideoSamples"), STAT_MediaUtils_FacadeNumPurgedVideoSamples, STATGROUP_Media);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("MediaPlayerFacade TotalPurgedVideoSamples"), STAT_MediaUtils_FacadeTotalPurgedVideoSamples, STATGROUP_Media);

/** Number of purged subtitle samples */
DECLARE_DWORD_COUNTER_STAT(TEXT("MediaPlayerFacade NumPurgedSubtitleSamples"), STAT_MediaUtils_FacadeNumPurgedSubtitleSamples, STATGROUP_Media);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("MediaPlayerFacade TotalPurgedSubtitleSamples"), STAT_MediaUtils_FacadeTotalPurgedSubtitleSamples, STATGROUP_Media);

/* Some constants
*****************************************************************************/

const double kMaxTimeSinceFrameStart = 0.300; // max seconds we allow between the start of the frame and the player facade timing computations (to catch suspended apps & debugging)
const double kMaxTimeSinceAudioTimeSampling = 0.250; // max seconds we allow to have passed between the last audio timing sampling and the player facade timing computations (to catch suspended apps & debugging - some platforms do update audio at a farily low rate: hence the big tollerance)
const double kOutdatedVideoSamplesTolerance = 0.050; // seconds video samples are allowed to be "too old" to stay in the player's output queue despite of calculations indicating they need to go
const double kOutdatedSubtitleSamplesTolerance = 0.050; // seconds subtitle samples are allowed to be "too old" to stay in the player's output queue despite of calculations indicating they need to go

/* Local helpers
*****************************************************************************/

namespace MediaPlayerFacade
{
	const FTimespan AudioPreroll = FTimespan::FromSeconds(1.0);
	const FTimespan MetadataPreroll = FTimespan::FromSeconds(1.0);
}

static FTimespan WrappedModulo(FTimespan Time, FTimespan Duration)
{
	return (Time >= FTimespan::Zero()) ? (Time % Duration) : (Duration + (Time % Duration));
}

/* FMediaPlayerFacade structors
*****************************************************************************/

FMediaPlayerFacade::FMediaPlayerFacade()
	: TimeDelay(FTimespan::Zero())
	, BlockOnRange(this)
	, Cache(new FMediaSampleCache)
	, LastRate(0.0f)
	, CurrentRate(0.0f)
	, bHaveActiveAudio(false)
	, VideoSampleAvailability(-1)
	, AudioSampleAvailability(-1)
	, bIsSinkFlushPending(false)
	, bAreEventsSafeForAnyThread(false)
{
	BlockOnRangeDisabled = false;

	MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
	bDidRecentPlayerHaveError = false;
}


FMediaPlayerFacade::~FMediaPlayerFacade()
{
	if (Player.IsValid())
	{
		{
			FScopeLock Lock(&CriticalSection);
			Player->Close();
		}
		NotifyLifetimeManagerDelegate_PlayerClosed();

		DestroyPlayer();
	}

	delete Cache;
	Cache = nullptr;
}


/* FMediaPlayerFacade interface
*****************************************************************************/

void FMediaPlayerFacade::AddAudioSampleSink(const TSharedRef<FMediaAudioSampleSink, ESPMode::ThreadSafe>& SampleSink)
{
	FScopeLock Lock(&CriticalSection);
	AudioSampleSinks.Add(SampleSink);
	PrimaryAudioSink = AudioSampleSinks.GetPrimaryAudioSink();
}


void FMediaPlayerFacade::AddCaptionSampleSink(const TSharedRef<FMediaOverlaySampleSink, ESPMode::ThreadSafe>& SampleSink)
{
	CaptionSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddMetadataSampleSink(const TSharedRef<FMediaBinarySampleSink, ESPMode::ThreadSafe>& SampleSink)
{
	FScopeLock Lock(&CriticalSection);
	MetadataSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddSubtitleSampleSink(const TSharedRef<FMediaOverlaySampleSink, ESPMode::ThreadSafe>& SampleSink)
{
	SubtitleSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddVideoSampleSink(const TSharedRef<FMediaTextureSampleSink, ESPMode::ThreadSafe>& SampleSink)
{
	VideoSampleSinks.Add(SampleSink);
}


bool FMediaPlayerFacade::CanPause() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetControls().CanControl(EMediaControl::Pause);
}


bool FMediaPlayerFacade::CanPlayUrl(const FString& Url, const IMediaOptions* Options)
{
	if (MediaModule == nullptr)
	{
		return false;
	}

	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		if (Factory->SupportsPlatform(RunningPlatformName) && Factory->CanPlayUrl(Url, Options))
		{
			return true;
		}
	}

	return false;
}


bool FMediaPlayerFacade::CanResume() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetControls().CanControl(EMediaControl::Resume);
}


bool FMediaPlayerFacade::CanScrub() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetControls().CanControl(EMediaControl::Scrub);
}


bool FMediaPlayerFacade::CanSeek() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetControls().CanControl(EMediaControl::Seek);
}


void FMediaPlayerFacade::Close()
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeClose);

	if (CurrentUrl.IsEmpty())
	{
		return;
	}

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (CurrentPlayer.IsValid())
	{
		{
			FScopeLock Lock(&CriticalSection);
			CurrentPlayer->Close();
		}
		NotifyLifetimeManagerDelegate_PlayerClosed();
	}

	BlockOnRange.Reset();

	Cache->Empty();
	CurrentUrl.Empty();
	LastRate = 0.0f;

	bHaveActiveAudio = false;
	VideoSampleAvailability = -1;
	AudioSampleAvailability = -1;
	bIsSinkFlushPending = false;
	bDidRecentPlayerHaveError = false;

	Flush();
}


uint32 FMediaPlayerFacade::GetAudioTrackChannels(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.NumChannels : 0;
}


uint32 FMediaPlayerFacade::GetAudioTrackSampleRate(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.SampleRate : 0;
}


FString FMediaPlayerFacade::GetAudioTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.TypeName : FString();
}


FTimespan FMediaPlayerFacade::GetDuration() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FTimespan::Zero();
	}
	return CurrentPlayer->GetControls().GetDuration();
}


const FGuid& FMediaPlayerFacade::GetGuid()
{
	return PlayerGuid;
}


FString FMediaPlayerFacade::GetInfo() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FString();
	}
	return CurrentPlayer->GetInfo();
}


FVariant FMediaPlayerFacade::GetMediaInfo(FName InfoName) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FVariant();
	}
	return CurrentPlayer->GetMediaInfo(InfoName);
}


FText FMediaPlayerFacade::GetMediaName() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FText::GetEmpty();
	}
	return CurrentPlayer->GetMediaName();
}


int32 FMediaPlayerFacade::GetNumTracks(EMediaTrackType TrackType) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return 0;
	}
	return CurrentPlayer->GetTracks().GetNumTracks(TrackType);
}


int32 FMediaPlayerFacade::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return 0;
	}
	return CurrentPlayer->GetTracks().GetNumTrackFormats(TrackType, TrackIndex);
}


FName FMediaPlayerFacade::GetPlayerName() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return NAME_None;
	}
	return MediaModule->GetPlayerFactory(CurrentPlayer->GetPlayerPluginGUID())->GetPlayerName();
}


float FMediaPlayerFacade::GetRate() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return 0.0f;
	}
	return CurrentPlayer->GetControls().GetRate();
}


int32 FMediaPlayerFacade::GetSelectedTrack(EMediaTrackType TrackType) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return INDEX_NONE;
	}
	return CurrentPlayer->GetTracks().GetSelectedTrack((EMediaTrackType)TrackType);
}


FString FMediaPlayerFacade::GetStats() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FString();
	}
	return CurrentPlayer->GetStats();
}


TRangeSet<float> FMediaPlayerFacade::GetSupportedRates(bool Unthinned) const
{
	const EMediaRateThinning Thinning = Unthinned ? EMediaRateThinning::Unthinned : EMediaRateThinning::Thinned;

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return TRangeSet<float>();
	}
	return CurrentPlayer->GetControls().GetSupportedRates(Thinning);
}


bool FMediaPlayerFacade::HaveVideoPlayback() const
{
	return VideoSampleSinks.Num() && GetSelectedTrack(EMediaTrackType::Video) != INDEX_NONE;
}


bool FMediaPlayerFacade::HaveAudioPlayback() const
{
	return PrimaryAudioSink.IsValid() && GetSelectedTrack(EMediaTrackType::Audio) != INDEX_NONE;
}


FTimespan FMediaPlayerFacade::GetTime() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);

	if (!CurrentPlayer.IsValid())
	{
		return FTimespan::Zero(); // no media opened
	}

	FTimespan Result;

	if (CurrentPlayer->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		// New style: framework controls timing - we use GetTimeStamp() and return the legacy part of the value
		FMediaTimeStamp TimeStamp = GetTimeStamp();
		return TimeStamp.IsValid() ? TimeStamp.Time : FTimespan::Zero();
	}
	else
	{
		// Old style: ask the player for timing
		Result = CurrentPlayer->GetControls().GetTime() - TimeDelay;
		if (Result.GetTicks() < 0)
		{
			Result = FTimespan::Zero();
		}
	}

	return Result;
}


FMediaTimeStamp FMediaPlayerFacade::GetTimeStamp() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FMediaTimeStamp();
	}

	FScopeLock Lock(&LastTimeValuesCS);

	if (!CurrentPlayer->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		// Make sure we can return values for V1 players...
		return FMediaTimeStamp(GetTime());
	}

	// Check if there are video samples present or presence is unknown.
	// Only when we know for sure that there are none because the existing video stream has ended do we set this to false.
	bool bHaveVideoSamples = VideoSampleAvailability != 0;

	if (HaveVideoPlayback() && bHaveVideoSamples)
	{
		/*
			Returning the precise time of the sample returned during TickFetch()
		*/
		return CurrentFrameVideoTimeStamp;
	}
	else if (HaveAudioPlayback())
	{
		/*
			We grab the last processed audio sample timestamp when it gets passed out to the sink(s) and keep it
			as "the value" for the frame (on the gamethread) -- an approximation, but better then having it return
			new values each time its called in one and the same frame...
		*/
		return CurrentFrameAudioTimeStamp;
	}

	// we assume video and/or audio to be present in any stream we play - otherwise: no time info
	// (at least for now)
	return FMediaTimeStamp();
}


FText FMediaPlayerFacade::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FText::GetEmpty();
	}
	return CurrentPlayer->GetTracks().GetTrackDisplayName((EMediaTrackType)TrackType, TrackIndex);
}


int32 FMediaPlayerFacade::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return INDEX_NONE;
	}
	return CurrentPlayer->GetTracks().GetTrackFormat((EMediaTrackType)TrackType, TrackIndex);
}


FString FMediaPlayerFacade::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return FString();
	}
	return CurrentPlayer->GetTracks().GetTrackLanguage((EMediaTrackType)TrackType, TrackIndex);
}


float FMediaPlayerFacade::GetVideoTrackAspectRatio(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return (GetVideoTrackFormat(TrackIndex, FormatIndex, Format) && (Format.Dim.Y != 0)) ? ((float)(Format.Dim.X) / Format.Dim.Y) : 0.0f;
}


FIntPoint FMediaPlayerFacade::GetVideoTrackDimensions(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.Dim : FIntPoint::ZeroValue;
}


float FMediaPlayerFacade::GetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.FrameRate : 0.0f;
}


TRange<float> FMediaPlayerFacade::GetVideoTrackFrameRates(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.FrameRates : TRange<float>::Empty();
}


FString FMediaPlayerFacade::GetVideoTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.TypeName : FString();
}


bool FMediaPlayerFacade::GetViewField(float& OutHorizontal, float& OutVertical) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetView().GetViewField(OutHorizontal, OutVertical);
}


bool FMediaPlayerFacade::GetViewOrientation(FQuat& OutOrientation) const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetView().GetViewOrientation(OutOrientation);
}


bool FMediaPlayerFacade::HasError() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return bDidRecentPlayerHaveError;
	}
	return (CurrentPlayer->GetControls().GetState() == EMediaState::Error);
}


bool FMediaPlayerFacade::IsBuffering() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return EnumHasAnyFlags(CurrentPlayer->GetControls().GetStatus(), EMediaStatus::Buffering);
}


bool FMediaPlayerFacade::IsConnecting() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return EnumHasAnyFlags(CurrentPlayer->GetControls().GetStatus(), EMediaStatus::Connecting);
}


bool FMediaPlayerFacade::IsLooping() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return CurrentPlayer->GetControls().IsLooping();
}


bool FMediaPlayerFacade::IsPaused() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return (CurrentPlayer->GetControls().GetState() == EMediaState::Paused);
}


bool FMediaPlayerFacade::IsPlaying() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return (CurrentPlayer->GetControls().GetState() == EMediaState::Playing);
}


bool FMediaPlayerFacade::IsPreparing() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return (CurrentPlayer->GetControls().GetState() == EMediaState::Preparing);
}

bool FMediaPlayerFacade::IsClosed() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}
	return (CurrentPlayer->GetControls().GetState() == EMediaState::Closed);
}

bool FMediaPlayerFacade::IsReady() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		return false;
	}

	EMediaState State = CurrentPlayer->GetControls().GetState();
	return ((State != EMediaState::Closed) &&
		(State != EMediaState::Error) &&
		(State != EMediaState::Preparing));
}

// ----------------------------------------------------------------------------------------------------------------------------------------------

class FMediaPlayerLifecycleManagerDelegateOpenRequest : public IMediaPlayerLifecycleManagerDelegate::IOpenRequest
{
public:
	FMediaPlayerLifecycleManagerDelegateOpenRequest(const FString& InUrl, const IMediaOptions* InOptions, const FMediaPlayerOptions* InPlayerOptions, IMediaPlayerFactory* InPlayerFactory, TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> InReusedPlayer, bool bInWillCreatePlayer, uint32 InWillUseNewResources)
		: Url(InUrl), Options(InOptions), PlayerFactory(InPlayerFactory), ReusedPlayer(InReusedPlayer), bWillCreatePlayer(bInWillCreatePlayer), NewResources(InWillUseNewResources)
	{
		if (InPlayerOptions)
		{
			PlayerOptions = *InPlayerOptions;
		}
	}

	virtual const FString& GetUrl() const override
	{
		return Url;
	}

	virtual const IMediaOptions* GetOptions() const override
	{
		return Options;
	}

	virtual const FMediaPlayerOptions* GetPlayerOptions() const override
	{
		return PlayerOptions.IsSet() ? &PlayerOptions.GetValue() : nullptr;
	}

	virtual IMediaPlayerFactory* GetPlayerFactory() const override
	{
		return PlayerFactory;
	}

	virtual bool WillCreateNewPlayer() const
	{
		return bWillCreatePlayer;
	}

	virtual bool WillUseNewResources(uint32 ResourceFlags) const
	{
		return !!(NewResources & ResourceFlags);
	}

	FString Url;
	const IMediaOptions* Options;
	TOptional<FMediaPlayerOptions> PlayerOptions;
	IMediaPlayerFactory* PlayerFactory;
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> ReusedPlayer;
	bool bWillCreatePlayer;
	uint32 NewResources;
};

class FMediaPlayerLifecycleManagerDelegateControl : public IMediaPlayerLifecycleManagerDelegate::IControl, public TSharedFromThis<FMediaPlayerLifecycleManagerDelegateControl, ESPMode::ThreadSafe>
{
public:
	FMediaPlayerLifecycleManagerDelegateControl(TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> InFacade) : Facade(InFacade), InstanceID(~0), SubmittedRequest(false) {}

	virtual ~FMediaPlayerLifecycleManagerDelegateControl()
	{
		if (!SubmittedRequest)
		{
			if (TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PinnedFacade = Facade.Pin())
			{
				PinnedFacade->ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			}
		}
	}

	virtual bool SubmitOpenRequest(IMediaPlayerLifecycleManagerDelegate::IOpenRequestRef&& OpenRequest) override
	{
		if (TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PinnedFacade = Facade.Pin())
		{
			const FMediaPlayerLifecycleManagerDelegateOpenRequest* OR = static_cast<const FMediaPlayerLifecycleManagerDelegateOpenRequest*>(OpenRequest.Get());
			if (PinnedFacade->ContinueOpen(AsShared(), OR->Url, OR->Options, OR->PlayerOptions.IsSet() ? &OR->PlayerOptions.GetValue() : nullptr, OR->PlayerFactory, OR->ReusedPlayer, OR->bWillCreatePlayer, InstanceID))
			{
				SubmittedRequest = true;
			}
			//note: we return "true" in all cases in which we were able to get to call "ContinueOpen". Failures in here will be messaged to the delegate using the OnMediaPlayerCreateFailed() method
			// (returning true here allows for capturing an unlikely early death of the facade while protecting us from double-handling the failure of the creation in the delegate)
			return true;
		}
		return false;
	}

	virtual TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> GetFacade() const override
	{
		return Facade.Pin();
	}

	virtual uint64 GetMediaPlayerInstanceID() const override
	{
		return InstanceID;
	}

	void SetInstanceID(uint64 InInstanceID)
	{
		InstanceID = InInstanceID;
	}

	void Reset()
	{
		SubmittedRequest = true;
	}

private:
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> Facade;
	uint64 InstanceID;
	bool SubmittedRequest;
};

// ----------------------------------------------------------------------------------------------------------------------------------------------

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerOpen(IMediaPlayerLifecycleManagerDelegate::IControlRef& NewLifecycleManagerDelegateControl, const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions, IMediaPlayerFactory* PlayerFactory, bool bWillCreatePlayer, uint32 WillUseNewResources, uint64 NewPlayerInstanceID)
{
	check(IsInGameThread() || IsInSlateThread());

	if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
	{
		NewLifecycleManagerDelegateControl = MakeShared<FMediaPlayerLifecycleManagerDelegateControl, ESPMode::ThreadSafe>(AsShared());
		if (NewLifecycleManagerDelegateControl.IsValid())
		{
			// Set instance ID we will use for a new player if we get the go-ahead to create it (old ID if player is about to be reused)
			static_cast<FMediaPlayerLifecycleManagerDelegateControl*>(NewLifecycleManagerDelegateControl.Get())->SetInstanceID(NewPlayerInstanceID);

			IMediaPlayerLifecycleManagerDelegate::IOpenRequestRef OpenRequest(new FMediaPlayerLifecycleManagerDelegateOpenRequest(Url, Options, PlayerOptions, PlayerFactory, !bWillCreatePlayer ? Player : TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>(), bWillCreatePlayer, WillUseNewResources));
			if (OpenRequest.IsValid())
			{
				if (Delegate->OnMediaPlayerOpen(NewLifecycleManagerDelegateControl, OpenRequest))
				{
					return true;
				}
			}
			static_cast<FMediaPlayerLifecycleManagerDelegateControl*>(NewLifecycleManagerDelegateControl.Get())->Reset();
		}
	}
	return false;
}

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerCreated()
{
	check(IsInGameThread() || IsInSlateThread());
	check(Player.IsValid());

	if (LifecycleManagerDelegateControl.IsValid())
	{
		if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
		{
			Delegate->OnMediaPlayerCreated(LifecycleManagerDelegateControl);
			return true;
		}
	}
	return false;
}

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerCreateFailed()
{
	check(IsInGameThread() || IsInSlateThread());

	if (LifecycleManagerDelegateControl.IsValid())
	{
		if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
		{
			Delegate->OnMediaPlayerCreateFailed(LifecycleManagerDelegateControl);
			return true;
		}
	}
	return false;
}

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerClosed()
{
	check(IsInGameThread() || IsInSlateThread());

	if (LifecycleManagerDelegateControl.IsValid())
	{
		if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
		{
			Delegate->OnMediaPlayerClosed(LifecycleManagerDelegateControl);
			return true;
		}
	}
	return false;
}

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerDestroyed()
{
	check(IsInGameThread() || IsInSlateThread());

	if (LifecycleManagerDelegateControl.IsValid())
	{
		if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
		{
			Delegate->OnMediaPlayerDestroyed(LifecycleManagerDelegateControl);
			return true;
		}
	}
	return false;
}

bool FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerResourcesReleased(uint32 ResourceFlags)
{
	check(IsInGameThread() || IsInSlateThread());

	if (LifecycleManagerDelegateControl.IsValid())
	{
		if (IMediaPlayerLifecycleManagerDelegate* Delegate = MediaModule->GetPlayerLifecycleManagerDelegate())
		{
			Delegate->OnMediaPlayerResourcesReleased(LifecycleManagerDelegateControl, ResourceFlags);
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------

void FMediaPlayerFacade::DestroyPlayer()
{
	FScopeLock Lock(&CriticalSection);

	if (!Player.IsValid())
	{
		return;
	}

	Player.Reset();
	NotifyLifetimeManagerDelegate_PlayerDestroyed();
	if (!PlayerUsesResourceReleaseNotification)
	{
		NotifyLifetimeManagerDelegate_PlayerResourcesReleased(IMediaPlayerLifecycleManagerDelegate::ResourceFlags_All);
	}
}

bool FMediaPlayerFacade::Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeOpen);

	ActivePlayerOptions.Reset();

	if (IsRunningDedicatedServer())
	{
		return false;
	}

	Close();

	if (Url.IsEmpty())
	{
		return false;
	}

	check(MediaModule);

	// find a player factory for the intended playback
	IMediaPlayerFactory* PlayerFactory = GetPlayerFactoryForUrl(Url, Options);
	if (PlayerFactory == nullptr)
	{
		return false;
	}

	IMediaPlayerFactory* OldFactory(Player.IsValid() ? MediaModule->GetPlayerFactory(Player->GetPlayerPluginGUID()) : nullptr);

	bool bWillCreatePlayer = (!Player.IsValid() || PlayerFactory != OldFactory);
	uint64 NewPlayerInstanceID;
	uint32 WillUseNewResources;

	if (bWillCreatePlayer)
	{
		NewPlayerInstanceID = MediaModule->CreateMediaPlayerInstanceID();
		WillUseNewResources = IMediaPlayerLifecycleManagerDelegate::ResourceFlags_All; // as we create a new player we assume all resources a newly created in any case
	}
	else
	{
		check(Player.IsValid());
		NewPlayerInstanceID = PlayerInstanceID;
		WillUseNewResources = Player->GetNewResourcesOnOpen(); // ask player what resources it will create again even if it already exists
	}

	IMediaPlayerLifecycleManagerDelegate::IControlRef NewLifecycleManagerDelegateControl;
	if (FMediaPlayerFacade::NotifyLifetimeManagerDelegate_PlayerOpen(NewLifecycleManagerDelegateControl, Url, Options, PlayerOptions, PlayerFactory, bWillCreatePlayer, WillUseNewResources, NewPlayerInstanceID))
	{
		// Assume all is well: the delegate will either (have) submit(ted) the request or not -- in any case we need to assume the best -> "true"
		return true;
	}

	// We did not notify successfully or the delegate will not submit the request in its own. Do so here...
	return ContinueOpen(NewLifecycleManagerDelegateControl, Url, Options, PlayerOptions, PlayerFactory, Player, bWillCreatePlayer, NewPlayerInstanceID);
}

bool FMediaPlayerFacade::ContinueOpen(IMediaPlayerLifecycleManagerDelegate::IControlRef NewLifecycleManagerDelegateControl, const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions, IMediaPlayerFactory* PlayerFactory, TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> ReusedPlayer, bool bCreateNewPlayer, uint64 NewPlayerInstanceID)
{
	// Create or reuse player
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> NewPlayer(bCreateNewPlayer ? PlayerFactory->CreatePlayer(*this) : ReusedPlayer);

	// Continue initialization ---------------------------------------

	if (NewPlayer != Player)
	{
		DestroyPlayer();

		class FAsyncResourceReleaseNotification : public IMediaPlayer::IAsyncResourceReleaseNotification
		{
		public:
			FAsyncResourceReleaseNotification(IMediaPlayerLifecycleManagerDelegate::IControlRef InDelegateControl) : DelegateControl(InDelegateControl) {}

			virtual void Signal(uint32 ResourceFlags) override
			{
				TFunction<void()> NotifyTask = [TargetDelegateControl = DelegateControl, ResourceFlags]()
				{
					// Get MediaModule & check if it is already unloaded...
					IMediaModule* TargetMediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
					if (TargetMediaModule)
					{
						// Delegate still there?
						if (IMediaPlayerLifecycleManagerDelegate* Delegate = TargetMediaModule->GetPlayerLifecycleManagerDelegate())
						{
							// Notify it!
							Delegate->OnMediaPlayerResourcesReleased(TargetDelegateControl, ResourceFlags);
						}
					}
				};
				Async(EAsyncExecution::TaskGraphMainThread, NotifyTask);
			};

			IMediaModule* MediaModule;
			IMediaPlayerLifecycleManagerDelegate::IControlRef DelegateControl;
		};

		FScopeLock Lock(&CriticalSection);
		Player = NewPlayer;
		PlayerInstanceID = NewPlayerInstanceID;
		LifecycleManagerDelegateControl = NewLifecycleManagerDelegateControl;
		PlayerUsesResourceReleaseNotification = LifecycleManagerDelegateControl.IsValid() ? Player->SetAsyncResourceReleaseNotification(TSharedRef<IMediaPlayer::IAsyncResourceReleaseNotification, ESPMode::ThreadSafe>(new FAsyncResourceReleaseNotification(LifecycleManagerDelegateControl))) : false;
	}
	else
	{
		LifecycleManagerDelegateControl = NewLifecycleManagerDelegateControl;
	}

	if (!Player.IsValid())
	{
		NotifyLifetimeManagerDelegate_PlayerCreateFailed();
		// Make sure we don't get called from the "tickable" thread anymore - no need as we have no player
		MediaModule->GetTicker().RemoveTickable(AsShared());
		return false;
	}

	// Make sure we get ticked on the "tickable" thread
	// (this will not re-add us, should we already be registered)
	MediaModule->GetTicker().AddTickable(AsShared());

	// update the Guid
	Player->SetGuid(PlayerGuid);

	CurrentUrl = Url;

	if (PlayerOptions)
	{
		ActivePlayerOptions = *PlayerOptions;
	}

	// open the new media source
	if (!Player->Open(Url, Options, PlayerOptions))
	{
		NotifyLifetimeManagerDelegate_PlayerCreateFailed();
		CurrentUrl.Empty();
		ActivePlayerOptions.Reset();

		return false;
	}

	{
		FScopeLock Lock(&LastTimeValuesCS);

		BlockOnRangeDisabled = false;
		BlockOnRange.Flush();
		LastVideoSampleProcessedTimeRange = TRange<FMediaTimeStamp>::Empty();
		LastAudioSampleProcessedTime.Invalidate();
		CurrentFrameVideoTimeStamp.Invalidate();
		CurrentFrameAudioTimeStamp.Invalidate();

		NextEstVideoTimeAtFrameStart.Invalidate();
	}

	if (bCreateNewPlayer)
	{
		NotifyLifetimeManagerDelegate_PlayerCreated();
	}

	return true;
}


void FMediaPlayerFacade::QueryCacheState(EMediaTrackType TrackType, EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Player.IsValid())
	{
		return;
	}

	if (State == EMediaCacheState::Cached)
	{
		if (TrackType == EMediaTrackType::Audio)
		{
			Cache->GetCachedAudioSampleRanges(OutTimeRanges);
		}
		else if (TrackType == EMediaTrackType::Video)
		{
			Cache->GetCachedVideoSampleRanges(OutTimeRanges);
		}
	}
	else
	{
		if (TrackType == EMediaTrackType::Video)
		{
			Player->GetCache().QueryCacheState(State, OutTimeRanges);
		}
	}
}


bool FMediaPlayerFacade::Seek(const FTimespan& Time)
{
	if (!Player.IsValid() || !Player->GetControls().Seek(Time))
	{
		return false;
	}

	if (Player.IsValid() && Player->FlushOnSeekStarted())
	{
		Flush(Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::PlayerUsesInternalFlushOnSeek));
	}

	return true;
}


bool FMediaPlayerFacade::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!Player.IsValid() || !Player->GetTracks().SelectTrack((EMediaTrackType)TrackType, TrackIndex))
	{
		return false;
	}

	if (!Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::IsTrackSwitchSeamless))
	{
		Flush();
	}
	return true;
}


void FMediaPlayerFacade::SetBlockOnTime(const FTimespan& Time)
{
#if !MEDIAPLAYERFACADE_DISABLE_BLOCKING
	if (!Player.IsValid() || !Player->GetControls().CanControl(EMediaControl::BlockOnFetch))
	{
		return;
	}

	if (Time == FTimespan::MinValue())
	{
		BlockOnRange.SetRange(TRange<FTimespan>::Empty());
		Player->GetControls().SetBlockingPlaybackHint(false);
	}
	else
	{
		TRange<FTimespan> Range;
		Range.Inclusive(Time, Time);
		BlockOnRange.SetRange(Range);
		Player->GetControls().SetBlockingPlaybackHint(true);
	}
#endif
}


void FMediaPlayerFacade::SetBlockOnTimeRange(const TRange<FTimespan>& TimeRange)
{
#if !MEDIAPLAYERFACADE_DISABLE_BLOCKING
	BlockOnRange.SetRange(TimeRange);
#endif
}


void FMediaPlayerFacade::FBlockOnRange::Flush()
{
	LastBlockOnRange = TRange<FTimespan>::Empty();
	OnBlockSeqIndex = 0;
}


void FMediaPlayerFacade::FBlockOnRange::SetRange(const TRange<FTimespan>& NewRange)
{
	CurrentTimeRange = NewRange;
	RangeIsDirty = true;
}


bool FMediaPlayerFacade::FBlockOnRange::IsSet() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Facade->Player);
	check(CurrentPlayer.IsValid());

	if (!RangeIsDirty)
	{
		return !BlockOnRange.IsEmpty();
	}
	return (!CurrentTimeRange.IsEmpty() && CurrentPlayer->GetControls().CanControl(EMediaControl::BlockOnFetch));
}


const TRange<FMediaTimeStamp>& FMediaPlayerFacade::FBlockOnRange::GetRange() const
{
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Facade->Player);
	check(CurrentPlayer.IsValid());

	if (!RangeIsDirty)
	{
		return BlockOnRange;
	}

	RangeIsDirty = false;

	if (CurrentTimeRange.IsEmpty() || !CurrentPlayer->GetControls().CanControl(EMediaControl::BlockOnFetch))
	{
		LastBlockOnRange = TRange<FTimespan>::Empty();
		BlockOnRange = TRange<FMediaTimeStamp>::Empty();
		CurrentPlayer->GetControls().SetBlockingPlaybackHint(false);
		return BlockOnRange;
	}

	EMediaState PlayerState = CurrentPlayer->GetControls().GetState();
	if (PlayerState != EMediaState::Paused && PlayerState != EMediaState::Playing)
	{
		// Return an empty range. Note that the "isSet()" method will still report a set block - so all code will remain in "external clock" mode,
		// but no samples will be requested (and no actual blocking should take place)
		static auto EmptyRange(TRange<FMediaTimeStamp>::Empty());
		return EmptyRange;
	}

	FTimespan Duration(CurrentPlayer->GetControls().GetDuration());
	FTimespan Start(CurrentTimeRange.GetLowerBoundValue());
	FTimespan End(CurrentTimeRange.GetUpperBoundValue());

	auto SetBlockOnRange = BlockOnRange;

	if (!CurrentPlayer->GetControls().IsLooping())
	{
		// We pass in the time range as is on seq-index zero at all times - players have to reject sample output / blocking logic will detect begin outside media range
		BlockOnRange = TRange<FMediaTimeStamp>(FMediaTimeStamp(Start, 0), FMediaTimeStamp(End, 0));
	}
	else
	{
		// If this would be called very early in the player's startup after open() we would not yet be known... that would be fatal
		/*
		 Should this actually happen in real-life applications, we could move the computations here into an accessor method used internally, so that this would be done
		 only if data is processed, which would also mean: we know the duration!
		 (Exception: live playback! --> but we would not allow blocking there anyway! (makes no sense as real life use case))
		*/
		check(!Duration.IsZero());
		if (Duration.IsZero())
		{
			// Catch if this is called to early and reset blocking...
			BlockOnRange = TRange<FMediaTimeStamp>::Empty();
			CurrentPlayer->GetControls().SetBlockingPlaybackHint(false);
			return BlockOnRange;
		}


		float Rate = Facade->GetUnpausedRate();

		// Modulo on the time to get it into media's range
		// (assumes zero-start-time)
		Start = WrappedModulo(Start, Duration);
		End = WrappedModulo(End, Duration);


		// Detect any non-monotonic movement of the range...
		if (!LastBlockOnRange.IsEmpty())
		{
			if (Rate >= 0.0f)
			{
				FTimespan LastStart = WrappedModulo(LastBlockOnRange.GetLowerBoundValue(), Duration);
				if ((LastStart > Start) || ((LastStart == Start) && (LastBlockOnRange.GetLowerBoundValue() < CurrentTimeRange.GetLowerBoundValue())))
				{
					++OnBlockSeqIndex;
				}
			}
			else
			{
				FTimespan LastEnd = WrappedModulo(LastBlockOnRange.GetUpperBoundValue(), Duration);
				if ((LastEnd < End) || ((LastEnd == End) && (LastBlockOnRange.GetUpperBoundValue() > CurrentTimeRange.GetUpperBoundValue())))
				{
					--OnBlockSeqIndex;
				}
			}
		}

		// Check if our range crosses the loop point (sequence boundary)
		int64 StartIndex, EndIndex;
		if (Rate >= 0.0)
		{
			StartIndex = OnBlockSeqIndex;
			EndIndex = (Start <= End) ? OnBlockSeqIndex : (OnBlockSeqIndex + 1);
		}
		else
		{
			StartIndex = (Start <= End) ? OnBlockSeqIndex : (OnBlockSeqIndex - 1);
			EndIndex = OnBlockSeqIndex;
		}

		// Assemble final blocking range
		BlockOnRange = TRange<FMediaTimeStamp>(FMediaTimeStamp(Start, StartIndex), FMediaTimeStamp(End, EndIndex));
		check(!BlockOnRange.IsEmpty());
	}

	// Does the new range overlap with the last one we set?
	if (!SetBlockOnRange.IsEmpty() && SetBlockOnRange.Overlaps(BlockOnRange))
	{
		// Yes, make sure the new range is setup so that it is a "correct" progression given the current playback direction...
		// (this may go so far as to undo any updates if the "new" one does not adds any range in the playback direction)
		if (CurrentPlayer->GetControls().GetRate() >= 0.0f)
		{
			BlockOnRange = TRange<FMediaTimeStamp>(std::max(BlockOnRange.GetLowerBoundValue(), SetBlockOnRange.GetLowerBoundValue()), std::max(BlockOnRange.GetUpperBoundValue(), SetBlockOnRange.GetUpperBoundValue()));
		}
		else
		{
			BlockOnRange = TRange<FMediaTimeStamp>(std::min(BlockOnRange.GetLowerBoundValue(), SetBlockOnRange.GetLowerBoundValue()), std::min(BlockOnRange.GetUpperBoundValue(), SetBlockOnRange.GetUpperBoundValue()));
		}
	}

	CurrentPlayer->GetControls().SetBlockingPlaybackHint(!BlockOnRange.IsEmpty());

	LastBlockOnRange = CurrentTimeRange;

	return BlockOnRange;
}


void FMediaPlayerFacade::SetCacheWindow(FTimespan Ahead, FTimespan Behind)
{
	Cache->SetCacheWindow(Ahead, Behind);
}


void FMediaPlayerFacade::SetGuid(FGuid& Guid)
{
	PlayerGuid = Guid;
}


bool FMediaPlayerFacade::SetLooping(bool Looping)
{
	return Player.IsValid() && Player->GetControls().SetLooping(Looping);
}


void FMediaPlayerFacade::SetMediaOptions(const IMediaOptions* Options)
{
}


bool FMediaPlayerFacade::SetRate(float Rate)
{
	// Enter CS as we change the rate which we read on the tickable thread
	FScopeLock Lock(&CriticalSection);

	// Can we set the rate at all?
	if (!Player.IsValid() || !Player->GetControls().SetRate(Rate))
	{
		return false;
	}

	// Is this new rate supported?
	if (Rate != 0.0f && !(Player->GetControls().GetSupportedRates(EMediaRateThinning::Thinned).Contains(Rate) || Player->GetControls().GetSupportedRates(EMediaRateThinning::Unthinned).Contains(Rate)))
	{
		return false;
	}

	if (CurrentRate == Rate)
	{
		// no change - just return with ok status
		return true;
	}

	if ((LastRate * Rate) < 0.0f)
	{
		Flush(); // direction change
	}
	else
	{
		if (Rate == 0.0f)
		{
			// Invalidate audio time on entering pause mode...
			if (TSharedPtr< FMediaAudioSampleSink, ESPMode::ThreadSafe> AudioSink = PrimaryAudioSink.Pin())
			{
				AudioSink->InvalidateAudioTime();
			}
		}
	}

	if (Rate != 0.0)
	{
		LastRate = Rate;
	}
	CurrentRate = Rate;

	return true;
}


bool FMediaPlayerFacade::SetNativeVolume(float Volume)
{
	if (!Player.IsValid())
	{
		return false;
	}

	return Player->SetNativeVolume(Volume);
}


bool FMediaPlayerFacade::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return Player.IsValid() ? Player->GetTracks().SetTrackFormat((EMediaTrackType)TrackType, TrackIndex, FormatIndex) : false;
}


bool FMediaPlayerFacade::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	return Player.IsValid() ? Player->GetTracks().SetVideoTrackFrameRate(TrackIndex, FormatIndex, FrameRate) : false;
}


bool FMediaPlayerFacade::SetViewField(float Horizontal, float Vertical, bool Absolute)
{
	return Player.IsValid() && Player->GetView().SetViewField(Horizontal, Vertical, Absolute);
}


bool FMediaPlayerFacade::SetViewOrientation(const FQuat& Orientation, bool Absolute)
{
	return Player.IsValid() && Player->GetView().SetViewOrientation(Orientation, Absolute);
}


bool FMediaPlayerFacade::SupportsRate(float Rate, bool Unthinned) const
{
	EMediaRateThinning Thinning = Unthinned ? EMediaRateThinning::Unthinned : EMediaRateThinning::Thinned;
	return Player.IsValid() && Player->GetControls().GetSupportedRates(Thinning).Contains(Rate);
}

void FMediaPlayerFacade::SetLastAudioRenderedSampleTime(FTimespan SampleTime)
{
	FScopeLock Lock(&LastTimeValuesCS);
	LastAudioRenderedSampleTime.TimeStamp = FMediaTimeStamp(SampleTime);
	LastAudioRenderedSampleTime.SampledAtTime = FPlatformTime::Seconds();
}

FTimespan FMediaPlayerFacade::GetLastAudioRenderedSampleTime() const
{
	FScopeLock Lock(&LastTimeValuesCS);
	return LastAudioRenderedSampleTime.TimeStamp.Time;
}

void FMediaPlayerFacade::SetAreEventsSafeForAnyThread(bool bInAreEventsSafeForAnyThread)
{
	bAreEventsSafeForAnyThread = bInAreEventsSafeForAnyThread;
}

/* FMediaPlayerFacade implementation
*****************************************************************************/

bool FMediaPlayerFacade::BlockOnFetch() const
{
	check(Player.IsValid());

	const TRange<FMediaTimeStamp>& BR = BlockOnRange.GetRange();

	if (BR.IsEmpty() || !Player->GetControls().CanControl(EMediaControl::BlockOnFetch) || BlockOnRangeDisabled)
	{
		return false; // no blocking requested / not supported
	}

	if (Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		//
		// V2 blocking logic
		//

		// If we have any active audio playback we skip any blocking
		if (HaveAudioPlayback())
		{
			return false;
		}

		float Rate = GetUnpausedRate();

		// If the current sample "out there" is actually overlapping with the current block, we might be good with no new sample
		if (LastVideoSampleProcessedTimeRange.Overlaps(BR))
		{
			// We have no new data (else we would not even call this method), but the last sample we returned is still inside the current range -> good, but...
			//
			// If the next sample would already cover more of the range than the older one we would like to use that instead -> but it may well be we do not have any data about the sample yet (and would indeed LIKE to block!)
			// So, we assume that the next sample will follow with no gap and have the same duration (a pretty good, general assumption) and check against that data to see if it would be better...

			// Get last sample's time range
			TRange<FMediaTimeStamp> LastSampleTimeRange(LastVideoSampleProcessedTimeRange);

			// Compute the "theoretical" next sample range...
			TRange<FMediaTimeStamp> NextSampleTimeRange = (Rate >= 0.0f) ? TRange<FMediaTimeStamp>(LastVideoSampleProcessedTimeRange.GetUpperBoundValue(), LastVideoSampleProcessedTimeRange.GetUpperBoundValue() + LastVideoSampleProcessedTimeRange.Size<FMediaTimeStamp>().Time)
				: TRange<FMediaTimeStamp>(LastVideoSampleProcessedTimeRange.GetLowerBoundValue() - LastVideoSampleProcessedTimeRange.Size<FMediaTimeStamp>().Time, LastVideoSampleProcessedTimeRange.GetLowerBoundValue());

			FTimespan Duration = Player->GetControls().GetDuration();

			if (!Player->GetControls().IsLooping())
			{
				// If we are not looping we need to clamp against the media's duration
				// (we assume it starts at zero here!)
				check(NextSampleTimeRange.GetLowerBoundValue().SequenceIndex == 0);
				NextSampleTimeRange = TRange<FMediaTimeStamp>::Intersection(NextSampleTimeRange, TRange<FMediaTimeStamp>(FMediaTimeStamp(0, 0), FMediaTimeStamp(Duration, 0)));
			}
			else
			{
				if (NextSampleTimeRange.GetLowerBoundValue().Time >= Duration)
				{
					check(Rate >= 0.0f);
					NextSampleTimeRange = TRange<FMediaTimeStamp>(FMediaTimeStamp(NextSampleTimeRange.GetLowerBoundValue().Time - Duration, NextSampleTimeRange.GetLowerBoundValue().SequenceIndex + 1), FMediaTimeStamp(NextSampleTimeRange.GetUpperBoundValue().Time - Duration, NextSampleTimeRange.GetUpperBoundValue().SequenceIndex + 1));
				}
				else if (NextSampleTimeRange.GetLowerBoundValue().Time < FTimespan::Zero())
				{
					check(Rate < 0.0f);
					NextSampleTimeRange = TRange<FMediaTimeStamp>(FMediaTimeStamp(NextSampleTimeRange.GetLowerBoundValue().Time + Duration, NextSampleTimeRange.GetLowerBoundValue().SequenceIndex - 1), FMediaTimeStamp(NextSampleTimeRange.GetUpperBoundValue().Time + Duration, NextSampleTimeRange.GetUpperBoundValue().SequenceIndex - 1));
				}
			}

			// Compute which one is larger inside the current range...
			int64 LastSampleCoverage = TRange<FMediaTimeStamp>::Intersection(BR, LastSampleTimeRange).Size<FMediaTimeStamp>().Time.GetTicks();
			int64 NextSampleCoverage = TRange<FMediaTimeStamp>::Intersection(BR, NextSampleTimeRange).Size<FMediaTimeStamp>().Time.GetTicks();

			if (LastSampleCoverage > NextSampleCoverage)
			{
				// Last one we returned is still good. No blocking needed...
				return false;
			}
		}

		// The next checks make only sense if the player is done preparing...
		if (!IsPreparing())
		{
			// Looping off?
			if (!Player->GetControls().IsLooping())
			{
				// Yes. Is the sample outside the media's range?
				// (note: this assumes the media starts at time ZERO - this will not be the case at all times (e.g. life playback) -- for now we assume a player will flagged blocked playback as invalid in that case!)
				if (BR.GetUpperBoundValue() < FMediaTimeStamp(0) || Player->GetControls().GetDuration() <= BR.GetLowerBoundValue().Time)
				{
					return false;
				}
			}
		}

		// Block until sample arrives!
		return true;
	}
	else
	{
		//
		// V1 blocking logic
		//

		if (IsPreparing())
		{
			return true; // block on media opening
		}

		if (!IsPlaying())
		{
			// no blocking if we are not playing (e.g. paused)
			return false;
		}

		if (CurrentRate < 0.0f)
		{
			return false; // block only in forward play
		}

		const bool VideoReady = (VideoSampleSinks.Num() == 0) || (BR.GetUpperBoundValue() < NextVideoSampleTime);

		if (VideoReady)
		{
			return false; // video is ready
		}

		return true;
	}

}


void FMediaPlayerFacade::Flush(bool bExcludePlayer)
{
	UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Flushing sinks"), this);

	FScopeLock Lock(&CriticalSection);

	AudioSampleSinks.Flush();
	CaptionSampleSinks.Flush();
	MetadataSampleSinks.Flush();
	SubtitleSampleSinks.Flush();
	VideoSampleSinks.Flush();

	if (Player.IsValid() && !bExcludePlayer)
	{
		Player->GetSamples().FlushSamples();
	}

	NextVideoSampleTime = FTimespan::MinValue();

	LastAudioRenderedSampleTime.Invalidate();
	LastVideoSampleProcessedTimeRange = TRange<FMediaTimeStamp>::Empty();
	BlockOnRange.Flush();

	NextEstVideoTimeAtFrameStart.Invalidate();
}


bool FMediaPlayerFacade::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = GetSelectedTrack(EMediaTrackType::Audio);
	}

	if (FormatIndex == INDEX_NONE)
	{
		FormatIndex = GetTrackFormat(EMediaTrackType::Audio, TrackIndex);
	}

	return (Player.IsValid() && Player->GetTracks().GetAudioTrackFormat(TrackIndex, FormatIndex, OutFormat));
}


IMediaPlayerFactory* FMediaPlayerFacade::GetPlayerFactoryForUrl(const FString& Url, const IMediaOptions* Options) const
{
	FName PlayerName;

	if (DesiredPlayerName != NAME_None)
	{
		PlayerName = DesiredPlayerName;
	}
	else if (Options != nullptr)
	{
		PlayerName = Options->GetDesiredPlayerName();
	}
	else
	{
		PlayerName = NAME_None;
	}

	if (MediaModule == nullptr)
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Failed to load Media module"));
		return nullptr;
	}

	//
	// Reuse existing player if explicitly requested name matches
	//
	if (Player.IsValid())
	{
		IMediaPlayerFactory* CurrentFactory = MediaModule->GetPlayerFactory(Player->GetPlayerPluginGUID());
		if (PlayerName == CurrentFactory->GetPlayerName())
		{
			return CurrentFactory;
		}
	}

	//
	// Try to create explicitly requested player
	//
	if (PlayerName != NAME_None)
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(PlayerName);

		if (Factory == nullptr)
		{
			UE_LOG(LogMediaUtils, Error, TEXT("Could not find desired player %s for %s"), *PlayerName.ToString(), *Url);
		}

		return Factory;
	}



	//
	// Try to find a fitting player with no explicit name given
	//


	// Can any existing player play the URL?
	if (Player.IsValid())
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(Player->GetPlayerPluginGUID());

		if ((Factory != nullptr) && Factory->CanPlayUrl(Url, Options))
		{
			// Yes...
			return Factory;
		}
	}

	// Try to auto-select new player...
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		if (!Factory->SupportsPlatform(RunningPlatformName) || !Factory->CanPlayUrl(Url, Options))
		{
			continue;
		}

		return Factory;
	}

	//
	// No suitable player found!
	//
	if (PlayerFactories.Num() > 0)
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Cannot play %s, because none of the enabled media player plug-ins support it:"), *Url);

		for (IMediaPlayerFactory* Factory : PlayerFactories)
		{
			if (Factory->SupportsPlatform(RunningPlatformName))
			{
				UE_LOG(LogMediaUtils, Log, TEXT("| %s (URI scheme or file extension not supported)"), *Factory->GetPlayerName().ToString());
			}
			else
			{
				UE_LOG(LogMediaUtils, Log, TEXT("| %s (only available on %s, but not on %s)"), *Factory->GetPlayerName().ToString(), *FString::Join(Factory->GetSupportedPlatforms(), TEXT(", ")), *RunningPlatformName);
			}
		}
	}
	else
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Cannot play %s: no media player plug-ins are installed and enabled in this project"), *Url);
	}

	return nullptr;
}


bool FMediaPlayerFacade::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = GetSelectedTrack(EMediaTrackType::Video);
	}

	if (FormatIndex == INDEX_NONE)
	{
		FormatIndex = GetTrackFormat(EMediaTrackType::Video, TrackIndex);
	}

	return (Player.IsValid() && Player->GetTracks().GetVideoTrackFormat(TrackIndex, FormatIndex, OutFormat));
}


void FMediaPlayerFacade::ProcessEvent(EMediaEvent Event, bool bIsBroadcastAllowed)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeProcessEvent);

	if (Event == EMediaEvent::TracksChanged)
	{
		SelectDefaultTracks();
	}
	else if ((Event == EMediaEvent::MediaOpened) || (Event == EMediaEvent::MediaOpenFailed))
	{
		if (Event == EMediaEvent::MediaOpenFailed)
		{
			CurrentUrl.Empty();
		}

		const FString MediaInfo = Player.IsValid() ? Player->GetInfo() : TEXT("");

		if (MediaInfo.IsEmpty())
		{
			UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Media Info: n/a"), this);
		}
		else
		{
			UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Media Info:\n%s"), this, *MediaInfo);
		}
	}

	if ((Event == EMediaEvent::PlaybackEndReached) ||
		(Event == EMediaEvent::TracksChanged))
	{
		Flush();
	}
	else if (Event == EMediaEvent::SeekCompleted)
	{
		if (!Player.IsValid() || Player->FlushOnSeekCompleted())
		{
			Flush(!Player.IsValid() || Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::PlayerUsesInternalFlushOnSeek));
		}
	}
	else if (Event == EMediaEvent::MediaClosed)
	{
		// Player still closed?
		if (CurrentUrl.IsEmpty())
		{
			// Yes, this also means: if we still have a player, it's still the one this event originated from

			// If player allows: close it down all the way right now
			if (Player.IsValid() && Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::AllowShutdownOnClose))
			{
				bDidRecentPlayerHaveError = HasError();
				DestroyPlayer();
			}

			// Stop issuing audio thread ticks until we open the player again
			MediaModule->GetTicker().RemoveTickable(AsShared());
		}
	}

	if (bIsBroadcastAllowed)
	{
		MediaEvent.Broadcast(Event);
	}
	else
	{
		QueuedEventBroadcasts.Enqueue(Event);
	}
}


void FMediaPlayerFacade::SelectDefaultTracks()
{
	if (!Player.IsValid())
	{
		return;
	}

	IMediaTracks& Tracks = Player->GetTracks();

	FMediaPlayerTrackOptions TrackOptions;
	if (ActivePlayerOptions.IsSet())
	{
		TrackOptions = ActivePlayerOptions.GetValue().Tracks;
	}

	Tracks.SelectTrack(EMediaTrackType::Audio, TrackOptions.Audio);
	Tracks.SelectTrack(EMediaTrackType::Caption, TrackOptions.Caption);
	Tracks.SelectTrack(EMediaTrackType::Metadata, TrackOptions.Metadata);
	Tracks.SelectTrack(EMediaTrackType::Subtitle, TrackOptions.Subtitle);
	Tracks.SelectTrack(EMediaTrackType::Video, TrackOptions.Video);
}


float FMediaPlayerFacade::GetUnpausedRate() const
{
	return (CurrentRate == 0.0f) ? LastRate : CurrentRate;
}


/* IMediaClockSink interface
*****************************************************************************/

void FMediaPlayerFacade::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickInput);

	if (Player.IsValid())
	{
		// Update flag reflecting presence of audio in the current stream
		// (doing it just once per gameloop is enough)
		bHaveActiveAudio = HaveAudioPlayback();

		Player->TickInput(DeltaTime, Timecode);

		bool bIsBroadcastAllowed = bAreEventsSafeForAnyThread || IsInGameThread();
		if (Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
		{
			//
			// New timing control (handled before any engine world, object etc. updates; so "all frame" (almost) see the state produced here)
			//

			// process deferred events
			// NOTE: if there is no player anymore we execute the remaining queued events in TickFetch (backwards compatibility - should move here once V1 support removed)
			EMediaEvent Event;
			if (bIsBroadcastAllowed)
			{
				while (QueuedEventBroadcasts.Dequeue(Event))
				{
					MediaEvent.Broadcast(Event);
				}
			}
			while (QueuedEvents.Dequeue(Event))
			{
				ProcessEvent(Event, bIsBroadcastAllowed);
			}

			// Handling events may have killed the player. Did it?
			if (!Player.IsValid())
			{
				// If so: nothing more to do!
				return;
			}

			if (bIsSinkFlushPending)
			{
				bIsSinkFlushPending = false;
				Flush();
			}

			//
			// Setup timing for sample processing
			//
			PreSampleProcessingTimeHandling();

			TRange<FMediaTimeStamp> TimeRange;
			if (!GetCurrentPlaybackTimeRange(TimeRange, CurrentRate, DeltaTime, false))
			{
				return;
			}

			SET_FLOAT_STAT(STAT_MediaUtils_FacadeTime, TimeRange.GetLowerBoundValue().Time.GetTotalSeconds());

			//
			// Process samples in range
			//
			IMediaSamples& Samples = Player->GetSamples();

			double BlockingStart = FPlatformTime::Seconds();
			while (1)
			{
				ProcessCaptionSamples(Samples, TimeRange);
				ProcessSubtitleSamples(Samples, TimeRange);

				if (ProcessVideoSamples(Samples, TimeRange))
				{
					break;
				}

				// No sample. Should we block for one?
				if (!BlockOnFetch())
				{
					// No... continue...
					break;
				}

				// Issue tick call with dummy timing as some players advance some state in the tick, which we wait for
				Player->TickInput(FTimespan::Zero(), FTimespan::MinValue());

				// Process deferred events & check for events that break the block
				bool bEventCancelsBlock = false;
				while (QueuedEvents.Dequeue(Event))
				{
					if (Event == EMediaEvent::MediaClosed || Event == EMediaEvent::MediaOpenFailed)
					{
						bEventCancelsBlock = true;
					}
					ProcessEvent(Event, bIsBroadcastAllowed);
				}

				// We might have lost the player during event handling or an event breaks the block...
				if (!Player.IsValid() || bEventCancelsBlock)
				{
					// Disable blocking feature for now (a new open would reset this)
					UE_LOG(LogMediaUtils, Warning, TEXT("Blocking media playback closed or failed. Disabling it for this playback session."));
					BlockOnRangeDisabled = true;
					break;
				}

				// Timeout?
				if ((FPlatformTime::Seconds() - BlockingStart) > MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS)
				{
					FString Url;
#if !UE_BUILD_SHIPPING
					Url = Player->GetUrl();
#endif // !UE_BUILD_SHIPPING
					UE_LOG(LogMediaUtils, Error, TEXT("Blocking media playback timed out. Disabling it for this playback session. URL:%s"),
						*Url);
					BlockOnRangeDisabled = true;
					break;
				}

				FPlatformProcess::Sleep(0.0f);
			}

			SET_DWORD_STAT(STAT_MediaUtils_FacadeNumVideoSamples, Samples.NumVideoSamples());

			//
			// Advance timing etc.
			//
			PostSampleProcessingTimeHandling(DeltaTime);

			if (bHaveActiveAudio)
			{
				// Keep currently last processed audio sample timestamp available for all frame (to provide consistent info)
				FScopeLock Lock(&LastTimeValuesCS);
				CurrentFrameAudioTimeStamp = LastAudioSampleProcessedTime.TimeStamp;
			}
		}

		// Check if primary audio sink needs a change and make sure invalid sinks are purged at all times
		PrimaryAudioSink = AudioSampleSinks.GetPrimaryAudioSink();
	}
}


void FMediaPlayerFacade::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickFetch);

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer(Player);
	if (!CurrentPlayer.IsValid())
	{
		// Send out deferred broadcasts.
		EMediaEvent Event;
		bool bIsBroadcastAllowed = bAreEventsSafeForAnyThread || IsInGameThread();
		if (bIsBroadcastAllowed)
		{
			while (QueuedEventBroadcasts.Dequeue(Event))
			{
				MediaEvent.Broadcast(Event);
			}
		}

		// process deferred events
		while (QueuedEvents.Dequeue(Event))
		{
			ProcessEvent(Event, bIsBroadcastAllowed);
		}
		return;
	}

	if (!CurrentPlayer->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		//
		// Old timing control
		//

		// let the player generate samples & process events
		CurrentPlayer->TickFetch(DeltaTime, Timecode);

		{
			// process deferred events
			EMediaEvent Event;
			while (QueuedEvents.Dequeue(Event))
			{
				ProcessEvent(Event, true);
			}
		}

		TRange<FTimespan> TimeRange;

		const FTimespan CurrentTime = GetTime();

		SET_FLOAT_STAT(STAT_MediaUtils_FacadeTime, CurrentTime.GetTotalSeconds());

		// get current play rate
		float Rate = GetUnpausedRate();

		if (Rate > 0.0f)
		{
			TimeRange = TRange<FTimespan>::AtMost(CurrentTime);
		}
		else if (Rate < 0.0f)
		{
			TimeRange = TRange<FTimespan>::AtLeast(CurrentTime);
		}
		else
		{
			TimeRange = TRange<FTimespan>(CurrentTime);
		}

		// process samples in range
		IMediaSamples& Samples = CurrentPlayer->GetSamples();

		bool Blocked = false;
		FDateTime BlockedTime;

		while (true)
		{
			ProcessCaptionSamples(Samples, TimeRange);
			ProcessSubtitleSamples(Samples, TimeRange);
			ProcessVideoSamplesV1(Samples, TimeRange);

			if (!BlockOnFetch())
			{
				break;
			}

			if (Blocked)
			{
				if ((FDateTime::UtcNow() - BlockedTime) >= FTimespan::FromSeconds(MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS))
				{
					UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Aborted block on fetch %s after %i seconds"),
						this,
						*BlockOnRange.GetRange().GetLowerBoundValue().Time.ToString(TEXT("%h:%m:%s.%t")),
						MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS
					);

					break;
				}
			}
			else
			{
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Blocking on fetch %s"), this, *BlockOnRange.GetRange().GetLowerBoundValue().Time.ToString(TEXT("%h:%m:%s.%t")));

				Blocked = true;
				BlockedTime = FDateTime::UtcNow();
			}

			FPlatformProcess::Sleep(0.0f);
		}
	}
}


void FMediaPlayerFacade::TickOutput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickOutput);

	if (!Player.IsValid())
	{
		return;
	}

	Cache->Tick(DeltaTime, CurrentRate, GetTime());
}


/* IMediaTickable interface
*****************************************************************************/

void FMediaPlayerFacade::TickTickable()
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickTickable);

	FScopeLock Lock(&CriticalSection);

	if (!Player.IsValid())
	{
		return;
	}

	float Rate = GetUnpausedRate();
	if (Rate == 0.0f)
	{
		return;
	}

	{
		FScopeLock Lock1(&LastTimeValuesCS);
		Player->SetLastAudioRenderedSampleTime(LastAudioRenderedSampleTime.TimeStamp.Time);
	}

	Player->TickAudio();

	// determine range of valid samples
	TRange<FTimespan> AudioTimeRange;
	TRange<FTimespan> MetadataTimeRange;

	const FTimespan Time = GetTime();

	bool bUseV2Timing = Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2);

	if (Rate > 0.0f)
	{
		if (!bUseV2Timing) // we leave range open - sends all the player has
		{
			AudioTimeRange = TRange<FTimespan>::Inclusive(FTimespan::MinValue(), Time + MediaPlayerFacade::AudioPreroll);
		}
		MetadataTimeRange = TRange<FTimespan>::Inclusive(FTimespan::MinValue(), Time + MediaPlayerFacade::MetadataPreroll);
	}
	else
	{
		if (!bUseV2Timing) // we leave range open - sends all the player has
		{
			AudioTimeRange = TRange<FTimespan>::Inclusive(Time - MediaPlayerFacade::AudioPreroll, FTimespan::MaxValue());
		}
		MetadataTimeRange = TRange<FTimespan>::Inclusive(Time - MediaPlayerFacade::MetadataPreroll, FTimespan::MaxValue());
	}

	// process samples in range
	IMediaSamples& Samples = Player->GetSamples();

	ProcessAudioSamples(Samples, AudioTimeRange);
	ProcessMetadataSamples(Samples, MetadataTimeRange);
	if (bUseV2Timing)
	{
		ProcessCaptionSamples(Samples, MetadataTimeRange);
		ProcessSubtitleSamples(Samples, MetadataTimeRange);
	}

	SET_DWORD_STAT(STAT_MediaUtils_FacadeNumAudioSamples, Samples.NumAudio());
}


void FMediaPlayerFacade::PreSampleProcessingTimeHandling()
{
	check(Player.IsValid());

	// No Audio clock?
	if (!bHaveActiveAudio)
	{
		// No external clock? (blocking)
		if (!BlockOnRange.IsSet())
		{
			// Do we have a current timestamp estimation?
			if (!NextEstVideoTimeAtFrameStart.IsValid())
			{
				// Not, yet. We need to attempt to get the next video sample's timestamp to get going...
				FMediaTimeStamp VideoTimeStamp;
				if (Player->GetSamples().PeekVideoSampleTime(VideoTimeStamp))
				{
					NextEstVideoTimeAtFrameStart = FMediaTimeStampSample(VideoTimeStamp, FPlatformTime::Seconds());
				}
			}
		}
	}
}


void FMediaPlayerFacade::PostSampleProcessingTimeHandling(FTimespan DeltaTime)
{
	check(Player.IsValid());

	float Rate = CurrentRate;

	// No Audio clock?
	if (!bHaveActiveAudio)
	{
		// No external clock? (blocking)
		if (!BlockOnRange.IsSet())
		{
			// Move video frame start estimate forward
			// (the initial NextEstVideoTimeAtFrameStart will never be valid if no video is present)
			if (!bHaveActiveAudio && NextEstVideoTimeAtFrameStart.IsValid())
			{
				if (Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UseRealtimeWithVideoOnly))
				{
					double NewBaseTime = FPlatformTime::Seconds();
					NextEstVideoTimeAtFrameStart.TimeStamp.Time += (NewBaseTime - NextEstVideoTimeAtFrameStart.SampledAtTime) * Rate;
					NextEstVideoTimeAtFrameStart.SampledAtTime = NewBaseTime;
				}
				else
				{
					NextEstVideoTimeAtFrameStart.TimeStamp.Time += DeltaTime * Rate;
				}

				// note: infinite duration (e.g. live playback - or players not yet supporting sequence indices on loops, when looping is enabled)
				// -> no need for special handling as FTimespan::MaxValue() is expected to be returned to signify this, which is quite "infinite" in practical terms
				FTimespan Duration = Player->GetControls().GetDuration();

				if (Player->GetControls().IsLooping())
				{
					if (Rate >= 0.0f)
					{
						if (NextEstVideoTimeAtFrameStart.TimeStamp.Time >= Duration)
						{
							NextEstVideoTimeAtFrameStart.TimeStamp.Time -= Duration;
							++NextEstVideoTimeAtFrameStart.TimeStamp.SequenceIndex;
						}
					}
					else
					{
						if (NextEstVideoTimeAtFrameStart.TimeStamp.Time < FTimespan::Zero())
						{
							NextEstVideoTimeAtFrameStart.TimeStamp.Time += Duration;
							--NextEstVideoTimeAtFrameStart.TimeStamp.SequenceIndex;
						}
					}
				}
				else
				{
					if (Rate >= 0.0f)
					{
						if (NextEstVideoTimeAtFrameStart.TimeStamp.Time >= Duration)
						{
							NextEstVideoTimeAtFrameStart.TimeStamp.Time = Duration - FTimespan::FromSeconds(0.0001);
						}
					}
					else
					{
						if (NextEstVideoTimeAtFrameStart.TimeStamp.Time < FTimespan::Zero())
						{
							NextEstVideoTimeAtFrameStart.TimeStamp.Time = FTimespan::Zero();
						}
					}
				}
			}
		}
	}
}


bool FMediaPlayerFacade::GetCurrentPlaybackTimeRange(TRange<FMediaTimeStamp>& TimeRange, float Rate, FTimespan DeltaTime, bool bDoNotUseFrameStartReference) const
{
	check(Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2));

	TSharedPtr<FMediaAudioSampleSink, ESPMode::ThreadSafe> AudioSink = PrimaryAudioSink.Pin();

	if (bHaveActiveAudio && AudioSink.IsValid())
	{
		//
		// Audio is available...
		//

		FMediaTimeStampSample AudioTime = AudioSink->GetAudioTime();
		if (!AudioTime.IsValid())
		{
			// No timing info available, no timerange available, no samples to process
			return false;
		}

		FMediaTimeStamp EstAudioTimeAtFrameStart;

		double Now = FPlatformTime::Seconds();

		if (!bDoNotUseFrameStartReference)
		{
			// Normal estimation relative to current frame start...
			// (on gamethread operation)

			check(IsInGameThread() || IsInSlateThread());

			double AgeOfFrameStart = Now - MediaModule->GetFrameStartTime();
			double AgeOfAudioTime = Now - AudioTime.SampledAtTime;

			if (AgeOfFrameStart >= 0.0 && AgeOfFrameStart <= kMaxTimeSinceFrameStart &&
				AgeOfAudioTime >= 0.0 && AgeOfAudioTime <= kMaxTimeSinceAudioTimeSampling)
			{
				// All realtime timestamps seem in sane ranges - we most likely did not have a lengthy interruption (suspended / debugging step)
				EstAudioTimeAtFrameStart = AudioTime.TimeStamp + FTimespan::FromSeconds((MediaModule->GetFrameStartTime() - AudioTime.SampledAtTime) * Rate);
			}
			else
			{
				// Realtime timestamps seem wonky. Proceed without them (worse estimation quality)
				EstAudioTimeAtFrameStart = AudioTime.TimeStamp;
			}
		}
		else
		{
			// Do not use frame start reference -> we compute relative to "now"
			// (for use off gamethread)
			EstAudioTimeAtFrameStart = AudioTime.TimeStamp + FTimespan::FromSeconds((Now - AudioTime.SampledAtTime) * Rate);
		}

		// Are we paused?
		if (Rate == 0.0f)
		{
			// Yes. We need to fetch a frame for the current display frame - once. Asking over and over until we get one...
			if (LastVideoSampleProcessedTimeRange.IsEmpty())
			{
				// We simply fake the rate to the last non-zero or 1.0 to fetch a frame fitting the time frame representing the whole current frame
				Rate = (LastRate == 0.0f) ? 1.0f : LastRate;
			}
		}

		TimeRange = (Rate >= 0.0f) ? TRange<FMediaTimeStamp>(EstAudioTimeAtFrameStart, EstAudioTimeAtFrameStart + DeltaTime * Rate)
			: TRange<FMediaTimeStamp>(EstAudioTimeAtFrameStart + DeltaTime * (1.0f + Rate), EstAudioTimeAtFrameStart + DeltaTime);
	}
	else
	{
		//
		// No Audio (no data and/or no sink)
		//
		if (!BlockOnRange.IsSet())
		{
			//
			// Internal clock (DT based)
			//

			// Do we now have a current timestamp estimation?
			if (!NextEstVideoTimeAtFrameStart.IsValid())
			{
				// No timing info available, no timerange available, no samples to process
				return false;
			}
			else
			{
				// Yes. Setup current time range & advance time estimation...

				// Are we paused?
				if (Rate == 0.0f)
				{
					// Yes. We need to fetch a frame for the current display frame - once. Asking over and over until we get one...
					if (LastVideoSampleProcessedTimeRange.IsEmpty())
					{
						// We simply fake the rate to the last non-zero or 1.0 to fetch a frame fitting the time frame representing the whole current frame
						Rate = (LastRate == 0.0f) ? 1.0f : LastRate;
					}
				}

				TimeRange = (Rate >= 0.0f) ? TRange<FMediaTimeStamp>(NextEstVideoTimeAtFrameStart.TimeStamp, NextEstVideoTimeAtFrameStart.TimeStamp + DeltaTime * Rate)
					: TRange<FMediaTimeStamp>(NextEstVideoTimeAtFrameStart.TimeStamp + DeltaTime * (1.0f + Rate), NextEstVideoTimeAtFrameStart.TimeStamp + DeltaTime);

				// If we are looping we check to prepare proper ranges should we wrap around either end of the media...
				// (we do not clamp in the non-looping case as the rest of the code should deal with that fine)
				if (Player->GetControls().IsLooping())
				{
					const FTimespan Duration = Player->GetControls().GetDuration();
					FTimespan WrappedStart = WrappedModulo(TimeRange.GetLowerBoundValue().Time, Duration);
					FTimespan WrappedEnd = WrappedModulo(TimeRange.GetUpperBoundValue().Time, Duration);
					if (WrappedStart > WrappedEnd)
					{
						if (Rate >= 0.0)
						{
							TimeRange.SetUpperBoundValue(FMediaTimeStamp(WrappedEnd, TimeRange.GetUpperBoundValue().SequenceIndex + 1));
						}
						else
						{
							TimeRange.SetLowerBoundValue(FMediaTimeStamp(WrappedStart, TimeRange.GetLowerBoundValue().SequenceIndex - 1));
						}
					}
				}
			}
		}
		else
		{
			//
			// External clock delivers time-range
			// (for now we just use the blocking time range as this clock type is solely used in that case)
			//
			TimeRange = BlockOnRange.GetRange();
		}
	}

	return !TimeRange.IsEmpty();
}


/* FMediaPlayerFacade implementation
*****************************************************************************/

void FMediaPlayerFacade::ProcessAudioSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> Sample;

	if (Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		// For V2 we basically expect to get no timerange at all: totally open
		// (we just have it around to be compatible / use older code that expects it)
		check(TimeRange.GetLowerBound().IsOpen() && TimeRange.GetUpperBound().IsOpen());

		//
		// "Modern" 1-Audio-Sink-Only case (aka: we only feed the primary sink)
		//
		if (TSharedPtr< FMediaAudioSampleSink, ESPMode::ThreadSafe> PinnedPrimaryAudioSink = PrimaryAudioSink.Pin())
		{
			while (PinnedPrimaryAudioSink->CanAcceptSamples(1))
			{
				if (!Samples.FetchAudio(TimeRange, Sample))
					break;

				if (!Sample.IsValid())
				{
					continue;
				}

				{
					FScopeLock Lock(&LastTimeValuesCS);
					LastAudioSampleProcessedTime.TimeStamp = FMediaTimeStamp(Sample->GetTime());
					LastAudioSampleProcessedTime.SampledAtTime = FPlatformTime::Seconds();
				}

				PinnedPrimaryAudioSink->Enqueue(Sample.ToSharedRef());
			}
		}
		else
		{
			// Do we have video playback?
			if (HaveVideoPlayback())
			{
				// We got video and audio, but no audio sink - throw away anything up to video playback time...
				// (rough estimate, as this is off-gamethread; but better than throwing things out with no throttling at all)
				{
					FScopeLock Lock(&LastTimeValuesCS);
					TimeRange.SetUpperBound(TRangeBound<FTimespan>(CurrentFrameVideoTimeStamp.Time));
				}
				while (Samples.FetchAudio(TimeRange, Sample))
					;
			}
			else
			{
				// No Video and no primary audio sink: we throw all away (sub-optimal as it will keep audio decoding busy; but this should be an edge case)
				while (Samples.FetchAudio(TimeRange, Sample))
					;
			}
		}
	}
	else
	{
		//
		// >1 Audio Sinks: we must drop samples that cause on overrun as SOME sinks will get it, some don't...
		// (mainly here to cover the "backwards compatibility" cases -> in the future we will probably only allow ONE AudioSink)
		//
		while (Samples.FetchAudio(TimeRange, Sample))
		{
			if (!Sample.IsValid())
			{
				continue;
			}

			if (!AudioSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxAudioSinkDepth))
			{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Audio sample sink overflow"), this);
#endif
			}
			else
			{
				FScopeLock Lock(&LastTimeValuesCS);
				LastAudioSampleProcessedTime.TimeStamp = FMediaTimeStamp(Sample->GetTime());
				LastAudioSampleProcessedTime.SampledAtTime = FPlatformTime::Seconds();
			}
		}
	}
}


void FMediaPlayerFacade::ProcessCaptionSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchCaption(TimeRange, Sample))
	{
		if (Sample.IsValid() && !CaptionSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxCaptionSinkDepth))
		{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Caption sample sink overflow"), this);
#endif
		}
	}
}


void FMediaPlayerFacade::ProcessMetadataSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchMetadata(TimeRange, Sample))
	{
		if (Sample.IsValid() && !MetadataSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxMetadataSinkDepth))
		{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Metadata sample sink overflow"), this);
#endif
		}
	}
}


void FMediaPlayerFacade::ProcessSubtitleSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchSubtitle(TimeRange, Sample))
	{
		//UE_LOG(LogMediaUtils, Display, TEXT("Subtitle @%.3f: %s"), Sample->GetTime().Time.GetTotalSeconds(), *Sample->GetText().ToString());
		if (Sample.IsValid() && !SubtitleSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxSubtitleSinkDepth))
		{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Subtitle sample sink overflow"), this);
#endif
		}
	}
}


void FMediaPlayerFacade::ProcessVideoSamplesV1(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	// Let the player do some processing if needed.
	if (Player.IsValid())
	{
		Player->ProcessVideoSamples();
	}

	// This is not to be used with V2 timing
	check(!Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2));

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchVideo(TimeRange, Sample))
	{
		if (!Sample.IsValid())
		{
			continue;
		}

		{
			FScopeLock Lock(&LastTimeValuesCS);
			CurrentFrameVideoTimeStamp = Sample->GetTime();
		}

		UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Fetched video sample %s"), this, *Sample->GetTime().Time.ToString(TEXT("%h:%m:%s.%t")));

		if (VideoSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxVideoSinkDepth))
		{
			if (CurrentRate >= 0.0f)
			{
				NextVideoSampleTime = Sample->GetTime().Time + Sample->GetDuration();
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Next video sample time %s"), this, *NextVideoSampleTime.ToString(TEXT("%h:%m:%s.%t")));
			}
		}
		else
		{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Video sample sink overflow"), this);
#endif
		}
	}
}


bool FMediaPlayerFacade::ProcessVideoSamples(IMediaSamples& Samples, const TRange<FMediaTimeStamp>& TimeRange)
{
	// Let the player do some processing if needed.
	if (Player.IsValid())
	{
		// note: avoid using this - it will be deprecated
		Player->ProcessVideoSamples();
	}

	// This is not to be used with V1 timing
	check(Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2));
	// We expect a fully closed range or we assume: nothing to do...
	check(TimeRange.GetLowerBound().IsClosed() && TimeRange.GetUpperBound().IsClosed());

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;

	if (!Player->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::AlwaysPullNewestVideoFrame))
	{
		//
		// Normal playback with timing control provided by MediaFramework
		//
		const bool bReverse = (GetUnpausedRate() < 0.0f);

		switch (Samples.FetchBestVideoSampleForTimeRange(TimeRange, Sample, bReverse))
		{
		case IMediaSamples::EFetchBestSampleResult::Ok:
			break;

		case IMediaSamples::EFetchBestSampleResult::NoSample:
		{
			break;
		}

		case IMediaSamples::EFetchBestSampleResult::NotSupported:
		{
			//
			// Fallback for players supporting V2 timing, but do not supply FetchBestVideoSampleForTimeRange() due to some
			// custom implementation of IMediaSamples (here to ease adoption of the new timing code - eventually should go away)
			//

			// Find newest sample that satisfies the time range
			// (the FetchXYZ() code does not work well with a lower range limit at all - we ask for a "up to" type range instead
			//  and limit the other side of the range in code here to not change the older logic & possibly cause trouble in old code)
			TRange<FMediaTimeStamp> TempRange = bReverse ? TRange<FMediaTimeStamp>::AtLeast(TimeRange.GetUpperBoundValue()) : TRange<FMediaTimeStamp>::AtMost(TimeRange.GetUpperBoundValue());
			while (Samples.FetchVideo(TempRange, Sample))
				;
			if (Sample.IsValid() &&
				((!bReverse && ((Sample->GetTime() + Sample->GetDuration()) > TimeRange.GetLowerBoundValue())) ||
					(bReverse && ((Sample->GetTime() - Sample->GetDuration()) < TimeRange.GetLowerBoundValue()))))
			{
				// Sample is good - nothing more to do here
			}
			else
			{
				Sample.Reset();
			}
			break;
		}
		}
	}
	else
	{
		//
		// Use newest video frame available at all times (no Mediaframework timing control)
		//
		TRange<FMediaTimeStamp> TempRange; // fully open range
		while (Samples.FetchVideo(TempRange, Sample))
			;
	}

	// Any sample?
	if (Sample.IsValid())
	{
		// Yes. If we are in blocking playback mode we need to make sure that the sample is really in the range we asked for and block on...
		// (same players might return an older sample as stop-gap measure if nothing can be found in the current range)

		TRange<FMediaTimeStamp> SampleTimeRange(Sample->GetTime(), Sample->GetTime() + Sample->GetDuration());

		// Is it what we want?
		const TRange<FMediaTimeStamp>& BR = BlockOnRange.GetRange();
		if (BR.IsEmpty() || BR.Overlaps(SampleTimeRange))
		{
			// Enqueue the sample to render
			// (we use a queue to stay compatible with existing structure and older sinks - new sinks will read this single entry right away on the gamethread
			//  and pass it along to rendering outside the queue)
			bool bOk = VideoSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxVideoSinkDepth);
			check(bOk);

			FScopeLock Lock(&LastTimeValuesCS);
			CurrentFrameVideoTimeStamp = SampleTimeRange.GetLowerBoundValue();
			LastVideoSampleProcessedTimeRange = SampleTimeRange;

			return true;
		}
	}
	return false;
}


void FMediaPlayerFacade::ProcessCaptionSamples(IMediaSamples& Samples, TRange<FMediaTimeStamp> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchCaption(TimeRange, Sample))
	{
		if (Sample.IsValid() && !CaptionSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxCaptionSinkDepth))
		{
#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Caption sample sink overflow"), this);
#endif
		}
	}
}


void FMediaPlayerFacade::ProcessSubtitleSamples(IMediaSamples& Samples, TRange<FMediaTimeStamp> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	// There might be samples in the subtitle sample queue that have may lie before the specified time range.
	// Since these have no way of being displayed they must be removed from the sample queue.
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer = Player;
	if (CurrentPlayer.IsValid() && CurrentPlayer->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		bool bReverse = CurrentRate < 0.0f;
		uint32 NumPurged = CurrentPlayer->GetSamples().PurgeOutdatedSubtitleSamples(TimeRange.GetLowerBoundValue() + (bReverse ? kOutdatedVideoSamplesTolerance : -kOutdatedVideoSamplesTolerance), bReverse);
		SET_DWORD_STAT(STAT_MediaUtils_FacadeNumPurgedSubtitleSamples, NumPurged);
		INC_DWORD_STAT_BY(STAT_MediaUtils_FacadeTotalPurgedSubtitleSamples, NumPurged);
	}

	while (Samples.FetchSubtitle(TimeRange, Sample))
	{
		if (Sample.IsValid() && !SubtitleSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxSubtitleSinkDepth))
		{
			FString Caption = Sample->GetText().ToString();
			UE_LOG(LogMediaUtils, Log, TEXT("New caption @%.3f: %s"), Sample->GetTime().Time.GetTotalSeconds(), *Caption);

#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Subtitle sample sink overflow"), this);
#endif
		}
	}
}


/* IMediaEventSink interface
*****************************************************************************/

void FMediaPlayerFacade::ReceiveMediaEvent(EMediaEvent Event)
{
	if (Event >= EMediaEvent::Internal_Start)
	{
		switch (Event)
		{
		case	EMediaEvent::Internal_PurgeVideoSamplesHint:
		{
			//
			// Player asks to attempt to purge older samples in the video output queue it maintains
			// (ask goes via facade as the player does not have accurate timing info)
			//
			TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CurrentPlayer = Player;

			if (!CurrentPlayer.IsValid())
			{
				return;
			}

			// We only support this for V2 timing players
			check(CurrentPlayer->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2));

			// Only do this if we do not block on time ranges
			if (BlockOnRange.IsSet())
			{
				// We do not purge as we do not need max perf, but max reliability to actually get certain frames
				return;
			}

			float Rate = CurrentRate;
			if (Rate == 0.0f)
			{
				return;
			}

			// Get current playback time
			// (note: we have DeltaTime forced to zero -> we just get a single value & we compute relative to "now", noty any game frame start)
			TRange<FMediaTimeStamp> TimeRange;
			if (!GetCurrentPlaybackTimeRange(TimeRange, Rate, FTimespan::Zero(), true))
			{
				return;
			}

			bool bReverse = (Rate < 0.0f);
			uint32 NumPurged = CurrentPlayer->GetSamples().PurgeOutdatedVideoSamples(TimeRange.GetLowerBoundValue() + (bReverse ? kOutdatedVideoSamplesTolerance : -kOutdatedVideoSamplesTolerance), bReverse);
			SET_DWORD_STAT(STAT_MediaUtils_FacadeNumPurgedVideoSamples, NumPurged);
			INC_DWORD_STAT_BY(STAT_MediaUtils_FacadeTotalPurgedVideoSamples, NumPurged);

			break;
		}

		case	EMediaEvent::Internal_ResetForDiscontinuity:
		{
			// Disabled for now to prevent a flush on the next handling iteration that might discard the samples a blocking range is waiting for.
			#if 0
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Reset for discontinuity"), this);
				bIsSinkFlushPending = true;
			#endif
			break;
		}
		case	EMediaEvent::Internal_RenderClockStart:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Render clock shall start"), this);
			break;
		}
		case	EMediaEvent::Internal_RenderClockStop:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Render clock shall stop"), this);
			break;
		}

		case	EMediaEvent::Internal_VideoSamplesAvailable:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Video samples ARE available"), this);
			VideoSampleAvailability = 1;
			break;
		}
		case	EMediaEvent::Internal_VideoSamplesUnavailable:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Video samples are NOT available"), this);
			VideoSampleAvailability = 0;
			break;
		}
		case	EMediaEvent::Internal_AudioSamplesAvailable:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Audio samples ARE available"), this);
			AudioSampleAvailability = 1;
			break;
		}
		case	EMediaEvent::Internal_AudioSamplesUnavailable:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Audio samples are NOT available"), this);
			AudioSampleAvailability = 0;
			break;
		}

		default:
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Received media event %s"), this, *MediaUtils::EventToString(Event));
			break;
		}
		}
	}
	else
	{
		UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Received media event %s"), this, *MediaUtils::EventToString(Event));
		QueuedEvents.Enqueue(Event);
	}
}
