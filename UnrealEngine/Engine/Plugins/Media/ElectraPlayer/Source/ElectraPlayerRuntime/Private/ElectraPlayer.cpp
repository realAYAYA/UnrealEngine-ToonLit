// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraPlayer.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPlatform.h"
#include "Misc/Optional.h"

#include "PlayerRuntimeGlobal.h"
#include "Player/AdaptiveStreamingPlayer.h"
#include "Renderer/RendererVideo.h"
#include "Renderer/RendererAudio.h"
#include "MediaMetaDataDecoderOutput.h"
#include "VideoDecoderResourceDelegate.h"
#include "Utilities/Utilities.h"

#include "Async/Async.h"

#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"

//-----------------------------------------------------------------------------

CSV_DEFINE_CATEGORY_MODULE(ELECTRAPLAYERRUNTIME_API, ElectraPlayer, false);

DECLARE_CYCLE_STAT(TEXT("FElectraPlayer::TickInput"), STAT_ElectraPlayer_ElectraPlayer_TickInput, STATGROUP_ElectraPlayer);

//-----------------------------------------------------------------------------

#define USE_INTERNAL_PLAYBACK_STATE 1

//-----------------------------------------------------------------------------

#if UE_BUILD_SHIPPING
#define HIDE_URLS_FROM_LOG	1
#else
#define HIDE_URLS_FROM_LOG	0
#endif

static FString SanitizeMessage(FString InMessage)
{
#if !HIDE_URLS_FROM_LOG
	return MoveTemp(InMessage);
#else
	int32 searchPos = 0;
	while(1)
	{
		static FString SchemeStr(TEXT("://"));
		static FString DotDotDotStr(TEXT("..."));
		static FString TermChars(TEXT("'\",; "));
		int32 schemePos = InMessage.Find(SchemeStr, ESearchCase::IgnoreCase, ESearchDir::FromStart, searchPos);
		if (schemePos != INDEX_NONE)
		{
			schemePos += SchemeStr.Len();
			// There may be a generic user message following a potential URL that we do not want to clobber.
			// We search for any next character that tends to end a URL in a user message, like one of ['",; ]
			int32 EndPos = InMessage.Len();
			int32 Start = schemePos;
			while(Start < EndPos)
			{
				int32 pos;
				if (TermChars.FindChar(InMessage[Start], pos))
				{
					break;
				}
				++Start;
			}
			InMessage.RemoveAt(schemePos, Start-schemePos);
			InMessage.InsertAt(schemePos, DotDotDotStr);
			searchPos = schemePos + SchemeStr.Len();
		}
		else
		{
			break;
		}
	}
	return InMessage;
#endif
}


//-----------------------------------------------------------------------------

class FMetaDataDecoderOutput : public IMetaDataDecoderOutput
{
public:
	virtual ~FMetaDataDecoderOutput() = default;
	virtual const void* GetData() override									{ return Data.GetData(); }
	virtual FTimespan GetDuration() const override							{ return Duration; }
	virtual uint32 GetSize() const override									{ return (uint32) Data.Num(); }
	virtual FDecoderTimeStamp GetTime() const override						{ return PresentationTime; }
	virtual EOrigin GetOrigin() const override								{ return Origin; }
	virtual EDispatchedMode GetDispatchedMode() const override				{ return DispatchedMode; }
	virtual const FString& GetSchemeIdUri() const override					{ return SchemeIdUri; }
	virtual const FString& GetValue() const override						{ return Value; }
	virtual const FString& GetID() const override							{ return ID; }
	virtual TOptional<FDecoderTimeStamp> GetTrackBaseTime() const override	{ return TrackBaseTime; }

	TArray<uint8> Data;
	FDecoderTimeStamp PresentationTime;
	FTimespan Duration;
	EOrigin Origin;
	EDispatchedMode DispatchedMode;
	FString SchemeIdUri;
	FString Value;
	FString ID;
	TOptional<FDecoderTimeStamp> TrackBaseTime;
};

//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
/**
 *	Construction of new player
 */
FElectraPlayer::FElectraPlayer(const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& InAdapterDelegate,
					 FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
					 FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
					 FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
					 FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate)
	: AdapterDelegate(InAdapterDelegate)
	, SendAnalyticMetricsDelegate(InSendAnalyticMetricsDelegate)
	, SendAnalyticMetricsPerMinuteDelegate(InSendAnalyticMetricsPerMinuteDelegate)
	, ReportVideoStreamingErrorDelegate(InReportVideoStreamingErrorDelegate)
	, ReportSubtitlesMetricsDelegate(InReportSubtitlesFileMetricsDelegate)
{
	CSV_EVENT(ElectraPlayer, TEXT("Player Creation"));

	WaitForPlayerDestroyedEvent = FPlatformProcess::GetSynchEventFromPool(true);
	WaitForPlayerDestroyedEvent->Trigger();

	AppTerminationHandler = MakeSharedTS<Electra::FApplicationTerminationHandler>();
	AppTerminationHandler->Terminate = [this]() { CloseInternal(false); };
	Electra::AddTerminationNotificationHandler(AppTerminationHandler);

	FString	OSMinor;
	AnalyticsGPUType = InAdapterDelegate->GetVideoAdapterName().TrimStartAndEnd();
	FPlatformMisc::GetOSVersions(AnalyticsOSVersion, OSMinor);
	AnalyticsOSVersion.TrimStartAndEndInline();

	SendAnalyticMetricsDelegate.AddRaw(this, &FElectraPlayer::SendAnalyticMetrics);
	SendAnalyticMetricsPerMinuteDelegate.AddRaw(this, &FElectraPlayer::SendAnalyticMetricsPerMinute);
	ReportVideoStreamingErrorDelegate.AddRaw(this, &FElectraPlayer::ReportVideoStreamingError);
	ReportSubtitlesMetricsDelegate.AddRaw(this, &FElectraPlayer::ReportSubtitlesMetrics);

#if USE_INTERNAL_PLAYBACK_STATE
	PlayerState.bUseInternal = true;
#endif
	bAllowKillAfterCloseEvent = false;
	bPlayerHasClosed = false;
	bHasPendingError = false;
	AnalyticsInstanceEventCount = 0;
	NumQueuedAnalyticEvents = 0;

	StaticResourceProvider = MakeShared<FAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe>(AdapterDelegate);
	VideoDecoderResourceDelegate = PlatformCreateVideoDecoderResourceDelegate(AdapterDelegate);

	ClearToDefaultState();

	UE_LOG(LogElectraPlayer, Log, TEXT("[%p] FElectraPlayer() created."), this);
}

//-----------------------------------------------------------------------------
/**
 *	Cleanup destructor
 */
FElectraPlayer::~FElectraPlayer()
{
	Electra::RemoveTerminationNotificationHandler(AppTerminationHandler);
	AppTerminationHandler.Reset();

	CloseInternal(false);
	WaitForPlayerDestroyedEvent->Wait();

	CSV_EVENT(ElectraPlayer, TEXT("Player Destruction"));

	SendAnalyticMetricsDelegate.RemoveAll(this);
	SendAnalyticMetricsPerMinuteDelegate.RemoveAll(this);
	ReportVideoStreamingErrorDelegate.RemoveAll(this);
	ReportSubtitlesMetricsDelegate.RemoveAll(this);
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p] ~FElectraPlayer() finished."), this);

	if (AsyncResourceReleaseNotification.IsValid())
	{
		AsyncResourceReleaseNotification->Signal(ResourceFlags_OutputBuffers);
	}

	FPlatformProcess::ReturnSynchEventToPool(WaitForPlayerDestroyedEvent);
}


void FElectraPlayer::ClearToDefaultState()
{
	FScopeLock lock(&PlayerLock);

	PlayerState.Reset();
	NumTracksAudio = 0;
	NumTracksVideo = 0;
	NumTracksSubtitle = 0;
	SelectedQuality = 0;
	SelectedVideoTrackIndex = -1;
	SelectedAudioTrackIndex = -1;
	SelectedSubtitleTrackIndex = -1;
	bVideoTrackIndexDirty = true;
	bAudioTrackIndexDirty = true;
	bSubtitleTrackIndexDirty = true;
	bInitialSeekPerformed = false;
	bDiscardOutputUntilCleanStart = false;
	LastPresentedFrameDimension = FIntPoint::ZeroValue;
	CurrentlyActiveVideoStreamFormat.Reset();
	DeferredPlayerEvents.Empty();
	MediaUrl.Empty();
}


//-----------------------------------------------------------------------------
/**
 *	Open player
 */
bool FElectraPlayer::OpenInternal(const FString& Url, const FParamDict& PlayerOptions, const FPlaystartOptions& InPlaystartOptions)
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);
	CSV_EVENT(ElectraPlayer, TEXT("Open"));

	CloseInternal(false);
	// Clear out our work variables
	ClearToDefaultState();
	bAllowKillAfterCloseEvent = false;
	bPlayerHasClosed = false;
	bHasPendingError = false;

	// Start statistics with a clean slate.
	Statistics.Reset();
	AnalyticsInstanceEventCount = 0;
	QueuedAnalyticEvents.Empty();
	NumQueuedAnalyticEvents = 0;
	// Create a guid string for the analytics. We do this here and not in the constructor in case the same instance is used over again.
	AnalyticsInstanceGuid = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	PlaystartOptions = InPlaystartOptions;

	// Create a new empty player structure. This contains the actual player instance, its associated renderers and sample queues.
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> NewPlayer = MakeShared<FInternalPlayerImpl, ESPMode::ThreadSafe>();

	// Get a writable copy of the URL so we can sanitize it if necessary.
	MediaUrl = Url;
	MediaUrl.TrimStartAndEndInline();
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] IMediaPlayer::Open(%s)"), this, NewPlayer.Get(), *SanitizeMessage(MediaUrl));

	// Create the renderers so we can pass them to the internal player.
	// They get a pointer to ourselves which they will call On[Video|Audio]Decoded() and On[Video|Audio]Flush() on.
	NewPlayer->RendererVideo = MakeShared<FElectraRendererVideo, ESPMode::ThreadSafe>(SharedThis(this));
	NewPlayer->RendererAudio = MakeShared<FElectraRendererAudio, ESPMode::ThreadSafe>(SharedThis(this));

	// Create the internal player and register ourselves as metrics receiver and static resource provider.
	IAdaptiveStreamingPlayer::FCreateParam CreateParams;
	CreateParams.VideoRenderer = NewPlayer->RendererVideo;
	CreateParams.AudioRenderer = NewPlayer->RendererAudio;
	CreateParams.ExternalPlayerGUID = PlayerGuid;
	NewPlayer->AdaptivePlayer = IAdaptiveStreamingPlayer::Create(CreateParams);
	NewPlayer->AdaptivePlayer->AddMetricsReceiver(this);
	NewPlayer->AdaptivePlayer->SetStaticResourceProviderCallback(StaticResourceProvider);
	NewPlayer->AdaptivePlayer->SetVideoDecoderResourceDelegate(VideoDecoderResourceDelegate);

	// Create the subtitle receiver and register it with the player.
	MediaPlayerSubtitleReceiver = MakeSharedTS<FSubtitleEventReceiver>();
	MediaPlayerSubtitleReceiver->GetSubtitleReceivedDelegate().BindRaw(this, &FElectraPlayer::OnSubtitleDecoded);
	MediaPlayerSubtitleReceiver->GetSubtitleFlushDelegate().BindRaw(this, &FElectraPlayer::OnSubtitleFlush);
	NewPlayer->AdaptivePlayer->AddSubtitleReceiver(MediaPlayerSubtitleReceiver);

	// Create a new media player event receiver and register it to receive all non player internal events as soon as they are received.
	MediaPlayerEventReceiver = MakeSharedTS<FAEMSEventReceiver>();
	MediaPlayerEventReceiver->GetEventReceivedDelegate().BindRaw(this, &FElectraPlayer::OnMediaPlayerEventReceived);
	NewPlayer->AdaptivePlayer->AddAEMSReceiver(MediaPlayerEventReceiver, TEXT("*"), TEXT(""), IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode::OnReceive);

	NewPlayer->AdaptivePlayer->Initialize(PlayerOptions);

	// Check for options that can be changed during playback and apply them at startup already.
	// If a media source supports the MaxResolutionForMediaStreaming option then we can override the max resolution.
	if (PlaystartOptions.MaxVerticalStreamResolution.IsSet())
	{
		NewPlayer->AdaptivePlayer->SetMaxResolution(0, PlaystartOptions.MaxVerticalStreamResolution.GetValue());
	}

	if (PlaystartOptions.MaxBandwidthForStreaming.IsSet())
	{
		NewPlayer->AdaptivePlayer->SetBitrateCeiling(PlaystartOptions.MaxBandwidthForStreaming.GetValue());
	}

	// Set the player member variable to the new player now that it has been fully initialized.
	CurrentPlayer = MoveTemp(NewPlayer);
	// Apply options that may have been set prior to calling Open().
	// Set these only if they have defined values as to not override what might have been set in the PlayerOptions.
	if (bFrameAccurateSeeking.IsSet())
	{
		SetFrameAccurateSeekMode(bFrameAccurateSeeking.GetValue());
	}
	if (bEnableLooping.IsSet())
	{
		SetLooping(bEnableLooping.GetValue());
	}
	if (CurrentPlaybackRange.Start.IsSet() || CurrentPlaybackRange.End.IsSet())
	{
		SetPlaybackRange(CurrentPlaybackRange);
	}

	// Issue load of the playlist.
	CurrentPlayer->AdaptivePlayer->LoadManifest(MediaUrl);
 	return true;
}

//-----------------------------------------------------------------------------
/**
 *	Close / Shutdown player
 */
void FElectraPlayer::CloseInternal(bool bKillAfterClose)
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	PlayerLock.Lock();
	if (bPlayerHasClosed || !CurrentPlayer.IsValid())
	{
		PlayerLock.Unlock();
		return;
	}
	bPlayerHasClosed = true;
	WaitForPlayerDestroyedEvent->Reset();
	PlayerLock.Unlock();


	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] IMediaPlayer::Close()"), this, CurrentPlayer.Get());
	CSV_EVENT(ElectraPlayer, TEXT("Close"));

	/*
	 * Closing the player is a delicate procedure because there are several worker threads involved
	 * that we need to make sure will not report back to us or deliver any pending data while we
	 * are cleaning everything up.
	 */
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe>	Player = CurrentPlayer;

	// For all intents and purposes the player can be considered closed here now already.
	PlayerState.State = EPlayerState::Closed;
	MediaUrl = FString();
	PlaystartOptions.TimeOffset.Reset();
	PlaystartOptions.InitialAudioTrackAttributes.Reset();
	CurrentPlaybackRange.Start.Reset();
	CurrentPlaybackRange.End.Reset();
	bFrameAccurateSeeking.Reset();
	bEnableLooping.Reset();

	// Next we detach ourselves from the renderers. This ensures we do not get any further data from them
	// via OnVideoDecoded() and OnAudioDecoded(). It also means we do not get any calls to OnVideoFlush() and OnAudioFlush()
	// and need to do this ourselves.
	if (Player->RendererVideo.IsValid())
	{
		Player->RendererVideo->DetachPlayer();
		// In addition we prepare the video renderer for decoder shutdown now.
		Player->RendererVideo->PrepareForDecoderShutdown();
	}
	if (Player->RendererAudio.IsValid())
	{
		Player->RendererAudio->DetachPlayer();
	}

	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		PinnedAdapterDelegate->PrepareForDecoderShutdown();
	}

	// Next up we clear out the sample queues.
	// NOTE that it is important we use the On..Flush() methods here and not simply clear out the queues.
	// The Flush() methods do more than that that is required to do and we don't need to duplicate that here.
	// Most notably they make sure all pending samples from MediaSamples are cleared.
	OnVideoFlush();
	OnAudioFlush();

	// Now that we should be clear of all samples and should also not be receiving any more we can tend
	// to the actual media player shutdown.
	check(Player->AdaptivePlayer.IsValid());
	if (Player->AdaptivePlayer.IsValid())
	{
		if (MediaPlayerEventReceiver.IsValid())
		{
			MediaPlayerEventReceiver->GetEventReceivedDelegate().Unbind();
			Player->AdaptivePlayer->RemoveAEMSReceiver(MediaPlayerEventReceiver, TEXT("*"), TEXT(""), IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode::OnStart);
			MediaPlayerEventReceiver.Reset();
		}

		if (MediaPlayerSubtitleReceiver.IsValid())
		{
			Player->AdaptivePlayer->RemoveSubtitleReceiver(MediaPlayerSubtitleReceiver);
			MediaPlayerSubtitleReceiver->GetSubtitleReceivedDelegate().Unbind();
			MediaPlayerSubtitleReceiver->GetSubtitleFlushDelegate().Unbind();
			MediaPlayerSubtitleReceiver.Reset();
		}

		// Unregister ourselves as the provider for static resources.
		Player->AdaptivePlayer->SetStaticResourceProviderCallback(nullptr);
		// Also unregister us from receiving further metric callbacks.
		// NOTE: This means we will not be receiving the final ReportPlaybackStopped() event, but that is on purpose!
		//       The closing of the player will be handled asynchronously by a thread and we must not be notified on
		//       *anything* any more. It is *very possible* that this instance here will be destroyed before the
		//       player is and any callback would only cause a crash.
		Player->AdaptivePlayer->RemoveMetricsReceiver(this);
	}

	// Clear any pending static resource requests now.
	StaticResourceProvider->ClearPendingRequests();

	// Pretend we got the playback stopped event via metrics, which we did not because we unregistered ourselves already.
	HandlePlayerEventPlaybackStopped();
	LogStatistics();
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::TracksChanged);
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaClosed);

	// Clear out the player instance now.
	CurrentPlayer.Reset();

	// Kick off asynchronous closing now.
	FInternalPlayerImpl::DoCloseAsync(MoveTemp(Player), AsyncResourceReleaseNotification);

	bAllowKillAfterCloseEvent = bKillAfterClose;

	WaitForPlayerDestroyedEvent->Trigger();
}

void FElectraPlayer::FInternalPlayerImpl::DoCloseAsync(TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> && Player, TSharedPtr<FElectraPlayer::IAsyncResourceReleaseNotifyContainer, ESPMode::ThreadSafe> AsyncResourceReleaseNotification)
{
	TSharedPtr<volatile bool> bClosedSig = MakeShared<volatile bool>(false);

	TFunction<void()> CloseTask = [Player, bClosedSig, AsyncResourceReleaseNotification]()
	{
		double TimeCloseBegan = FPlatformTime::Seconds();
		Player->AdaptivePlayer->Stop();
		Player->AdaptivePlayer.Reset();
		Player->RendererVideo.Reset();
		Player->RendererAudio.Reset();
		*bClosedSig = true;
		double TimeCloseEnded = FPlatformTime::Seconds();
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p] DoCloseAsync() finished after %.3f msec!"), Player.Get(), (TimeCloseEnded - TimeCloseBegan) * 1000.0);

		if (AsyncResourceReleaseNotification.IsValid())
		{
			AsyncResourceReleaseNotification->Signal(ResourceFlags_Decoder);
		}
	};

	// Fallback to simple, sequential execution if the engine is already shutting down...
	if (GIsRunning)
	{
		Async(EAsyncExecution::ThreadPool, MoveTemp(CloseTask));

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FInternalPlayerImpl* PlayerImpl = Player.Get();

			TFunction<void()> CheckTimeoutTask = [bClosedSig, PlayerImpl]()
			{
				// Wait for 3 seconds and if the player did not close by then log an error!
				FPlatformProcess::Sleep(3.0f);
				if (*bClosedSig == false)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("[%p] DoCloseAsync() did not complete in time. Player may be dead and dangling!"), PlayerImpl);
				}
			};
			Async(EAsyncExecution::Thread, MoveTemp(CheckTimeoutTask));
		#endif
	}
	else
	{
		CloseTask();
	}
}



float FElectraPlayer::FPlayerState::GetRate() const
{
	return bUseInternal && IntendedPlayRate.IsSet() ? IntendedPlayRate.GetValue() : CurrentPlayRate;
}

FElectraPlayer::EPlayerState FElectraPlayer::FPlayerState::GetState() const
{
	if (bUseInternal && IntendedPlayRate.IsSet() && (State == FElectraPlayer::EPlayerState::Playing || State == FElectraPlayer::EPlayerState::Paused || State == FElectraPlayer::EPlayerState::Stopped))
	{
		return IntendedPlayRate.GetValue() != 0.0f ? FElectraPlayer::EPlayerState::Playing : FElectraPlayer::EPlayerState::Paused;
	}
	return State;
}

FElectraPlayer::EPlayerStatus FElectraPlayer::FPlayerState::GetStatus() const
{
	return Status;
}

void FElectraPlayer::FPlayerState::SetIntendedPlayRate(float InIntendedRate)
{
	IntendedPlayRate = InIntendedRate;
}

void FElectraPlayer::FPlayerState::SetPlayRateFromPlayer(float InCurrentPlayerPlayRate)
{
	CurrentPlayRate = InCurrentPlayerPlayRate;
	// If reverse playback is selected even though it is not supported, leave it set as such.
	if (IntendedPlayRate.IsSet() && IntendedPlayRate.GetValue() >= 0.0f)
	{
		IntendedPlayRate.Reset();
	}
}


//-----------------------------------------------------------------------------
/**
* Suspends or resumes decoder instances.
*/
void FElectraPlayer::SuspendOrResumeDecoders(bool bSuspend)
{
	PlatformSuspendOrResumeDecoders(bSuspend);
}


//-----------------------------------------------------------------------------
/**
*/
void FElectraPlayer::NotifyOfOptionChange()
{
	PlatformNotifyOfOptionChange();
}

//-----------------------------------------------------------------------------
/**
*/
void FElectraPlayer::SetAsyncResourceReleaseNotification(IAsyncResourceReleaseNotifyContainer* InAsyncResourceReleaseNotification)
{
	AsyncResourceReleaseNotification = TSharedPtr<IAsyncResourceReleaseNotifyContainer, ESPMode::ThreadSafe>(InAsyncResourceReleaseNotification);
}

//-----------------------------------------------------------------------------
/**
*/
void FElectraPlayer::Tick(FTimespan DeltaTime, FTimespan Timecode)
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_ElectraPlayer_TickInput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, TickInput);

	// Handle the internal player, if we have one.
	PlayerLock.Lock();
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (!bPlayerHasClosed && Player.IsValid() && PlayerState.State != EPlayerState::Error)
	{
		if (Player->RendererVideo.IsValid())
		{
			Player->RendererVideo->TickInput(DeltaTime, Timecode);
		}

		// Handle static resource fetch requests.
		StaticResourceProvider->ProcessPendingStaticResourceRequests();

		// Check for option changes
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			FVariantValue Value = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::MaxVerticalStreamResolution);
			if (Value.IsValid())
			{
				int64 NewVerticalStreamResolution = Value.GetInt64();
				if (NewVerticalStreamResolution != PlaystartOptions.MaxVerticalStreamResolution.Get(0))
				{
					PlaystartOptions.MaxVerticalStreamResolution = NewVerticalStreamResolution;
					UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Limiting max vertical resolution to %d"), this, Player.Get(), (int32)NewVerticalStreamResolution);
					Player->AdaptivePlayer->SetMaxResolution(0, (int32)NewVerticalStreamResolution);
				}
			}
			Value = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::MaxBandwidthForStreaming);
			if (Value.IsValid())
			{
				int64 NewBandwidthForStreaming = Value.GetInt64();
				if (NewBandwidthForStreaming != PlaystartOptions.MaxBandwidthForStreaming.Get(0))
				{
					PlaystartOptions.MaxBandwidthForStreaming = NewBandwidthForStreaming;
					UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Limiting max streaming bandwidth to %d bps"), this, Player.Get(), (int32)NewBandwidthForStreaming);
					Player->AdaptivePlayer->SetBitrateCeiling((int32)NewBandwidthForStreaming);
				}
			}
		}

		// Process accumulated player events.
		HandleDeferredPlayerEvents();
		if (bHasPendingError)
		{
			bHasPendingError = false;
			if (PlayerState.State == EPlayerState::Preparing)
			{
				DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaOpenFailed);
			}
			else if (PlayerState.State == EPlayerState::Playing)
			{
				DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaClosed);
			}

			CloseInternal(true);
			PlayerState.State = EPlayerState::Error;
		}
	}
	else
	{
		DeferredPlayerEvents.Empty();
	}
	PlayerLock.Unlock();

	// Forward enqueued session events. We do this even with no current internal player to ensure all pending events are sent and none are lost.
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		IElectraPlayerAdapterDelegate::EPlayerEvent Event;
		while (DeferredEvents.Dequeue(Event))
		{
			PinnedAdapterDelegate->SendMediaEvent(Event);
		}
	}
}

//-----------------------------------------------------------------------------
/**
 * The video renderer is adding a buffer to the queue
 */
void FElectraPlayer::OnVideoDecoded(const FVideoDecoderOutputPtr& DecoderOutput, bool bDoNotRender)
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		if (PlayerState.State != EPlayerState::Closed)
		{
			if (DecoderOutput.IsValid())
			{
				if (!bDoNotRender)
				{
					PresentVideoFrame(DecoderOutput);
				}
			}
		}
	}
}

void FElectraPlayer::OnVideoFlush()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			PinnedAdapterDelegate->OnVideoFlush();
			// The event sink (player facade) needs to prepare for all new data.
			// We tell it only here and not also in OnAudioFlush() since both flushes occur at the same time
			// and there are always both the video and audio renderer. See OnAudioRenderingStarted()
			PinnedAdapterDelegate->SendMediaEvent(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_ResetForDiscontinuity);
		}
	}
}

//-----------------------------------------------------------------------------
/**
 * The audio renderer is adding a buffer to the queue
 *
 */
void FElectraPlayer::OnAudioDecoded(const IAudioDecoderOutputPtr& DecoderOutput)
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		if (PlayerState.State != EPlayerState::Closed)
		{
			PresentAudioFrame(DecoderOutput);
		}
	}
}

void FElectraPlayer::OnAudioFlush()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			PinnedAdapterDelegate->OnAudioFlush();
		}
	}
}

void FElectraPlayer::OnSubtitleDecoded(ISubtitleDecoderOutputPtr DecoderOutput)
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		if (PlayerState.State != EPlayerState::Closed)
		{
			PresentSubtitle(DecoderOutput);
		}
	}
}

void FElectraPlayer::OnSubtitleFlush()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			PinnedAdapterDelegate->OnSubtitleFlush();
		}
	}
}


void FElectraPlayer::OnVideoRenderingStarted()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			PinnedAdapterDelegate->SendMediaEvent(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_RenderClockStart);
		}
	}
}
void FElectraPlayer::OnVideoRenderingStopped()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.IsValid())
	{
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			PinnedAdapterDelegate->SendMediaEvent(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_RenderClockStop);
		}
	}
}

void FElectraPlayer::OnAudioRenderingStarted()
{
	// Note: We do nothing here because there is always a video renderer and an audio renderer and both
	//       get started and stopped at the same time. Thus we only need to relay the event from one of them.
}

void FElectraPlayer::OnAudioRenderingStopped()
{
	// Note: We do nothing here because there is always a video renderer and an audio renderer and both
	//       get started and stopped at the same time. Thus we only need to relay the event from one of them.
}


//-----------------------------------------------------------------------------
/**
 * Check timeline and moves sample over to media FACADE sinks
 *
 * Returns true if sample was moved over, but DOES not remove the sample from player queue
 */
bool FElectraPlayer::PresentVideoFrame(const FVideoDecoderOutputPtr& InVideoFrame)
{
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		PinnedAdapterDelegate->PresentVideoFrame(InVideoFrame);
		LastPresentedFrameDimension = InVideoFrame->GetOutputDim();
	}
	return true;
}

//-----------------------------------------------------------------------------
/**
 * Check timeline and moves sample over to media FACADE sinks
 *
 * Returns true if sample was moved over, but DOES not remove the sample from player queue
 */
bool FElectraPlayer::PresentAudioFrame(const IAudioDecoderOutputPtr& DecoderOutput)
{
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		PinnedAdapterDelegate->PresentAudioFrame(DecoderOutput);
	}
	return true;
}


bool FElectraPlayer::PresentSubtitle(const ISubtitleDecoderOutputPtr& DecoderOutput)
{
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		PinnedAdapterDelegate->PresentSubtitleSample(DecoderOutput);
	}
	return true;
}

//-----------------------------------------------------------------------------
/**
 * Attempt to drop any old frames from the presentation queue
*/
void FElectraPlayer::DropOldFramesFromPresentationQueue()
{
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		// We ask the event sink (player facade) to trigger this, as we don't have good enough timing info
		PinnedAdapterDelegate->SendMediaEvent(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_PurgeVideoSamplesHint);
	}
}

//-----------------------------------------------------------------------------
/**
 *
*/
bool FElectraPlayer::CanPresentVideoFrames(uint64 NumFrames)
{
	DropOldFramesFromPresentationQueue();
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		if (!bDiscardOutputUntilCleanStart)
		{
			return PinnedAdapterDelegate->CanReceiveVideoSamples(NumFrames);
		}
	}
	return false;
}

bool FElectraPlayer::CanPresentAudioFrames(uint64 NumFrames)
{
	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		if (!bDiscardOutputUntilCleanStart)
		{
			return PinnedAdapterDelegate->CanReceiveAudioSamples(NumFrames);
		}
	}
	return false;
}

float FElectraPlayer::GetRate() const
{
	return PlayerState.GetRate();
/*
	if (PlayerState.bUseInternal)
	{
		return 0.0f;
	}
	else
	{
		return CurrentPlayer.Get() && CurrentPlayer->AdaptivePlayer->IsPlaying() ? 1.0f : 0.0f;
	}
*/
}

FElectraPlayer::EPlayerState FElectraPlayer::GetState() const
{
	return PlayerState.GetState();
}

FElectraPlayer::EPlayerStatus FElectraPlayer::GetStatus() const
{
	return PlayerState.GetStatus();
}

bool FElectraPlayer::IsLooping() const
{
	if (CurrentPlayer.Get())
	{
		IAdaptiveStreamingPlayer::FLoopState loopState;
		CurrentPlayer->AdaptivePlayer->GetLoopState(loopState);
		return loopState.bIsEnabled;
	}
	return bEnableLooping.IsSet() ? bEnableLooping.GetValue() : false;
}

bool FElectraPlayer::SetLooping(bool bLooping)
{
	bEnableLooping = bLooping;
	if (CurrentPlayer.Get())
	{
		IAdaptiveStreamingPlayer::FLoopParam loop;
		loop.bEnableLooping = bLooping;
		CurrentPlayer->AdaptivePlayer->SetLooping(loop);
		UE_LOG(LogElectraPlayer, Verbose, TEXT("[%p][%p] IMediaPlayer::SetLooping(%s)"), this, CurrentPlayer.Get(), bLooping?TEXT("true"):TEXT("false"));
		return true;
	}
	return false;
}

int32 FElectraPlayer::GetLoopCount() const
{
	if (CurrentPlayer.Get())
	{
		IAdaptiveStreamingPlayer::FLoopState loopState;
		CurrentPlayer->AdaptivePlayer->GetLoopState(loopState);
		return (int32) loopState.Count;
	}
	return -1;
}


FTimespan FElectraPlayer::GetTime() const
{
	if (CurrentPlayer.Get())
	{
		Electra::FTimeValue playerTime = CurrentPlayer->AdaptivePlayer->GetPlayPosition();
		return playerTime.GetAsTimespan();
	}
	else
	{
		return FTimespan::Zero();
	}
}

FTimespan FElectraPlayer::GetDuration() const
{
	if (CurrentPlayer.Get())
	{
		Electra::FTimeValue playDuration = CurrentPlayer->AdaptivePlayer->GetDuration();
		if (playDuration.IsValid())
		{
			return playDuration.IsInfinity() ? FTimespan::MaxValue() : playDuration.GetAsTimespan();
		}
	}
	return FTimespan::Zero();
}

bool FElectraPlayer::IsLive() const
{
	if (CurrentPlayer.Get())
	{
		Electra::FTimeValue playDuration = CurrentPlayer->AdaptivePlayer->GetDuration();
		if (playDuration.IsValid())
		{
			return playDuration.IsInfinity();
		}
	}
	// Default assumption is Live playback.
	return true;
}

FTimespan FElectraPlayer::GetSeekableDuration() const
{
	if (CurrentPlayer.Get())
	{
		Electra::FTimeRange seekRange;
		CurrentPlayer->AdaptivePlayer->GetSeekableRange(seekRange);
		if (seekRange.IsValid())
		{
			// By definition here this is always positive, even for Live streams where we intend
			// to seek only backwards from the Live edge.
			return FTimespan((seekRange.End - seekRange.Start).GetAsHNS());
		}
	}
	return FTimespan::Zero();
}


bool FElectraPlayer::SetRate(float Rate)
{
	//UE_LOG(LogElectraPlayer, Verbose, TEXT("[%p][%p] IMediaPlayer::SetRate(%.3f)"), this, CurrentPlayer.Get(), Rate);
	if (CurrentPlayer.Get())
	{
		// Set the intended rate, which *may* be set negative. This is not supported and we put the adaptive player into pause
		// if this happens, but we keep the intended rate set nevertheless.
		PlayerState.SetIntendedPlayRate(Rate);
		if (Rate <= 0.0f)
		{
			CurrentPlayer->AdaptivePlayer->Pause();
		}
		else
		{
			if (CurrentPlayer->AdaptivePlayer->IsPaused() || !CurrentPlayer->AdaptivePlayer->IsPlaying())
			{
				TriggerFirstSeekIfNecessary();
				CurrentPlayer->AdaptivePlayer->Resume();
			}
		}
		return true;
	}
	return false;
}

void FElectraPlayer::TriggerFirstSeekIfNecessary()
{
	if (!bInitialSeekPerformed)
	{
		bInitialSeekPerformed = true;

		// Set up the initial playback position
		IAdaptiveStreamingPlayer::FSeekParam playParam;
		// First we look at any potential time offset specified in the playstart options.
		if (PlaystartOptions.TimeOffset.IsSet())
		{
			FTimespan Target;
			CalculateTargetSeekTime(Target, PlaystartOptions.TimeOffset.GetValue());
			playParam.Time.SetFromHNS(Target.GetTicks());
		}
		else
		{
			// Do not set a start time, let the player pick one.
			//playParam.Time.SetToZero();
		}

		// Next, give a list of the seekable positions to the delegate and ask it if it wants to seek to one of them,
		// overriding any potential time offset from above.
		TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
		if (PinnedAdapterDelegate.IsValid())
		{
			TSharedPtr<TArray<FTimespan>, ESPMode::ThreadSafe> SeekablePositions = MakeShared<TArray<FTimespan>, ESPMode::ThreadSafe>();
			if (SeekablePositions.IsValid())
			{
				CurrentPlayer->AdaptivePlayer->GetSeekablePositions(*SeekablePositions);

				// Check with the delegate if it wants to start somewhere else.
				FVariantValue Result = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::PlaystartPosFromSeekPositions, FVariantValue(SeekablePositions));
				if (Result.IsValid())
				{
					check(Result.IsType(FVariantValue::EDataType::TypeInt64));
					playParam.Time.SetFromHNS(Result.GetInt64());
				}
			}
		}

		// Trigger buffering at the intended start time.
		CurrentPlayer->AdaptivePlayer->SeekTo(playParam);
	}
}


void FElectraPlayer::CalculateTargetSeekTime(FTimespan& OutTargetTime, const FTimespan& InTime)
{
	if (CurrentPlayer.Get())
	{
		Electra::FTimeValue newTime;
		Electra::FTimeRange playRange;
		newTime.SetFromHNS(InTime.GetTicks());
		CurrentPlayer->AdaptivePlayer->GetSeekableRange(playRange);

		// Seek semantics are different for VoD and Live.
		// For VoD we assume the timeline to be from [0 .. duration) and not offset to what may have been an original airdate in UTC, and the seek time
		// needs to fall into that range.
		// For Live the timeline is assumed to be UTC wallclock time in [UTC-DVRwindow .. UTC) and the seek time is an offset BACKWARDS from the UTC Live edge
		// into content already aired.
		if (IsLive())
		{
			// If the target is maximum we treat it as going to the Live edge.
			if (InTime == FTimespan::MaxValue())
			{
				OutTargetTime = InTime;
				return;
			}
			// In case the seek time has been given as a negative number we negate it.
			if (newTime.GetAsHNS() < 0)
			{
				newTime = Electra::FTimeValue::GetZero() - newTime;
			}
			// We want to go that far back from the Live edge.
			newTime = playRange.End - newTime;
			// Need to clamp this to the beginning of the timeline.
			if (newTime < playRange.Start)
			{
				newTime = playRange.Start;
			}
		}
		else
		{
			// For VoD we clamp the time into the timeline only when it would fall off the beginning.
			// We purposely allow to seek outside the duration which will trigger an 'ended' event.
			// This is to make sure that a game event during which a VoD asset is played and synchronized
			// to the beginning of the event itself will not play the last n seconds for people who have
			// joined the event when it is already over.
			if (newTime < playRange.Start)
			{
				newTime = playRange.Start;
			}
			/*
			else if (newTime > playRange.End)
			{
				newTime = playRange.End;
			}
			*/
		}

		OutTargetTime = FTimespan(newTime.GetAsHNS());
	}
}


bool FElectraPlayer::Seek(const FTimespan& Time)
{
	FSeekParam NoParams;
	return Seek(Time, NoParams);
}

bool FElectraPlayer::Seek(const FTimespan& Time, const FSeekParam& Param)
{
	if (CurrentPlayer.Get())
	{
		FTimespan Target;
		CalculateTargetSeekTime(Target, Time);
		Electra::IAdaptiveStreamingPlayer::FSeekParam seek;
		if (Target != FTimespan::MaxValue())
		{
			seek.Time.SetFromTimespan(Target);
		}
		seek.StartingBitrate = Param.StartingBitrate;
		seek.bOptimizeForScrubbing = Param.bOptimizeForScrubbing;
		seek.DistanceThreshold = Param.DistanceThreshold;
		bInitialSeekPerformed = true;
		bDiscardOutputUntilCleanStart = true;
		CurrentPlayer->AdaptivePlayer->SeekTo(seek);
		return true;
	}
	return false;
}

void FElectraPlayer::SetFrameAccurateSeekMode(bool bEnableFrameAccuracy)
{
	bFrameAccurateSeeking = bEnableFrameAccuracy;
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		LockedPlayer->AdaptivePlayer->EnableFrameAccurateSeeking(bFrameAccurateSeeking.GetValue());
	}
}

void FElectraPlayer::ModifyOptions(const FParamDict& InOptionsToSetOrChange, const FParamDict& InOptionsToClear)
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		LockedPlayer->AdaptivePlayer->ModifyOptions(InOptionsToSetOrChange, InOptionsToClear);
	}
}


void FElectraPlayer::SetPlaybackRange(const FPlaybackRange& InPlaybackRange)
{
	CurrentPlaybackRange = InPlaybackRange;
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		Electra::IAdaptiveStreamingPlayer::FPlaybackRange Range;
		if (CurrentPlaybackRange.Start.IsSet())
		{
			Range.Start = Electra::FTimeValue().SetFromTimespan(CurrentPlaybackRange.Start.GetValue());
		}
		if (CurrentPlaybackRange.End.IsSet())
		{
			Range.End = Electra::FTimeValue().SetFromTimespan(CurrentPlaybackRange.End.GetValue());
		}
		LockedPlayer->AdaptivePlayer->SetPlaybackRange(Range);
	}
}

void FElectraPlayer::GetPlaybackRange(FPlaybackRange& OutPlaybackRange) const
{
	OutPlaybackRange = CurrentPlaybackRange;
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		Electra::IAdaptiveStreamingPlayer::FPlaybackRange Range;
		LockedPlayer->AdaptivePlayer->GetPlaybackRange(Range);
		if (Range.Start.IsSet())
		{
			OutPlaybackRange.Start = Range.Start.GetValue().GetAsTimespan();
		}
		else
		{
			OutPlaybackRange.Start.Reset();
		}
		if (Range.End.IsSet())
		{
			OutPlaybackRange.End = Range.End.GetValue().GetAsTimespan();
		}
		else
		{
			OutPlaybackRange.End.Reset();
		}
	}
}


TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> FElectraPlayer::GetTrackStreamMetadata(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		TArray<Electra::FTrackMetadata> TrackMetaData;
		if (TrackType == EPlayerTrackType::Video)
		{
			LockedPlayer->AdaptivePlayer->GetTrackMetadata(TrackMetaData, Electra::EStreamType::Video);
		}
		else if (TrackType == EPlayerTrackType::Audio)
		{
			LockedPlayer->AdaptivePlayer->GetTrackMetadata(TrackMetaData, Electra::EStreamType::Audio);
		}
		else if (TrackType == EPlayerTrackType::Subtitle)
		{
			LockedPlayer->AdaptivePlayer->GetTrackMetadata(TrackMetaData, Electra::EStreamType::Subtitle);
		}
		if (TrackIndex >= 0 && TrackIndex < TrackMetaData.Num())
		{
			return MakeShared<Electra::FTrackMetadata, ESPMode::ThreadSafe>(TrackMetaData[TrackIndex]);
		}
	}
	return nullptr;
}


bool FElectraPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FAudioTrackFormat& OutFormat) const
{
	if (TrackIndex >= 0 && TrackIndex < NumTracksAudio && FormatIndex == 0)
	{
		TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(EPlayerTrackType::Audio, TrackIndex);
		if (Meta.IsValid())
		{
			const Electra::FStreamCodecInformation& ci = Meta->HighestBandwidthCodec;
			OutFormat.BitsPerSample = 16;
			OutFormat.NumChannels = (uint32)ci.GetNumberOfChannels();
			OutFormat.SampleRate = (uint32)ci.GetSamplingRate();
			OutFormat.TypeName = ci.GetCodecSpecifierRFC6381();
			return true;
		}
	}
	return false;
}



bool FElectraPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FVideoTrackFormat& OutFormat) const
{
	if (TrackIndex >= 0 && TrackIndex < NumTracksVideo && FormatIndex == 0)
	{
		TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(EPlayerTrackType::Video, TrackIndex);
		if (Meta.IsValid())
		{
			const Electra::FStreamCodecInformation& ci = Meta->HighestBandwidthCodec;
			OutFormat.Dim.X = ci.GetResolution().Width;
			OutFormat.Dim.Y = ci.GetResolution().Height;
			OutFormat.FrameRate = (float)ci.GetFrameRate().GetAsDouble();
			OutFormat.FrameRates = TRange<float>{ OutFormat.FrameRate };
			OutFormat.TypeName = ci.GetCodecSpecifierRFC6381();
			return true;
		}
	}
	return false;
}


int32 FElectraPlayer::GetNumVideoStreams(int32 TrackIndex) const
{
	TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(EPlayerTrackType::Video, TrackIndex);
	return Meta.IsValid() ? Meta->StreamDetails.Num() : 0;
}

bool FElectraPlayer::GetVideoStreamFormat(FVideoStreamFormat& OutFormat, int32 InTrackIndex, int32 InStreamIndex) const
{
	TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(EPlayerTrackType::Video, InTrackIndex);
	if (Meta.IsValid() && InStreamIndex < Meta->StreamDetails.Num())
	{
		const Electra::FStreamCodecInformation& ci = Meta->StreamDetails[InStreamIndex].CodecInformation;
		OutFormat.Bitrate = Meta->StreamDetails[InStreamIndex].Bandwidth;
		OutFormat.Resolution.X = ci.GetResolution().Width;
		OutFormat.Resolution.Y = ci.GetResolution().Height;
		OutFormat.FrameRate = (float)ci.GetFrameRate().GetAsDouble();
		return true;
	}
	return false;
}


bool FElectraPlayer::GetActiveVideoStreamFormat(FVideoStreamFormat& OutFormat) const
{
	FScopeLock lock(&PlayerLock);
	if (CurrentlyActiveVideoStreamFormat.IsSet())
	{
		OutFormat = CurrentlyActiveVideoStreamFormat.GetValue();
	}
	return false;
}


int32 FElectraPlayer::GetNumTracks(EPlayerTrackType TrackType) const
{
	if (TrackType == EPlayerTrackType::Audio)
	{
		return NumTracksAudio;
	}
	else if (TrackType == EPlayerTrackType::Video)
	{
		return NumTracksVideo;
	}
	else if (TrackType == EPlayerTrackType::Subtitle)
	{
		return NumTracksSubtitle;
	}
	// TODO: Implement missing track types.

	return 0;
}

int32 FElectraPlayer::GetNumTrackFormats(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	// Right now we only have a single format per track
	if ((TrackType == EPlayerTrackType::Video && NumTracksVideo != 0) ||
		(TrackType == EPlayerTrackType::Audio && NumTracksAudio != 0) ||
		(TrackType == EPlayerTrackType::Subtitle && NumTracksSubtitle != 0))
	{
		return 1;
	}
	return 0;
}

int32 FElectraPlayer::GetSelectedTrack(EPlayerTrackType TrackType) const
{
	/*
		To reduce the overhead of this function we check for the track the underlying player has
		actually selected only when we were told the tracks changed.

		It is possible that the underlying player changes the track automatically as playback progresses.
		For instance, when playing a DASH stream consisting of several periods the player needs to re-select
		the audio stream when transitioning from one period into the next, which may change the index of
		the selected track.
	*/

	auto CheckAndReselectTrack = [this](Electra::EStreamType InStreamType, bool& InOutDirtyFlag, int32& InOutSelectedIndex, int32 InNumTracks) -> int32
	{
		if (InOutDirtyFlag)
		{
			if (InNumTracks == 0)
			{
				InOutSelectedIndex = -1;
			}
			else
			{
				TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
				if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
				{
					if (LockedPlayer->AdaptivePlayer->IsTrackDeselected(InStreamType))
					{
						InOutSelectedIndex = -1;
						InOutDirtyFlag = false;
					}
					else
					{
						Electra::FStreamSelectionAttributes Attributes;
						LockedPlayer->AdaptivePlayer->GetSelectedTrackAttributes(Attributes, InStreamType);
						if (Attributes.OverrideIndex.IsSet())
						{
							InOutSelectedIndex = Attributes.OverrideIndex.GetValue();
							InOutDirtyFlag = false;
						}
					}
				}
			}
		}
		return InOutSelectedIndex;
	};

	// Electra does not have caption or metadata tracks, handle only video, audio and subtitles.
	if (TrackType == EPlayerTrackType::Video)
	{
		return CheckAndReselectTrack(Electra::EStreamType::Video, bVideoTrackIndexDirty, SelectedVideoTrackIndex, NumTracksVideo);
	}
	else if (TrackType == EPlayerTrackType::Audio)
	{
		return CheckAndReselectTrack(Electra::EStreamType::Audio, bAudioTrackIndexDirty, SelectedAudioTrackIndex, NumTracksAudio);
	}
	else if (TrackType == EPlayerTrackType::Subtitle)
	{
		return CheckAndReselectTrack(Electra::EStreamType::Subtitle, bSubtitleTrackIndexDirty, SelectedSubtitleTrackIndex, NumTracksSubtitle);
	}
	return -1;
}

FText FElectraPlayer::GetTrackDisplayName(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(TrackType, TrackIndex);
	if (Meta.IsValid())
	{
		if (TrackType == EPlayerTrackType::Video)
		{
			if (!Meta->Label.IsEmpty())
			{
				return FText::FromString(Meta->Label);
			}
			return FText::FromString(FString::Printf(TEXT("Video Track ID %s"), *Meta->ID));
		}
		else if (TrackType == EPlayerTrackType::Audio)
		{
			if (!Meta->Label.IsEmpty())
			{
				return FText::FromString(Meta->Label);
			}
			return FText::FromString(FString::Printf(TEXT("Audio Track ID %s"), *Meta->ID));
		}
		else if (TrackType == EPlayerTrackType::Subtitle)
		{
			FString Name;
			if (!Meta->Label.IsEmpty())
			{
				Name = FString::Printf(TEXT("%s (%s)"), *Meta->Label, *Meta->HighestBandwidthCodec.GetCodecSpecifierRFC6381());
			}
			else
			{
				Name = FString::Printf(TEXT("Subtitle Track ID %s (%s)"), *Meta->ID, *Meta->HighestBandwidthCodec.GetCodecSpecifierRFC6381());
			}
			return FText::FromString(Name);
		}
	}
	return FText();
}

int32 FElectraPlayer::GetTrackFormat(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	// Right now we only have a single format per track so we return format index 0 at all times.
	return 0;
}

FString FElectraPlayer::GetTrackLanguage(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(TrackType, TrackIndex);
	if (Meta.IsValid())
	{
		return Meta->Language;
	}
	return TEXT("");
}

FString FElectraPlayer::GetTrackName(EPlayerTrackType TrackType, int32 TrackIndex) const
{
	return TEXT("");
}


/**
 * Selects a specified track for playback.
 *
 * Note:
 *   There is currently no concept of selecting a track based on metadata, only by index.
 *   The idea being that before selecting a track by index the application needs to check
 *   the metadata beforehand (eg. call GetTrackLanguage()) to figure out the index of the
 *   track it wants to play.
 *
 *   The underlying player however needs to select tracks based on metadata alone instead
 *   of an index in case the track layout changes dynamically during playback.
 *   For example, a part of the presentation could have both English and French audio,
 *   followed by a part (say, an advertisement) that only has English audio, followed
 *   by the continued regular part that has both. Without any user intervention the
 *   player needs to automatically switch from French to English and back to French, or
 *   index 1 -> 0 -> 1 (assuming French was the starting language of choice).
 *   Indices are therefore meaningless to the underlying player.
 *
 *   SelectTrack() is currently called implicitly by FMediaPlayerFacade::SelectDefaultTracks()
 *   when EMediaEvent::TracksChanged is received. This is why this event is NOT sent out
 *   in HandlePlayerEventTracksChanged() when the underlying player notifies us about a
 *   change in track layout.
 *   Other than the very first track selection made by the facade this method should only
 *   be called from a direct user interaction.
 */
bool FElectraPlayer::SelectTrack(EPlayerTrackType TrackType, int32 TrackIndex)
{
	auto PerformSelection = [this, TrackType, TrackIndex](int32& OutSelectedTrackIndex, IElectraPlayerInterface::FStreamSelectionAttributes& OutSelectionAttributes) -> bool
	{
		Electra::EStreamType StreamType = TrackType == IElectraPlayerInterface::EPlayerTrackType::Video ? Electra::EStreamType::Video :
										  TrackType == IElectraPlayerInterface::EPlayerTrackType::Audio ? Electra::EStreamType::Audio :
										  TrackType == IElectraPlayerInterface::EPlayerTrackType::Subtitle ? Electra::EStreamType::Subtitle :
										  Electra::EStreamType::Unsupported;
		// Select a track or deselect?
		if (TrackIndex >= 0)
		{
			// Check if the track index exists by checking the presence of the track metadata.
			// If for some reason the index is not valid the selection will not be changed.
			TSharedPtr<Electra::FTrackMetadata, ESPMode::ThreadSafe> Meta = GetTrackStreamMetadata(TrackType, TrackIndex);
			if (Meta.IsValid())
			{
				// Switch only when the track index has changed.
				if (GetSelectedTrack(TrackType) != TrackIndex)
				{
					Electra::FStreamSelectionAttributes TrackAttributes;
					TrackAttributes.OverrideIndex = TrackIndex;

					OutSelectionAttributes.TrackIndexOverride = TrackIndex;
					if (!Meta->Kind.IsEmpty())
					{
						TrackAttributes.Kind = Meta->Kind;
						OutSelectionAttributes.Kind = Meta->Kind;
					}
					if (!Meta->Language.IsEmpty())
					{
						TrackAttributes.Language_ISO639 = Meta->Language;
						OutSelectionAttributes.Language_ISO639 = Meta->Language;
					}
					TrackAttributes.Codec = Meta->HighestBandwidthCodec.GetCodecName();
					OutSelectionAttributes.Codec = Meta->HighestBandwidthCodec.GetCodecName();

					TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
					if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
					{
						LockedPlayer->AdaptivePlayer->SelectTrackByAttributes(StreamType, TrackAttributes);
					}

					OutSelectedTrackIndex = TrackIndex;
				}
				return true;
			}
		}
		else
		{
			// Deselect track.
			OutSelectionAttributes.TrackIndexOverride = -1;
			OutSelectedTrackIndex = -1;
			TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
			if (LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
			{
				LockedPlayer->AdaptivePlayer->DeselectTrack(StreamType);
			}
			return true;
		}
		return false;
	};

	if (TrackType == EPlayerTrackType::Video)
	{
		return PerformSelection(SelectedVideoTrackIndex, PlaystartOptions.InitialVideoTrackAttributes);
	}
	else if (TrackType == EPlayerTrackType::Audio)
	{
		return PerformSelection(SelectedAudioTrackIndex, PlaystartOptions.InitialAudioTrackAttributes);
	}
	else if (TrackType == EPlayerTrackType::Subtitle)
	{
		return PerformSelection(SelectedSubtitleTrackIndex, PlaystartOptions.InitialSubtitleTrackAttributes);
	}
	return false;
}



void FElectraPlayer::OnMediaPlayerEventReceived(TSharedPtrTS<IAdaptiveStreamingPlayerAEMSEvent> InEvent, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode InDispatchMode)
{
#if !UE_BUILD_SHIPPING
	const TCHAR* const Origins[] = { TEXT("Playlist"), TEXT("Inband"), TEXT("TimedMetadata"), TEXT("???") };
	UE_LOG(LogElectraPlayer, Verbose, TEXT("[%p][%p] %s event %s with \"%s\", \"%s\", \"%s\" PTS @ %.3f for %.3fs"), this, CurrentPlayer.Get(),
		Origins[Electra::Utils::Min((int32)InEvent->GetOrigin(), (int32)UE_ARRAY_COUNT(Origins)-1)],
		InDispatchMode==IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode::OnReceive?TEXT("received"):TEXT("started"),
		*InEvent->GetSchemeIdUri(), *InEvent->GetValue(), *InEvent->GetID(),
		InEvent->GetPresentationTime().GetAsSeconds(), InEvent->GetDuration().GetAsSeconds());
#endif

	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (PinnedAdapterDelegate.IsValid() && LockedPlayer.IsValid() && LockedPlayer->AdaptivePlayer.IsValid())
	{
		Electra::FTimeRange MediaTimeline;
		LockedPlayer->AdaptivePlayer->GetTimelineRange(MediaTimeline);

		// Create a binary media sample of our extended format and pass it up.
		TSharedPtr<FMetaDataDecoderOutput, ESPMode::ThreadSafe> Meta = MakeShared<FMetaDataDecoderOutput, ESPMode::ThreadSafe>();
		switch(InDispatchMode)
		{
			default:
			case IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode::OnReceive:
			{
				Meta->DispatchedMode = FMetaDataDecoderOutput::EDispatchedMode::OnReceive;
				break;
			}
			case IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode::OnStart:
			{
				Meta->DispatchedMode = FMetaDataDecoderOutput::EDispatchedMode::OnStart;
				break;
			}
		}
		switch(InEvent->GetOrigin())
		{
			default:
			case IAdaptiveStreamingPlayerAEMSEvent::EOrigin::TimedMetadata:
			{
				Meta->Origin = FMetaDataDecoderOutput::EOrigin::TimedMetadata;
				break;
			}
			case IAdaptiveStreamingPlayerAEMSEvent::EOrigin::EventStream:
			{
				Meta->Origin = FMetaDataDecoderOutput::EOrigin::EventStream;
				break;
			}
			case IAdaptiveStreamingPlayerAEMSEvent::EOrigin::InbandEventStream:
			{
				Meta->Origin = FMetaDataDecoderOutput::EOrigin::InbandEventStream;
				break;
			}
		}
		Meta->Data = InEvent->GetMessageData();
		Meta->SchemeIdUri = InEvent->GetSchemeIdUri();
		Meta->Value = InEvent->GetValue();
		Meta->ID = InEvent->GetID(),
		Meta->Duration = InEvent->GetDuration().GetAsTimespan();
		Meta->PresentationTime = FDecoderTimeStamp(InEvent->GetPresentationTime().GetAsTimespan(), 0);
		// Set the current timeline start as the metadata track's zero point. This is only useful if the timeline does not
		// actually change over time. The use of the base time is therefore tied to knowledge by the using code that the
		// timeline will be fixed.
		Meta->TrackBaseTime = FDecoderTimeStamp(MediaTimeline.Start.GetAsTimespan(), MediaTimeline.Start.GetSequenceIndex());
		PinnedAdapterDelegate->PresentMetadataSample(Meta);
	}
}


TSharedPtr<FElectraPlayer::FAnalyticsEvent> FElectraPlayer::CreateAnalyticsEvent(FString InEventName)
{
	// Since analytics are popped from the outside only we check if we have accumulated a lot without them having been retrieved.
	// To prevent those from growing beyond leap and bounds we limit ourselves to 100.
	while(NumQueuedAnalyticEvents > 100)
	{
		QueuedAnalyticEvents.Pop();
		FPlatformAtomics::InterlockedDecrement(&NumQueuedAnalyticEvents);
	}

	TSharedPtr<FAnalyticsEvent> Ev = MakeShared<FAnalyticsEvent>();
	Ev->EventName = MoveTemp(InEventName);
	AddCommonAnalyticsAttributes(Ev->ParamArray);
	return Ev;
}

void FElectraPlayer::AddCommonAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& InOutParamArray)
{
	InOutParamArray.Add(FAnalyticsEventAttribute(TEXT("SessionId"), AnalyticsInstanceGuid));
	InOutParamArray.Add(FAnalyticsEventAttribute(TEXT("EventNum"), AnalyticsInstanceEventCount));
	InOutParamArray.Add(FAnalyticsEventAttribute(TEXT("Utc"), static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp())));
	InOutParamArray.Add(FAnalyticsEventAttribute(TEXT("OS"), FString::Printf(TEXT("%s"), *AnalyticsOSVersion)));
	InOutParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUAdapter"), AnalyticsGPUType));
}


void FElectraPlayer::EnqueueAnalyticsEvent(TSharedPtr<FAnalyticsEvent> InAnalyticEvent)
{
	QueuedAnalyticEvents.Enqueue(InAnalyticEvent);
	FPlatformAtomics::InterlockedIncrement(&NumQueuedAnalyticEvents);
}

//-----------------------------------------------------------------------------
/**
 *	State management information from media player...
 */
void FElectraPlayer::HandleDeferredPlayerEvents()
{
	TSharedPtrTS<FPlayerMetricEventBase> Event;
	while(DeferredPlayerEvents.Dequeue(Event))
	{
		switch(Event->Type)
		{
			case FPlayerMetricEventBase::EType::OpenSource:
			{
				FPlayerMetricEvent_OpenSource* Ev = static_cast<FPlayerMetricEvent_OpenSource*>(Event.Get());
				HandlePlayerEventOpenSource(Ev->URL);
				break;
			}
			case FPlayerMetricEventBase::EType::ReceivedMasterPlaylist:
			{
				FPlayerMetricEvent_ReceivedMasterPlaylist* Ev = static_cast<FPlayerMetricEvent_ReceivedMasterPlaylist*>(Event.Get());
				HandlePlayerEventReceivedMasterPlaylist(Ev->EffectiveURL);
				break;
			}
			case FPlayerMetricEventBase::EType::ReceivedPlaylists:
			{
				HandlePlayerEventReceivedPlaylists();
				break;
			}
			case FPlayerMetricEventBase::EType::TracksChanged:
			{
				HandlePlayerEventTracksChanged();
				break;
			}
			case FPlayerMetricEventBase::EType::PlaylistDownload:
			{
				FPlayerMetricEvent_PlaylistDownload* Ev = static_cast<FPlayerMetricEvent_PlaylistDownload*>(Event.Get());
				HandlePlayerEventPlaylistDownload(Ev->PlaylistDownloadStats);
				break;
			}
			case FPlayerMetricEventBase::EType::CleanStart:
			{
				bDiscardOutputUntilCleanStart = false;
				break;
			}
			case FPlayerMetricEventBase::EType::BufferingStart:
			{
				FPlayerMetricEvent_BufferingStart* Ev = static_cast<FPlayerMetricEvent_BufferingStart*>(Event.Get());
				HandlePlayerEventBufferingStart(Ev->BufferingReason);
				break;
			}
			case FPlayerMetricEventBase::EType::BufferingEnd:
			{
				FPlayerMetricEvent_BufferingEnd* Ev = static_cast<FPlayerMetricEvent_BufferingEnd*>(Event.Get());
				HandlePlayerEventBufferingEnd(Ev->BufferingReason);
				break;
			}
			case FPlayerMetricEventBase::EType::Bandwidth:
			{
				FPlayerMetricEvent_Bandwidth* Ev = static_cast<FPlayerMetricEvent_Bandwidth*>(Event.Get());
				HandlePlayerEventBandwidth(Ev->EffectiveBps, Ev->ThroughputBps, Ev->LatencyInSeconds);
				break;
			}
			case FPlayerMetricEventBase::EType::BufferUtilization:
			{
				FPlayerMetricEvent_BufferUtilization* Ev = static_cast<FPlayerMetricEvent_BufferUtilization*>(Event.Get());
				HandlePlayerEventBufferUtilization(Ev->BufferStats);
				break;
			}
			case FPlayerMetricEventBase::EType::SegmentDownload:
			{
				FPlayerMetricEvent_SegmentDownload* Ev = static_cast<FPlayerMetricEvent_SegmentDownload*>(Event.Get());
				HandlePlayerEventSegmentDownload(Ev->SegmentDownloadStats);
				break;
			}
			case FPlayerMetricEventBase::EType::LicenseKey:
			{
				FPlayerMetricEvent_LicenseKey* Ev = static_cast<FPlayerMetricEvent_LicenseKey*>(Event.Get());
				HandlePlayerEventLicenseKey(Ev->LicenseKeyStats);
				break;
			}
			case FPlayerMetricEventBase::EType::DataAvailabilityChange:
			{
				FPlayerMetricEvent_DataAvailabilityChange* Ev = static_cast<FPlayerMetricEvent_DataAvailabilityChange*>(Event.Get());
				HandlePlayerEventDataAvailabilityChange(Ev->DataAvailability);
				break;
			}
			case FPlayerMetricEventBase::EType::VideoQualityChange:
			{
				FPlayerMetricEvent_VideoQualityChange* Ev = static_cast<FPlayerMetricEvent_VideoQualityChange*>(Event.Get());
				HandlePlayerEventVideoQualityChange(Ev->NewBitrate, Ev->PreviousBitrate, Ev->bIsDrasticDownswitch);
				break;
			}
			case FPlayerMetricEventBase::EType::CodecFormatChange:
			{
				FPlayerMetricEvent_CodecFormatChange* Ev = static_cast<FPlayerMetricEvent_CodecFormatChange*>(Event.Get());
				HandlePlayerEventCodecFormatChange(Ev->NewDecodingFormat);
				break;
			}
			case FPlayerMetricEventBase::EType::PrerollStart:
			{
				HandlePlayerEventPrerollStart();
				break;
			}
			case FPlayerMetricEventBase::EType::PrerollEnd:
			{
				HandlePlayerEventPrerollEnd();
				break;
			}
			case FPlayerMetricEventBase::EType::PlaybackStart:
			{
				HandlePlayerEventPlaybackStart();
				break;
			}
			case FPlayerMetricEventBase::EType::PlaybackPaused:
			{
				HandlePlayerEventPlaybackPaused();
				break;
			}
			case FPlayerMetricEventBase::EType::PlaybackResumed:
			{
				HandlePlayerEventPlaybackResumed();
				break;
			}
			case FPlayerMetricEventBase::EType::PlaybackEnded:
			{
				HandlePlayerEventPlaybackEnded();
				break;
			}
			case FPlayerMetricEventBase::EType::JumpInPlayPosition:
			{
				FPlayerMetricEvent_JumpInPlayPosition* Ev = static_cast<FPlayerMetricEvent_JumpInPlayPosition*>(Event.Get());
				HandlePlayerEventJumpInPlayPosition(Ev->ToNewTime, Ev->FromTime, Ev->TimejumpReason);
				break;
			}
			case FPlayerMetricEventBase::EType::PlaybackStopped:
			{
				HandlePlayerEventPlaybackStopped();
				break;
			}
			case FPlayerMetricEventBase::EType::SeekCompleted:
			{
				HandlePlayerEventSeekCompleted();
				break;
			}
			case FPlayerMetricEventBase::EType::Error:
			{
				FPlayerMetricEvent_Error* Ev = static_cast<FPlayerMetricEvent_Error*>(Event.Get());
				HandlePlayerEventError(Ev->ErrorReason);
				break;
			}
			case FPlayerMetricEventBase::EType::LogMessage:
			{
				FPlayerMetricEvent_LogMessage* Ev = static_cast<FPlayerMetricEvent_LogMessage*>(Event.Get());
				HandlePlayerEventLogMessage(Ev->LogLevel, Ev->LogMessage, Ev->PlayerWallclockMilliseconds);
				break;
			}
			case FPlayerMetricEventBase::EType::DroppedVideoFrame:
			{
				HandlePlayerEventDroppedVideoFrame();
				break;
			}
			case FPlayerMetricEventBase::EType::DroppedAudioFrame:
			{
				HandlePlayerEventDroppedAudioFrame();
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

void FElectraPlayer::HandlePlayerEventOpenSource(const FString& URL)
{
	PlayerState.Status = PlayerState.Status | EPlayerStatus::Connecting;

	PlayerState.State = EPlayerState::Preparing;
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaConnecting);

	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Opening stream at \"%s\""), this, CurrentPlayer.Get(), *SanitizeMessage(URL));

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	Statistics.AddMessageToHistory(TEXT("Opening stream"));
	Statistics.InitialURL = URL;
	Statistics.TimeAtOpen = FPlatformTime::Seconds();
	Statistics.LastState  = "Opening";

	// Enqueue an "OpenSource" event.
	static const FString kEventNameElectraOpenSource(TEXT("Electra.OpenSource"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraOpenSource))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraOpenSource);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), *URL));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventReceivedMasterPlaylist(const FString& EffectiveURL)
{
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Received master playlist from \"%s\""), this, CurrentPlayer.Get(), *SanitizeMessage(EffectiveURL));

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	Statistics.AddMessageToHistory(TEXT("Got master playlist"));
	// Note the time it took to get the master playlist
	Statistics.TimeToLoadMasterPlaylist = FPlatformTime::Seconds() - Statistics.TimeAtOpen;
	Statistics.LastState = "Preparing";


	// Enqueue a "MasterPlaylist" event.
	static const FString kEventNameElectraMasterPlaylist(TEXT("Electra.MasterPlaylist"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraMasterPlaylist))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraMasterPlaylist);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), *EffectiveURL));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventReceivedPlaylists()
{
	PlayerState.Status = PlayerState.Status & ~EPlayerStatus::Connecting;

	//PlayerState.Status = PlayerState.Status | EPlayerStatus::Buffering;
	//DeferredEvents.Enqueue(EPlayerEvent::MediaBuffering);

	// Player starts in paused mode. We need a SetRate() to start playback...

	MediaStateOnPreparingFinished();

	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Received initial stream playlists"), this, CurrentPlayer.Get());

	Electra::FTimeRange MediaTimeline;
	Electra::FTimeValue MediaDuration;
	CurrentPlayer->AdaptivePlayer->GetTimelineRange(MediaTimeline);
	MediaDuration = CurrentPlayer->AdaptivePlayer->GetDuration();

	// Update statistics
	StatisticsLock.Lock();
	Statistics.AddMessageToHistory(TEXT("Got initial playlists"));
	// Note the time it took to get the stream playlist
	Statistics.TimeToLoadStreamPlaylists = FPlatformTime::Seconds() - Statistics.TimeAtOpen;
	Statistics.LastState = "Idle";
	// Establish the timeline and duration.
	Statistics.MediaTimelineAtStart = MediaTimeline;
	Statistics.MediaTimelineAtEnd = MediaTimeline;
	Statistics.MediaDuration = MediaDuration.IsInfinity() ? -1.0 : MediaDuration.GetAsSeconds();
	// Get the video bitrates and populate our number of segments per bitrate map.
	TArray<Electra::FTrackMetadata> VideoStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(VideoStreamMetaData, Electra::EStreamType::Video);
	NumTracksVideo = VideoStreamMetaData.Num();
	if (NumTracksVideo)
	{
		for(int32 i=0; i<VideoStreamMetaData[0].StreamDetails.Num(); ++i)
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Found %d * %d video stream at bitrate %d"), this, CurrentPlayer.Get(),
												VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Width,
												VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Height,
												VideoStreamMetaData[0].StreamDetails[i].Bandwidth);
		}
	}
	StatisticsLock.Unlock();

	SelectedVideoTrackIndex = NumTracksVideo ? 0 : -1;

	// Enqueue a "PlaylistsLoaded" event.
	static const FString kEventNameElectraPlaylistLoaded(TEXT("Electra.PlaylistsLoaded"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPlaylistLoaded))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPlaylistLoaded);
		EnqueueAnalyticsEvent(AnalyticEvent);
	}

	TArray<Electra::FTrackMetadata> AudioStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(AudioStreamMetaData, Electra::EStreamType::Audio);
	NumTracksAudio = AudioStreamMetaData.Num();

	TArray<Electra::FTrackMetadata> SubtitleStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(SubtitleStreamMetaData, Electra::EStreamType::Subtitle);
	NumTracksSubtitle = SubtitleStreamMetaData.Num();

	// Set the initial audio track selection attributes.
	Electra::FStreamSelectionAttributes InitialAudioAttributes;
	InitialAudioAttributes.Kind = PlaystartOptions.InitialAudioTrackAttributes.Kind;
	InitialAudioAttributes.Language_ISO639 = PlaystartOptions.InitialAudioTrackAttributes.Language_ISO639;
	InitialAudioAttributes.OverrideIndex = PlaystartOptions.InitialAudioTrackAttributes.TrackIndexOverride;
	CurrentPlayer->AdaptivePlayer->SetInitialStreamAttributes(Electra::EStreamType::Audio, InitialAudioAttributes);

	// Set the initial subtitle track selection attributes.
	Electra::FStreamSelectionAttributes InitialSubtitleAttributes;
	InitialSubtitleAttributes.Kind = PlaystartOptions.InitialSubtitleTrackAttributes.Kind;
	InitialSubtitleAttributes.Language_ISO639 = PlaystartOptions.InitialSubtitleTrackAttributes.Language_ISO639;
	InitialSubtitleAttributes.OverrideIndex = PlaystartOptions.InitialSubtitleTrackAttributes.TrackIndexOverride;
	CurrentPlayer->AdaptivePlayer->SetInitialStreamAttributes(Electra::EStreamType::Subtitle, InitialSubtitleAttributes);

	// Trigger preloading unless forbidden.
	if (PlaystartOptions.bDoNotPreload == false)
	{
		TriggerFirstSeekIfNecessary();
	}
}

void FElectraPlayer::HandlePlayerEventTracksChanged()
{
	TArray<Electra::FTrackMetadata> VideoStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(VideoStreamMetaData, Electra::EStreamType::Video);
	NumTracksVideo = VideoStreamMetaData.Num();
	if (NumTracksVideo)
	{
		for(int32 i=0; i<VideoStreamMetaData[0].StreamDetails.Num(); ++i)
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Found %d * %d video stream at bitrate %d"), this, CurrentPlayer.Get(),
												VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Width,
												VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Height,
												VideoStreamMetaData[0].StreamDetails[i].Bandwidth);
		}
	}

	TArray<Electra::FTrackMetadata> AudioStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(AudioStreamMetaData, Electra::EStreamType::Audio);
	NumTracksAudio = AudioStreamMetaData.Num();

	TArray<Electra::FTrackMetadata> SubtitleStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(SubtitleStreamMetaData, Electra::EStreamType::Subtitle);
	NumTracksSubtitle = SubtitleStreamMetaData.Num();

	bVideoTrackIndexDirty = true;
	bAudioTrackIndexDirty = true;
	bSubtitleTrackIndexDirty = true;

	/*
	Note: Do not send TracksChanged here since this would trigger a re-selecting of the default tracks.
		DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::TracksChanged);
	*/
}


void FElectraPlayer::HandlePlayerEventPlaylistDownload(const Electra::Metrics::FPlaylistDownloadStats& PlaylistDownloadStats)
{
	// To reduce the number of playlist events during a Live presentation we will only report the initial playlist load
	// and later on only failed loads but not successful ones.
	bool bReport =  PlaylistDownloadStats.LoadType == Electra::Playlist::ELoadType::Initial || !PlaylistDownloadStats.bWasSuccessful;
	if (bReport)
	{
		static const FString kEventNameElectraPlaylistDownload(TEXT("Electra.PlaylistDownload"));
		if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPlaylistDownload))
		{
			// Enqueue a "PlaylistDownload" event.
			TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPlaylistDownload);
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), *PlaylistDownloadStats.URL));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Failure"), *PlaylistDownloadStats.FailureReason));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("ListType"), Electra::Playlist::GetPlaylistTypeString(PlaylistDownloadStats.ListType)));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("LoadType"), Electra::Playlist::GetPlaylistLoadTypeString(PlaylistDownloadStats.LoadType)));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("HTTPStatus"), PlaylistDownloadStats.HTTPStatusCode));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Retry"), PlaylistDownloadStats.RetryNumber));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bSuccess"), PlaylistDownloadStats.bWasSuccessful));
			EnqueueAnalyticsEvent(AnalyticEvent);
		}
	}
}

void FElectraPlayer::HandlePlayerEventLicenseKey(const Electra::Metrics::FLicenseKeyStats& LicenseKeyStats)
{
	// TBD
	if (LicenseKeyStats.bWasSuccessful)
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] License key obtained"), this, CurrentPlayer.Get());
		FScopeLock Lock(&StatisticsLock);
		Statistics.AddMessageToHistory(TEXT("Obtained license key"));
	}
	else
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] License key error \"%s\""), this, CurrentPlayer.Get(), *LicenseKeyStats.FailureReason);
		FScopeLock Lock(&StatisticsLock);
		Statistics.AddMessageToHistory(TEXT("License key error"));
	}
}


void FElectraPlayer::HandlePlayerEventDataAvailabilityChange(const Electra::Metrics::FDataAvailabilityChange& DataAvailability)
{
	// Pass this event up to the media player facade. We do not act on this here right now.
	if (DataAvailability.StreamType == Electra::EStreamType::Video)
	{
		if (DataAvailability.Availability == Electra::Metrics::FDataAvailabilityChange::EAvailability::DataAvailable)
		{
			DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_VideoSamplesAvailable);
		}
		else if (DataAvailability.Availability == Electra::Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable)
		{
			DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_VideoSamplesUnavailable);
		}
	}
	else if (DataAvailability.StreamType == Electra::EStreamType::Audio)
	{
		if (DataAvailability.Availability == Electra::Metrics::FDataAvailabilityChange::EAvailability::DataAvailable)
		{
			DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_AudioSamplesAvailable);
		}
		else if (DataAvailability.Availability == Electra::Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable)
		{
			DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_AudioSamplesUnavailable);
		}
	}
}


void FElectraPlayer::HandlePlayerEventBufferingStart(Electra::Metrics::EBufferingReason BufferingReason)
{
	PlayerState.Status = PlayerState.Status | EPlayerStatus::Buffering;
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaBuffering);

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	Statistics.TimeAtBufferingBegin = FPlatformTime::Seconds();
	switch(BufferingReason)
	{
		case Electra::Metrics::EBufferingReason::Initial:
			Statistics.bIsInitiallyDownloading = true;
			Statistics.LastState = "Buffering";
			break;
		case Electra::Metrics::EBufferingReason::Seeking:
			Statistics.LastState = "Seeking";
			break;
		case Electra::Metrics::EBufferingReason::Rebuffering:
			++Statistics.NumTimesRebuffered;
			Statistics.LastState = "Rebuffering";
			break;
	}
	// Enqueue a "BufferingStart" event.
	static const FString kEventNameElectraBufferingStart(TEXT("Electra.BufferingStart"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraBufferingStart))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraBufferingStart);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Type"), Electra::Metrics::GetBufferingReasonString(BufferingReason)));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}

	FString Msg = FString::Printf(TEXT("%s buffering starts"), Electra::Metrics::GetBufferingReasonString(BufferingReason));
	Statistics.AddMessageToHistory(Msg);

	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] %s"), this, CurrentPlayer.Get(), *Msg);
	CSV_EVENT(ElectraPlayer, TEXT("Buffering starts"));
}

void FElectraPlayer::HandlePlayerEventBufferingEnd(Electra::Metrics::EBufferingReason BufferingReason)
{
	// Note: While this event signals the end of buffering the player will now immediately transition into the pre-rolling
	//       state from which a playback start is not quite possible yet and would incur a slight delay until it is.
	//       To avoid this we keep the state as buffering until the pre-rolling phase has also completed.
	//PlayerState.Status = PlayerState.Status & ~EPlayerStatus::Buffering;

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	double BufferingDuration = FPlatformTime::Seconds() - Statistics.TimeAtBufferingBegin;
	switch(BufferingReason)
	{
		case Electra::Metrics::EBufferingReason::Initial:
			Statistics.InitialBufferingDuration = BufferingDuration;
			break;
		case Electra::Metrics::EBufferingReason::Seeking:
			// End of seek buffering is not relevant here.
			break;
		case Electra::Metrics::EBufferingReason::Rebuffering:
			if (BufferingDuration > Statistics.LongestRebufferingDuration)
			{
				Statistics.LongestRebufferingDuration = BufferingDuration;
			}
			Statistics.TotalRebufferingDuration += BufferingDuration;
			break;
	}

	// Enqueue a "BufferingEnd" event.
	static const FString kEventNameElectraBufferingEnd(TEXT("Electra.BufferingEnd"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraBufferingEnd))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraBufferingEnd);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Type"), Electra::Metrics::GetBufferingReasonString(BufferingReason)));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] %s buffering ended after %.3fs"), this, CurrentPlayer.Get(), Electra::Metrics::GetBufferingReasonString(BufferingReason), BufferingDuration);
	Statistics.AddMessageToHistory(TEXT("Buffering ended"));
// Should we set the state (back?) to something or wait for the following play/pause event to set a new one?
	Statistics.LastState = "Ready";

	CSV_EVENT(ElectraPlayer, TEXT("Buffering ends"));
}

void FElectraPlayer::HandlePlayerEventBandwidth(int64 EffectiveBps, int64 ThroughputBps, double LatencyInSeconds)
{
//	FScopeLock Lock(&StatisticsLock);
	UE_LOG(LogElectraPlayer, VeryVerbose, TEXT("[%p][%p] Observed bandwidth of %lld Kbps; throughput = %lld Kbps; latency = %.3fs"), this, CurrentPlayer.Get(), EffectiveBps/1000, ThroughputBps/1000, LatencyInSeconds);
}

void FElectraPlayer::HandlePlayerEventBufferUtilization(const Electra::Metrics::FBufferStats& BufferStats)
{
//	FScopeLock Lock(&StatisticsLock);
}

void FElectraPlayer::HandlePlayerEventSegmentDownload(const Electra::Metrics::FSegmentDownloadStats& SegmentDownloadStats)
{
	// Cached responses are not actual network traffic, so we ignore them.
	if (SegmentDownloadStats.bIsCachedResponse)
	{
		return;
	}
	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	if (SegmentDownloadStats.StreamType == Electra::EStreamType::Video)
	{
		Statistics.NumVideoDatabytesStreamed += SegmentDownloadStats.NumBytesDownloaded;
		if (!Statistics.VideoSegmentBitratesStreamed.Contains(SegmentDownloadStats.Bitrate))
		{
			Statistics.VideoSegmentBitratesStreamed.Add(SegmentDownloadStats.Bitrate, 0);
		}
		++Statistics.VideoSegmentBitratesStreamed[SegmentDownloadStats.Bitrate];

		if (Statistics.bIsInitiallyDownloading)
		{
			Statistics.InitialBufferingBandwidth.AddSample(8*SegmentDownloadStats.NumBytesDownloaded / (SegmentDownloadStats.TimeToDownload > 0.0 ? SegmentDownloadStats.TimeToDownload : 1.0), SegmentDownloadStats.TimeToFirstByte);
			if (Statistics.InitialBufferingDuration > 0.0)
			{
				Statistics.bIsInitiallyDownloading = false;
			}
		}
	}
	else if (SegmentDownloadStats.StreamType == Electra::EStreamType::Audio)
	{
		Statistics.NumAudioDatabytesStreamed += SegmentDownloadStats.NumBytesDownloaded;
	}
	if (SegmentDownloadStats.bWasSuccessful)
	{
		UE_LOG(LogElectraPlayer, VeryVerbose, TEXT("[%p][%p] Downloaded %s segment at bitrate %d: Playback time = %.3fs, duration = %.3fs, download time = %.3fs, URL=%s \"%s\""), this, CurrentPlayer.Get(), Electra::GetStreamTypeName(SegmentDownloadStats.StreamType), SegmentDownloadStats.Bitrate, SegmentDownloadStats.PresentationTime, SegmentDownloadStats.Duration, SegmentDownloadStats.TimeToDownload, *SegmentDownloadStats.Range, *SanitizeMessage(SegmentDownloadStats.URL));
	}
	else if (SegmentDownloadStats.bWasAborted)
	{
		++Statistics.NumSegmentDownloadsAborted;
	}
	if (!SegmentDownloadStats.bWasSuccessful || SegmentDownloadStats.RetryNumber)
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] %s segment download issue (%s): retry:%d, success:%d, aborted:%d, filler:%d"), this, CurrentPlayer.Get(), Electra::Metrics::GetSegmentTypeString(SegmentDownloadStats.SegmentType), *SegmentDownloadStats.FailureReason, SegmentDownloadStats.RetryNumber, SegmentDownloadStats.bWasSuccessful, SegmentDownloadStats.bWasAborted, SegmentDownloadStats.bInsertedFillerData);

		if (SegmentDownloadStats.FailureReason.Len())
		{
			Statistics.AddMessageToHistory(FString::Printf(TEXT("%s segment download issue (%s)"), Electra::Metrics::GetSegmentTypeString(SegmentDownloadStats.SegmentType), *SegmentDownloadStats.FailureReason));
		}

		static const FString kEventNameElectraSegmentIssue(TEXT("Electra.SegmentIssue"));
		if (Electra::IsAnalyticsEventEnabled(kEventNameElectraSegmentIssue))
		{
			// Enqueue a "SegmentIssue" event.
			TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraSegmentIssue);
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), *SegmentDownloadStats.URL));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Failure"), *SegmentDownloadStats.FailureReason));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("SegmentType"), Electra::Metrics::GetSegmentTypeString(SegmentDownloadStats.SegmentType)));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("HTTPStatus"), SegmentDownloadStats.HTTPStatusCode));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Retry"), SegmentDownloadStats.RetryNumber));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bSuccess"), SegmentDownloadStats.bWasSuccessful));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeToFirstByte"), SegmentDownloadStats.TimeToFirstByte));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("ByteSize"), SegmentDownloadStats.ByteSize));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumBytesDownloaded"), SegmentDownloadStats.NumBytesDownloaded));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bWasAborted"), SegmentDownloadStats.bWasAborted));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bDidTimeout"), SegmentDownloadStats.bDidTimeout));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bParseFailure"), SegmentDownloadStats.bParseFailure));
			AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bInsertedFillerData"), SegmentDownloadStats.bInsertedFillerData));
			EnqueueAnalyticsEvent(AnalyticEvent);
		}
	}
}

void FElectraPlayer::HandlePlayerEventVideoQualityChange(int32 NewBitrate, int32 PreviousBitrate, bool bIsDrasticDownswitch)
{
	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	if (PreviousBitrate == 0)
	{
		Statistics.InitialStreamBitrate = NewBitrate;
	}
	else
	{
		if(bIsDrasticDownswitch)
		{
			++Statistics.NumQualityDrasticDownswitches;
		}
		if(NewBitrate > PreviousBitrate)
		{
			++Statistics.NumQualityUpswitches;
		}
		else
		{
			++Statistics.NumQualityDownswitches;
		}
	}
	if (bIsDrasticDownswitch)
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Player switched video quality drastically down to %d bps from %d bps. %d upswitches, %d downswitches (%d drastic ones)"), this, CurrentPlayer.Get(), NewBitrate, PreviousBitrate, Statistics.NumQualityUpswitches, Statistics.NumQualityDownswitches, Statistics.NumQualityDrasticDownswitches);
	}
	else
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Player switched video quality to %d bps from %d bps. %d upswitches, %d downswitches (%d drastic ones)"), this, CurrentPlayer.Get(), NewBitrate, PreviousBitrate, Statistics.NumQualityUpswitches, Statistics.NumQualityDownswitches, Statistics.NumQualityDrasticDownswitches);
	}

	int32 prvWidth  = Statistics.CurrentlyActiveResolutionWidth;
	int32 prvHeight = Statistics.CurrentlyActiveResolutionHeight;
	// Get the current playlist URL
	TArray<Electra::FTrackMetadata> VideoStreamMetaData;
	CurrentPlayer->AdaptivePlayer->GetTrackMetadata(VideoStreamMetaData, Electra::EStreamType::Video);
	if (VideoStreamMetaData.Num())
	{
		for(int32 i=0; i<VideoStreamMetaData[0].StreamDetails.Num(); ++i)
		{
			if (VideoStreamMetaData[0].StreamDetails[i].Bandwidth == NewBitrate)
			{
				SelectedQuality = i;
				Statistics.CurrentlyActivePlaylistURL = VideoStreamMetaData[0].ID;
				Statistics.CurrentlyActiveResolutionWidth = VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Width;
				Statistics.CurrentlyActiveResolutionHeight = VideoStreamMetaData[0].StreamDetails[i].CodecInformation.GetResolution().Height;
				break;
			}
		}
	}

	// Enqueue a "VideoQualityChange" event.
	static const FString kEventNameElectraVideoQualityChange(TEXT("Electra.VideoQualityChange"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraVideoQualityChange))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraVideoQualityChange);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("OldBitrate"), PreviousBitrate));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("NewBitrate"), NewBitrate));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("bIsDrasticDownswitch"), bIsDrasticDownswitch));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("OldResolution"), *FString::Printf(TEXT("%d*%d"), prvWidth, prvHeight)));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("NewResolution"), *FString::Printf(TEXT("%d*%d"), Statistics.CurrentlyActiveResolutionWidth, Statistics.CurrentlyActiveResolutionHeight)));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}

	CSV_EVENT(ElectraPlayer, TEXT("QualityChange %d -> %d"), PreviousBitrate, NewBitrate);
}

void FElectraPlayer::HandlePlayerEventCodecFormatChange(const Electra::FStreamCodecInformation& NewDecodingFormat)
{
	if (NewDecodingFormat.IsVideoCodec())
	{
		FVideoStreamFormat fmt;
		fmt.Bitrate = NewDecodingFormat.GetBitrate();
		fmt.Resolution.X = NewDecodingFormat.GetResolution().Width;
		fmt.Resolution.Y = NewDecodingFormat.GetResolution().Height;
		fmt.FrameRate = NewDecodingFormat.GetFrameRate().IsValid() ? NewDecodingFormat.GetFrameRate().GetAsDouble() : 0.0;
		{
		FScopeLock lock(&PlayerLock);
		CurrentlyActiveVideoStreamFormat = fmt;
		}
	}
}


void FElectraPlayer::HandlePlayerEventPrerollStart()
{
	bDiscardOutputUntilCleanStart = false;
	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	Statistics.TimeAtPrerollBegin = FPlatformTime::Seconds();
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Player starts prerolling to warm decoders and renderers"), this, CurrentPlayer.Get());
//	Statistics.LastState = "Ready";
//	FString					CurrentState;	// "Empty", "Opening", "Preparing", "Buffering", "Idle", "Ready", "Playing", "Paused", "Seeking", "Rebuffering", "Ended", "Closed"

	// Enqueue a "PrerollStart" event.
	static const FString kEventNameElectraPrerollStart(TEXT("Electra.PrerollStart"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPrerollStart))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPrerollStart);
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPrerollEnd()
{
	// Note: See comments in ReportBufferingEnd()
	//       Preroll follows at the end of buffering and we keep the buffering state until preroll has finished as well.
	PlayerState.Status = PlayerState.Status & ~EPlayerStatus::Buffering;

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	if (Statistics.TimeForInitialPreroll < 0.0)
	{
		Statistics.TimeForInitialPreroll = FPlatformTime::Seconds() - Statistics.TimeAtPrerollBegin;
	}
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Player prerolling complete"), this, CurrentPlayer.Get());
	Statistics.LastState = "Ready";

	// Enqueue a "PrerollEnd" event.
	static const FString kEventNameElectraPrerollEnd(TEXT("Electra.PrerollEnd"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPrerollEnd))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPrerollEnd);
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPlaybackStart()
{
	PlayerState.Status = PlayerState.Status & ~EPlayerStatus::Buffering;
	MediaStateOnPlay();

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	double PlayPos = CurrentPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();
	if (Statistics.PlayPosAtStart < 0.0)
	{
		Statistics.PlayPosAtStart = PlayPos;
	}
	Statistics.LastState = "Playing";
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Playback started at play position %.3f"), this, CurrentPlayer.Get(), PlayPos);
	Statistics.AddMessageToHistory(TEXT("Playback started"));

	// Enqueue a "Start" event.
	static const FString kEventNameElectraStart(TEXT("Electra.Start"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraStart))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraStart);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPos"), PlayPos));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPlaybackPaused()
{
	MediaStateOnPause();
	FScopeLock Lock(&StatisticsLock);
	Statistics.LastState = "Paused";
	double PlayPos = CurrentPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Playback paused at play position %.3f"), this, CurrentPlayer.Get(), PlayPos);
	Statistics.AddMessageToHistory(TEXT("Playback paused"));

	// Enqueue a "Pause" event.
	static const FString kEventNameElectraPause(TEXT("Electra.Pause"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPause))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPause);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPos"), PlayPos));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPlaybackResumed()
{
	MediaStateOnPlay();
	FScopeLock Lock(&StatisticsLock);
	Statistics.LastState = "Playing";
	double PlayPos = CurrentPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Playback resumed at play position %.3f"), this, CurrentPlayer.Get(), PlayPos);
	Statistics.AddMessageToHistory(TEXT("Playback resumed"));

	// Enqueue a "Resume" event.
	static const FString kEventNameElectraResume(TEXT("Electra.Resume"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraResume))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraResume);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPos"), PlayPos));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPlaybackEnded()
{
	UpdatePlayEndStatistics();
	double PlayPos = CurrentPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();

	// Update statistics
	{
	FScopeLock Lock(&StatisticsLock);
	Statistics.LastState = "Ended";
	Statistics.bDidPlaybackEnd = true;
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Playback reached end at play position %.3f"), this, CurrentPlayer.Get(), PlayPos);
	Statistics.AddMessageToHistory(TEXT("Playback ended"));

	// Enqueue an "End" event.
	static const FString kEventNameElectraEnd(TEXT("Electra.End"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraEnd))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraEnd);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPos"), PlayPos));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
	}

	MediaStateOnEndReached();
}

void FElectraPlayer::HandlePlayerEventJumpInPlayPosition(const Electra::FTimeValue& ToNewTime, const Electra::FTimeValue& FromTime, Electra::Metrics::ETimeJumpReason TimejumpReason)
{
	Electra::FTimeRange MediaTimeline;
	CurrentPlayer->AdaptivePlayer->GetTimelineRange(MediaTimeline);

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	if (TimejumpReason == Electra::Metrics::ETimeJumpReason::UserSeek)
	{
		if (ToNewTime > FromTime)
		{
			++Statistics.NumTimesForwarded;
		}
		else if (ToNewTime < FromTime)
		{
			++Statistics.NumTimesRewound;
		}
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Jump in play position from %.3f to %.3f"), this, CurrentPlayer.Get(), FromTime.GetAsSeconds(), ToNewTime.GetAsSeconds());
	}
	else if (TimejumpReason == Electra::Metrics::ETimeJumpReason::Looping)
	{
		++Statistics.NumTimesLooped;
		Electra::IAdaptiveStreamingPlayer::FLoopState loopState;
		CurrentPlayer->AdaptivePlayer->GetLoopState(loopState);
		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Looping (%d) from %.3f to %.3f"), this, CurrentPlayer.Get(), loopState.Count, FromTime.GetAsSeconds(), ToNewTime.GetAsSeconds());
		Statistics.AddMessageToHistory(TEXT("Looped"));
	}

	// Enqueue a "PositionJump" event.
	static const FString kEventNameElectraPositionJump(TEXT("Electra.PositionJump"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraPositionJump))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraPositionJump);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("From"), FromTime.GetAsSeconds()));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("To"), ToNewTime.GetAsSeconds()));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Cause"), Electra::Metrics::GetTimejumpReasonString(TimejumpReason)));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.Start"), MediaTimeline.Start.GetAsSeconds(-1.0)));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.End"), MediaTimeline.End.GetAsSeconds(-1.0)));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventPlaybackStopped()
{
	UpdatePlayEndStatistics();
	double PlayPos = CurrentPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	Statistics.bDidPlaybackEnd = true;
	// Note: we do not change Statistics.LastState since we want to keep the state the player was in when it got closed.
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Playback stopped. Last play position %.3f"), this, CurrentPlayer.Get(), PlayPos);
	Statistics.AddMessageToHistory(TEXT("Stopped"));

	// Enqueue a "Stop" event.
	static const FString kEventNameElectraStop(TEXT("Electra.Stop"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraStop))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraStop);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPos"), PlayPos));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventSeekCompleted()
{
	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Seek completed"), this, CurrentPlayer.Get());
	bDiscardOutputUntilCleanStart = false;
	MediaStateOnSeekFinished();
}

void FElectraPlayer::HandlePlayerEventError(const FString& ErrorReason)
{
	bHasPendingError = true;

	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	// If there is already an error do not overwrite it. First come, first serve!
	if (Statistics.LastError.Len() == 0)
	{
		Statistics.LastError = ErrorReason;
	}
	// Note: we do not change Statistics.LastState to something like 'error' because we want to know the state the player was in when it errored.
	UE_LOG(LogElectraPlayer, Error, TEXT("[%p][%p] ReportError: \"%s\""), this, CurrentPlayer.Get(), *SanitizeMessage(ErrorReason));
	Statistics.AddMessageToHistory(FString::Printf(TEXT("Error: %s"), *SanitizeMessage(ErrorReason)));
	FString MessageHistory;
	for(auto &msg : Statistics.MessageHistoryBuffer)
	{
		MessageHistory.Append(msg);
		MessageHistory.Append(TEXT("\n"));
	}

	// Enqueue an "Error" event.
	static const FString kEventNameElectraError(TEXT("Electra.Error"));
	if (Electra::IsAnalyticsEventEnabled(kEventNameElectraError))
	{
		TSharedPtr<FAnalyticsEvent> AnalyticEvent = CreateAnalyticsEvent(kEventNameElectraError);
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("Reason"), *ErrorReason));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("LastState"), *Statistics.LastState));
		AnalyticEvent->ParamArray.Add(FAnalyticsEventAttribute(TEXT("MessageHistory"), MessageHistory));
		EnqueueAnalyticsEvent(AnalyticEvent);
	}
}

void FElectraPlayer::HandlePlayerEventLogMessage(Electra::IInfoLog::ELevel InLogLevel, const FString& InLogMessage, int64 InPlayerWallclockMilliseconds)
{
	FString m(SanitizeMessage(InLogMessage));
	switch(InLogLevel)
	{
		case Electra::IInfoLog::ELevel::Error:
		{
			UE_LOG(LogElectraPlayer, Error, TEXT("[%p][%p] %s"), this, CurrentPlayer.Get(), *m);
			FScopeLock Lock(&StatisticsLock);
			Statistics.AddMessageToHistory(m);
			break;
		}
		case Electra::IInfoLog::ELevel::Warning:
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("[%p][%p] %s"), this, CurrentPlayer.Get(), *m);
			FScopeLock Lock(&StatisticsLock);
			Statistics.AddMessageToHistory(m);
			break;
		}
		case Electra::IInfoLog::ELevel::Info:
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] %s"), this, CurrentPlayer.Get(), *m);
			break;
		}
		case Electra::IInfoLog::ELevel::Verbose:
		{
			UE_LOG(LogElectraPlayer, Verbose, TEXT("[%p][%p] %s"), this, CurrentPlayer.Get(), *m);
			break;
		}
	}
}

void FElectraPlayer::HandlePlayerEventDroppedVideoFrame()
{
}

void FElectraPlayer::HandlePlayerEventDroppedAudioFrame()
{
}


void FElectraPlayer::FDroppedFrameStats::AddNewDrop(const FTimespan& InFrameTime, const FTimespan& InPlayerTime, void* InPtrElectraPlayer, void* InPtrCurrentPlayer)
{
	double Now = FPlatformTime::Seconds();
	++NumTotalDropped;

	FTimespan Diff = InPlayerTime - InFrameTime;
	if (Diff > WorstDeltaTime)
	{
		WorstDeltaTime = Diff;
	}

	if (Now - SystemTimeAtLastReport >= LogWarningAfterSeconds)
	{
		FString Type = FrameType == EFrameType::VideoFrame ? TEXT("video") :
					   FrameType == EFrameType::AudioFrame ? TEXT("audio") :
					   TEXT("<unknown>");

		UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] dropped %s %s @ %s. %d drops so far. Worst delta %.3f ms"), InPtrElectraPlayer, InPtrCurrentPlayer, *Type, *InFrameTime.ToString(TEXT("%h:%m:%s.%f")), *InPlayerTime.ToString(TEXT("%h:%m:%s.%f")), NumTotalDropped, WorstDeltaTime.GetTotalMilliseconds());
		SystemTimeAtLastReport = Now;
	}

	PlayerTimeAtLastReport = InPlayerTime;
}

void FElectraPlayer::UpdatePlayEndStatistics()
{
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> LockedPlayer = CurrentPlayer;
	if (!LockedPlayer.IsValid() || !LockedPlayer->AdaptivePlayer.IsValid())
	{
		return;
	}

	double PlayPos = LockedPlayer->AdaptivePlayer->GetPlayPosition().GetAsSeconds();
	Electra::FTimeRange MediaTimeline;
	Electra::FTimeValue MediaDuration;
	LockedPlayer->AdaptivePlayer->GetTimelineRange(MediaTimeline);
	MediaDuration = LockedPlayer->AdaptivePlayer->GetDuration();
	// Update statistics
	FScopeLock Lock(&StatisticsLock);
	if (Statistics.PlayPosAtStart >= 0.0 && Statistics.PlayPosAtEnd < 0.0)
	{
		Statistics.PlayPosAtEnd = PlayPos;
	}
	// Update the media timeline end.
	Statistics.MediaTimelineAtEnd = MediaTimeline;
	// Also re-set the duration in case it changed dynamically.
	Statistics.MediaDuration = MediaDuration.IsInfinity() ? -1.0 : MediaDuration.GetAsSeconds();
}


void FElectraPlayer::LogStatistics()
{
	FString SegsPerStream;

	FScopeLock Lock(&StatisticsLock);

	for(const TPair<int32, uint32>& pair : Statistics.VideoSegmentBitratesStreamed)
	{
		SegsPerStream += FString::Printf(TEXT("%d : %u\n"), pair.Key, pair.Value);
	}

	TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
	if (PinnedAdapterDelegate.IsValid())
	{
		UE_LOG(LogElectraPlayer, Log, TEXT(
			"[%p][%p] Electra player statistics:\n"\
			"OS: %s\n"\
			"GPU Adapter: %s\n"
			"URL: %s\n"\
			"Time after master playlist loaded: %.3fs\n"\
			"Time after stream playlists loaded: %.3fs\n"\
			"Time for initial buffering: %.3fs\n"\
			"Initial video stream bitrate: %d bps\n"\
			"Initial buffering bandwidth bps: %.3f\n"\
			"Initial buffering latency: %.3fs\n"\
			"Time for initial preroll: %.3fs\n"\
			"Number of times moved forward: %d\n"\
			"Number of times moved backward: %d\n"\
			"Number of times looped: %d\n"\
			"Number of times rebuffered: %d\n"\
			"Total time spent rebuffering: %.3fs\n"\
			"Longest rebuffering time: %.3fs\n"\
			"First media timeline start: %.3fs\n"\
			"First media timeline end: %.3fs\n"\
			"Last media timeline start: %.3fs\n"\
			"Last media timeline end: %.3fs\n"\
			"Media duration: %.3fs\n"\
			"Play position at start: %.3fs\n"\
			"Play position at end: %.3fs\n"\
			"Number of quality upswitches: %d\n"\
			"Number of quality downswitches: %d\n"\
			"Number of drastic downswitches: %d\n"\
			"Bytes of video data streamed: %lld\n"\
			"Bytes of audio data streamed: %lld\n"\
			"Number of segments fetched across all quality levels:\n%s"\
			"Currently active playlist URL: %s\n"\
			"Currently active resolution: %d * %d\n" \
			"Current state: %s\n" \
			"Number of video frames dropped: %d, worst time delta %.3f ms\n" \
			"Number of audio frames dropped: %d, worst time delta %.3f ms\n" \
			"Last error: %s\n" \
			"Subtitles URL: %s\n" \
			"Subtitles response time: %.3fs\n" \
			"Subtitles last error: %s\n"
		),
			this, CurrentPlayer.Get(),
			*FString::Printf(TEXT("%s"), *AnalyticsOSVersion),
			*PinnedAdapterDelegate->GetVideoAdapterName().TrimStartAndEnd(),
			*SanitizeMessage(Statistics.InitialURL),
			Statistics.TimeToLoadMasterPlaylist,
			Statistics.TimeToLoadStreamPlaylists,
			Statistics.InitialBufferingDuration,
			Statistics.InitialStreamBitrate,
			Statistics.InitialBufferingBandwidth.GetAverageBandwidth(),
			Statistics.InitialBufferingBandwidth.GetAverageLatency(),
			Statistics.TimeForInitialPreroll,
			Statistics.NumTimesForwarded,
			Statistics.NumTimesRewound,
			Statistics.NumTimesLooped,
			Statistics.NumTimesRebuffered,
			Statistics.TotalRebufferingDuration,
			Statistics.LongestRebufferingDuration,
			Statistics.MediaTimelineAtStart.Start.GetAsSeconds(-1.0),
			Statistics.MediaTimelineAtStart.End.GetAsSeconds(-1.0),
			Statistics.MediaTimelineAtEnd.Start.GetAsSeconds(-1.0),
			Statistics.MediaTimelineAtEnd.End.GetAsSeconds(-1.0),
			Statistics.MediaDuration,
			Statistics.PlayPosAtStart,
			Statistics.PlayPosAtEnd,
			Statistics.NumQualityUpswitches,
			Statistics.NumQualityDownswitches,
			Statistics.NumQualityDrasticDownswitches,
			(long long int)Statistics.NumVideoDatabytesStreamed,
			(long long int)Statistics.NumAudioDatabytesStreamed,
			*SegsPerStream,
			*SanitizeMessage(Statistics.CurrentlyActivePlaylistURL),
			Statistics.CurrentlyActiveResolutionWidth,
			Statistics.CurrentlyActiveResolutionHeight,
			*Statistics.LastState,
			Statistics.DroppedVideoFrames.NumTotalDropped,
			Statistics.DroppedVideoFrames.WorstDeltaTime.GetTotalMilliseconds(),
			Statistics.DroppedAudioFrames.NumTotalDropped,
			Statistics.DroppedAudioFrames.WorstDeltaTime.GetTotalMilliseconds(),
			*SanitizeMessage(Statistics.LastError),
			*SanitizeMessage(Statistics.SubtitlesURL),
			Statistics.SubtitlesResponseTime,
			*Statistics.SubtitlesLastError
		);
		
		if (Statistics.LastError.Len())
		{
			FString MessageHistory;
			for(auto &msg : Statistics.MessageHistoryBuffer)
			{
				MessageHistory.Append(msg);
				MessageHistory.Append(TEXT("\n"));
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("Most recent log messages:\n%s"), *MessageHistory);
		}
	}
}


void FElectraPlayer::SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider, const FGuid& InPlayerGuid)
{
	if (PlayerGuid != InPlayerGuid)
	{
		return;
	}
	if (!AnalyticsProvider.IsValid())
	{
		return;
	}

	if (!Statistics.bDidPlaybackEnd)
	{
		UE_LOG(LogElectraPlayer, Warning, TEXT("[%p][%p] Submitting analytics during playback, some data may be incomplete"), this, CurrentPlayer.Get());
		// Try to fill in some of the blanks.
		UpdatePlayEndStatistics();
	}

	UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] Submitting analytics"), this, CurrentPlayer.Get());


	// First emit all enqueued events before sending the final one.
	SendPendingAnalyticMetrics(AnalyticsProvider);


	TArray<FAnalyticsEventAttribute> ParamArray;
	AddCommonAnalyticsAttributes(ParamArray);
	StatisticsLock.Lock();
	FString MessageHistory;
	for(auto &msg : Statistics.MessageHistoryBuffer)
	{
		MessageHistory.Append(msg);
		MessageHistory.Append(TEXT("\n"));
	}
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), Statistics.InitialURL));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("LastState"), Statistics.LastState));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MessageHistory"), MessageHistory));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("LastError"), Statistics.LastError));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("FinalVideoResolution"), FString::Printf(TEXT("%d*%d"), Statistics.CurrentlyActiveResolutionWidth, Statistics.CurrentlyActiveResolutionHeight)));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeElapsedToMasterPlaylist"), Statistics.TimeToLoadMasterPlaylist));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeElapsedToPlaylists"), Statistics.TimeToLoadStreamPlaylists));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InitialAvgBufferingBandwidth"), Statistics.InitialBufferingBandwidth.GetAverageBandwidth()));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InitialAvgBufferingLatency"), Statistics.InitialBufferingBandwidth.GetAverageLatency()));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InitialVideoBitrate"), Statistics.InitialStreamBitrate));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InitialBufferingDuration"), Statistics.InitialBufferingDuration));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InitialPrerollDuration"), Statistics.TimeForInitialPreroll));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeElapsedUntilReady"), Statistics.TimeForInitialPreroll + Statistics.TimeAtPrerollBegin - Statistics.TimeAtOpen));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.First.Start"), Statistics.MediaTimelineAtStart.Start.GetAsSeconds(-1.0)));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.First.End"), Statistics.MediaTimelineAtStart.End.GetAsSeconds(-1.0)));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.Last.Start"), Statistics.MediaTimelineAtEnd.Start.GetAsSeconds(-1.0)));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaTimeline.Last.End"), Statistics.MediaTimelineAtEnd.End.GetAsSeconds(-1.0)));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("MediaDuration"), Statistics.MediaDuration));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPosAtStart"), Statistics.PlayPosAtStart));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayPosAtEnd"), Statistics.PlayPosAtEnd));
// FIXME: the difference is pointless as it does not tell how long playback was really performed for unless we are tracking an uninterrupted playback of a Live session.
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlaybackDuration"), Statistics.PlayPosAtEnd >= 0.0 ? Statistics.PlayPosAtEnd - Statistics.PlayPosAtStart : 0.0));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumTimesMovedForward"), (uint32) Statistics.NumTimesForwarded));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumTimesMovedBackward"), (uint32) Statistics.NumTimesRewound));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumTimesLooped"), (uint32) Statistics.NumTimesLooped));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("AbortedSegmentDownloads"), (uint32) Statistics.NumSegmentDownloadsAborted));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumQualityUpswitches"), (uint32) Statistics.NumQualityUpswitches));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumQualityDownswitches"), (uint32) Statistics.NumQualityDownswitches));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumQualityDrasticDownswitches"), (uint32) Statistics.NumQualityDrasticDownswitches));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Rebuffering.Num"), (uint32)Statistics.NumTimesRebuffered));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Rebuffering.AvgDuration"), Statistics.NumTimesRebuffered > 0 ? Statistics.TotalRebufferingDuration / Statistics.NumTimesRebuffered : 0.0));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Rebuffering.MaxDuration"), Statistics.LongestRebufferingDuration));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumBytesStreamedAudio"), (double) Statistics.NumAudioDatabytesStreamed));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumBytesStreamedVideo"), (double) Statistics.NumVideoDatabytesStreamed));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumVideoFramesDropped"), Statistics.DroppedVideoFrames.NumTotalDropped));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("WorstVideoTimeDeltaMillis"), Statistics.DroppedVideoFrames.WorstDeltaTime.GetTotalMilliseconds()));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("NumAudioFramesDropped"), Statistics.DroppedAudioFrames.NumTotalDropped));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("WorstAudioTimeDeltaMillis"), Statistics.DroppedAudioFrames.WorstDeltaTime.GetTotalMilliseconds()));
	FString SegsPerStream;
	for(const TPair<int32, uint32>& pair : Statistics.VideoSegmentBitratesStreamed)
	{
		SegsPerStream += FString::Printf(TEXT("%d:%u;"), pair.Key, pair.Value);
	}
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("VideoSegmentFetchStats"), *SegsPerStream));

	ParamArray.Add(FAnalyticsEventAttribute(TEXT("SubtitlesURL"), Statistics.SubtitlesURL));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("SubtitlesResponseTime"), Statistics.SubtitlesResponseTime));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("SubtitlesLastError"), Statistics.SubtitlesLastError));
	StatisticsLock.Unlock();

	AnalyticsProvider->RecordEvent(TEXT("Electra.FinalMetrics"), MoveTemp(ParamArray));
}

void FElectraPlayer::SendAnalyticMetricsPerMinute(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider)
{
	SendPendingAnalyticMetrics(AnalyticsProvider);

	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> Player = CurrentPlayer;
	if (Player.Get() && Player->AdaptivePlayer->IsPlaying())
	{
		TArray<FAnalyticsEventAttribute> ParamArray;
		AddCommonAnalyticsAttributes(ParamArray);
		StatisticsLock.Lock();
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), Statistics.CurrentlyActivePlaylistURL));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("VideoResolution"), FString::Printf(TEXT("%d*%d"), Statistics.CurrentlyActiveResolutionWidth, Statistics.CurrentlyActiveResolutionHeight)));
		StatisticsLock.Unlock();
		AnalyticsProvider->RecordEvent(TEXT("Electra.PerMinuteMetrics"), MoveTemp(ParamArray));
	}
}

void FElectraPlayer::SendPendingAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider)
{
	FScopeLock Lock(&StatisticsLock);
	TSharedPtr<FAnalyticsEvent> AnalyticEvent;
	while(QueuedAnalyticEvents.Dequeue(AnalyticEvent))
	{
		AnalyticsProvider->RecordEvent(*AnalyticEvent->EventName, MoveTemp(AnalyticEvent->ParamArray));
		FPlatformAtomics::InterlockedDecrement(&NumQueuedAnalyticEvents);
	}
}


void FElectraPlayer::ReportVideoStreamingError(const FGuid& InPlayerGuid, const FString& LastError)
{
	if (PlayerGuid != InPlayerGuid)
	{
		return;
	}

	FScopeLock Lock(&StatisticsLock);
	// Only replace a blank string with a non-blank string. We want to preserve
	// existing last error messages, as they will be the root of the problem.
	if (LastError.Len() > 0 && Statistics.LastError.Len() == 0)
	{
		Statistics.LastError = LastError;
	}
}

void FElectraPlayer::ReportSubtitlesMetrics(const FGuid& InPlayerGuid, const FString& URL, double ResponseTime, const FString& LastError)
{
	if (PlayerGuid != InPlayerGuid)
	{
		return;
	}

	Statistics.SubtitlesURL = URL;
	Statistics.SubtitlesResponseTime = ResponseTime;
	Statistics.SubtitlesLastError = LastError;
}

void FElectraPlayer::MediaStateOnPreparingFinished()
{
	if (!ensure(PlayerState.State == EPlayerState::Preparing))
	{
		return;
	}

	CSV_EVENT(ElectraPlayer, TEXT("MediaStateOnPreparingFinished"));

	PlayerState.State = EPlayerState::Stopped;
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::MediaOpened);
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::TracksChanged);
}

bool FElectraPlayer::MediaStateOnPlay()
{
	if (PlayerState.State != EPlayerState::Stopped && PlayerState.State != EPlayerState::Paused)
	{
		return false;
	}

	CSV_EVENT(ElectraPlayer, TEXT("MediaStateOnPlay"));

	PlayerState.State = EPlayerState::Playing;
	PlayerState.SetPlayRateFromPlayer(1.0f);

	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackResumed);
	return true;
}

bool FElectraPlayer::MediaStateOnPause()
{
	if (PlayerState.State != EPlayerState::Playing)
	{
		return false;
	}

	CSV_EVENT(ElectraPlayer, TEXT("MediaStateOnPause"));

	PlayerState.State = EPlayerState::Paused;
	PlayerState.SetPlayRateFromPlayer(0.0f);

	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackSuspended);
	return true;
}

void FElectraPlayer::MediaStateOnEndReached()
{
	CSV_EVENT(ElectraPlayer, TEXT("MediaStateOnEndReached"));

	switch (PlayerState.State)
	{
	case EPlayerState::Preparing:
	case EPlayerState::Playing:
	case EPlayerState::Paused:
	case EPlayerState::Stopped:
		DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackEndReached);
		break;
	default:
		// NOP
		break;
	}
	PlayerState.State = EPlayerState::Stopped;
}

void FElectraPlayer::MediaStateOnSeekFinished()
{
	CSV_EVENT(ElectraPlayer, TEXT("MediaStateOnSeekFinished"));
	DeferredEvents.Enqueue(IElectraPlayerAdapterDelegate::EPlayerEvent::SeekCompleted);
}

// ------------------------------------------------------------------------------------------------------------------

FElectraPlayer::FAdaptiveStreamingPlayerResourceProvider::FAdaptiveStreamingPlayerResourceProvider(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& InAdapterDelegate)
	: AdapterDelegate(InAdapterDelegate)
{
}

void FElectraPlayer::FAdaptiveStreamingPlayerResourceProvider::ProvideStaticPlaybackDataForURL(TSharedPtr<Electra::IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe> InOutRequest)
{
	check(InOutRequest.IsValid());
	PendingStaticResourceRequests.Enqueue(InOutRequest);
}

void FElectraPlayer::FAdaptiveStreamingPlayerResourceProvider::ProcessPendingStaticResourceRequests()
{
	TSharedPtr<Electra::IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe> InOutRequest;
	while (PendingStaticResourceRequests.Dequeue(InOutRequest))
	{
		check(InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::Playlist ||
			  InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::LicenseKey);
		if (InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::Playlist)
		{
			TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
			if (PinnedAdapterDelegate.IsValid())
			{
				FVariantValue Value = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::PlayListData, FVariantValue(InOutRequest->GetResourceURL()));
				if (Value.IsValid())
				{
					FString PlaylistData = Value.GetFString();
					// There needs to be a non-empty return that is also _not_ the default value we have provided!
					// The latter being a quirk for a FN specific GetMediaOption() that takes the _default-value_ as the URL to look up the
					// playlist contents for. When we are not in FN and have the standard engine version only we get the URL we pass in back
					// since that's the default value when the key to look up has not been found.
					if (!PlaylistData.IsEmpty() && PlaylistData != InOutRequest->GetResourceURL())
					{
						/*
						UE_LOG(LogElectraPlayer, Log, TEXT("[%p][%p] IMediaPlayer::Open: Source provided playlist data for '%s'"), this, CurrentPlayer.Get(), *SanitizeMessage(InOutRequest->GetResourceURL()));
						*/

						// FString is Unicode but the HTTP response for an HLS playlist is a UTF-8 string.
						// Create a plain array from this.
						TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ResponseDataPtr = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>((const uint8*)TCHAR_TO_UTF8(*PlaylistData), PlaylistData.Len());
						// And put it into the request
						InOutRequest->SetPlaybackData(ResponseDataPtr);
					}
				}
			}
		}
		else if (InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::LicenseKey)
		{
			TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin();
			if (PinnedAdapterDelegate.IsValid())
			{
				FVariantValue Value = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::LicenseKeyData, FVariantValue(InOutRequest->GetResourceURL()));
				if (Value.IsValid())
				{
					FString LicenseKeyData = Value.GetFString();
					if (!LicenseKeyData.IsEmpty() && LicenseKeyData != InOutRequest->GetResourceURL())
					{
						TArray<uint8> BinKey;
						BinKey.AddUninitialized(LicenseKeyData.Len());
						BinKey.SetNum(HexToBytes(LicenseKeyData, BinKey.GetData()));
						TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ResponseDataPtr = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>(BinKey);
						InOutRequest->SetPlaybackData(ResponseDataPtr);
					}
				}
			}
		}
		InOutRequest->SignalDataReady();
	}
}

void FElectraPlayer::FAdaptiveStreamingPlayerResourceProvider::ClearPendingRequests()
{
	PendingStaticResourceRequests.Empty();
}

// ------------------------------------------------------------------------------------------------------------------

IElectraPlayerInterface* FElectraPlayerRuntimeFactory::CreatePlayer(const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate,
	FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
	FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
	FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
	FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate)
{
	return new FElectraPlayer(AdapterDelegate, InSendAnalyticMetricsDelegate, InSendAnalyticMetricsPerMinuteDelegate, InReportVideoStreamingErrorDelegate, InReportSubtitlesFileMetricsDelegate);
};
