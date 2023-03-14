// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/AdaptiveStreamingPlayerInternal.h"
#include "ElectraPlayerPrivate.h"
#include "PlayerRuntimeGlobal.h"

#include "StreamAccessUnitBuffer.h"
#include "Player/AdaptiveStreamingPlayerABR.h"
#include "Player/PlayerLicenseKey.h"
#include "Player/AdaptivePlayerOptionKeynames.h"
#include "Utilities/Utilities.h"
#include "Utilities/ISO639-Map.h"
#include "Player/DRM/DRMManager.h"

#include "HAL/LowLevelMemTracker.h"


DECLARE_CYCLE_STAT(TEXT("FAdaptiveStreamingPlayer::WorkerThread"), STAT_ElectraPlayer_AdaptiveWorker, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FAdaptiveStreamingPlayer::EventThread"), STAT_ElectraPlayer_EventWorker, STATGROUP_ElectraPlayer);


namespace Electra
{
FAdaptiveStreamingPlayer *FAdaptiveStreamingPlayer::PointerToLatestPlayer;

TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> IAdaptiveStreamingPlayer::Create(const IAdaptiveStreamingPlayer::FCreateParam& InCreateParameters)
{
	return MakeShared<FAdaptiveStreamingPlayer, ESPMode::ThreadSafe>(InCreateParameters);
}

void IAdaptiveStreamingPlayer::DebugHandle(void* pPlayer, void (*debugDrawPrintf)(void* pPlayer, const char *pFmt, ...))
{
	FAdaptiveStreamingPlayer::DebugHandle(pPlayer, debugDrawPrintf);
}


#if PLATFORM_ANDROID

FParamDict& IAdaptiveStreamingPlayer::Android_Workarounds(FStreamCodecInformation::ECodec InForCodec)
{
	return FAdaptiveStreamingPlayer::Android_Workarounds(InForCodec);
}

#endif


//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------


namespace
{
	inline int32 StreamTypeToArrayIndex(EStreamType StreamType)
	{
		switch(StreamType)
		{
			case EStreamType::Video:
				return 0;
			case EStreamType::Audio:
				return 1;
			case EStreamType::Subtitle:
				return 2;
			default:
				return 3;
		}
	}
}

//-----------------------------------------------------------------------------
/**
 * CTOR
 */
FAdaptiveStreamingPlayer::FAdaptiveStreamingPlayer(const IAdaptiveStreamingPlayer::FCreateParam& InCreateParameters)
{
	Electra::AddActivePlayerInstance();

	ExternalPlayerGUID		= InCreateParameters.ExternalPlayerGUID;

	ManifestType			= EMediaFormatType::Unknown;

	VideoRender.Renderer    = CreateWrappedRenderer(InCreateParameters.VideoRenderer, EStreamType::Video);
	AudioRender.Renderer    = CreateWrappedRenderer(InCreateParameters.AudioRenderer, EStreamType::Audio);
	CurrentState   		    = EPlayerState::eState_Idle;
	PipelineState  		    = EPipelineState::ePipeline_Stopped;
	DecoderState   		    = EDecoderState::eDecoder_Paused;
	StreamState			    = EStreamState::eStream_Running;
	PlaybackRate   		    = 0.0;
	RenderRateScale			= 1.0;
	StreamReaderHandler     = nullptr;
	bStreamingHasStarted	= false;
	RebufferCause   	    = ERebufferCause::None;
	Manifest  				= nullptr;
	LastBufferingState 		= EPlayerState::eState_Buffering;
	bIsClosing 				= false;

	RebufferDetectedAtPlayPos.SetToInvalid();

	CurrentPlaybackSequenceID[0] = 0;
	CurrentPlaybackSequenceID[1] = 0;
	CurrentPlaybackSequenceID[2] = 0;
	CurrentPlaybackSequenceID[3] = 0;
	CurrentPlaybackSequenceState.Reset();

	bIsVideoDeselected = false;
	bIsAudioDeselected = false;
	bIsTextDeselected = false;

	DataAvailabilityStateVid.StreamType = EStreamType::Video;
	DataAvailabilityStateVid.Availability = Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable;
	DataAvailabilityStateAud.StreamType = EStreamType::Audio;
	DataAvailabilityStateAud.Availability = Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable;
	DataAvailabilityStateTxt.StreamType = EStreamType::Subtitle;
	DataAvailabilityStateTxt.Availability = Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable;

	// Subtitles need to be explicitly selected by the user.
	SelectedStreamAttributesTxt.Deselect();


	// Create the UTC time handler for this player session
	SynchronizedUTCTime = ISynchronizedUTCTime::Create();
	SynchronizedUTCTime->SetTime(MEDIAutcTime::Current());

	// Create the render clock
	RenderClock = MakeSharedTS<FMediaRenderClock>();

	// Create the AEMS handler. This is needed early on for the client to register receivers on.
	AEMSEventHandler = IAdaptiveStreamingPlayerAEMSHandler::Create();

	bShouldBePaused  = false;
	bShouldBePlaying = false;

	SeekVars.Reset();
	CurrentLoopState = {};
	NextLoopStates.Empty();
	bFirstSegmentRequestIsForLooping = false;

	BitrateCeiling = 0;
	VideoResolutionLimitWidth = 0;
	VideoResolutionLimitHeight = 0;

	TMediaInterlockedExchangePointer(PointerToLatestPlayer, this);

	bUseSharedWorkerThread = true;
	StartWorkerThread();
}



//-----------------------------------------------------------------------------
/**
 * DTOR
 */
FAdaptiveStreamingPlayer::~FAdaptiveStreamingPlayer()
{
	TMediaInterlockedExchangePointer(PointerToLatestPlayer, (FAdaptiveStreamingPlayer*)0);

	FMediaEvent doneSig;
	WorkerThread.SendCloseMessage(&doneSig);
	doneSig.Wait();
	StopWorkerThread();

	MetricListenerCriticalSection.Lock();
	MetricListeners.Empty();
	MetricListenerCriticalSection.Unlock();

	RenderClock.Reset();
	delete SynchronizedUTCTime;

	delete AEMSEventHandler;

	Electra::RemoveActivePlayerInstance();
}



//-----------------------------------------------------------------------------
/**
 * Returns the error event. If there is no error an empty pointer will be returned.
 *
 * @return
 */
FErrorDetail FAdaptiveStreamingPlayer::GetError() const
{
	FMediaCriticalSection::ScopedLock lock(DiagnosticsCriticalSection);
	return LastErrorDetail;
}










void FAdaptiveStreamingPlayer::GetStreamBufferUtilization(FAccessUnitBufferInfo& OutInfo, EStreamType BufferType)
{
	FScopeLock lock(&DataBuffersCriticalSection);

	// Start with the current buffer.
	bool bFirst = true;
	if (ActiveDataOutputBuffers.IsValid())
	{
		TSharedPtrTS<FMultiTrackAccessUnitBuffer> Buffer = ActiveDataOutputBuffers->GetBuffer(BufferType);
		if (Buffer.IsValid())
		{
			Buffer->GetStats(OutInfo);
			bFirst = false;
		}
	}
	// Then add all the upcoming buffers. This list includes the current write buffer at the end
	// so we must not add that one as well.
	for(int32 i=0,iMax=NextDataBuffers.Num(); i<iMax; ++i)
	{
		TSharedPtrTS<FMultiTrackAccessUnitBuffer> Buffer = NextDataBuffers[i]->GetBuffer(BufferType);
		if (Buffer.IsValid())
		{
			if (bFirst)
			{
				Buffer->GetStats(OutInfo);
				bFirst = false;
			}
			else
			{
				FAccessUnitBufferInfo info;
				Buffer->GetStats(info);
				OutInfo.PushedDuration += info.PushedDuration;
				OutInfo.PlayableDuration += info.PlayableDuration;
				OutInfo.CurrentMemInUse += info.CurrentMemInUse;
				OutInfo.NumCurrentAccessUnits += info.NumCurrentAccessUnits;
				// Propagate any end of data state to the overall stats. This is important
				// to determine when the current buffer has finished playing.
				if (info.bEndOfData)
				{
					OutInfo.bEndOfData = info.bEndOfData;
				}
				if (i == iMax-1)
				{
					OutInfo.bLastPushWasBlocked = info.bLastPushWasBlocked;
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
/**
 * Updates the internally kept diagnostics value with the current
 * decoder live values.
 */
void FAdaptiveStreamingPlayer::UpdateDiagnostics()
{
	FAccessUnitBufferInfo vidInf;
	FAccessUnitBufferInfo audInf;
	FAccessUnitBufferInfo txtInf;
	GetStreamBufferUtilization(vidInf, EStreamType::Video);
	GetStreamBufferUtilization(audInf, EStreamType::Audio);
	GetStreamBufferUtilization(txtInf, EStreamType::Subtitle);
	int64 tNow = MEDIAutcTime::CurrentMSec();
	DiagnosticsCriticalSection.Lock();
	VideoBufferStats.StreamBuffer = vidInf;
	AudioBufferStats.StreamBuffer = audInf;
	TextBufferStats.StreamBuffer = txtInf;
	VideoBufferStats.UpdateStalledDuration(tNow);
	AudioBufferStats.UpdateStalledDuration(tNow);
	TextBufferStats.UpdateStalledDuration(tNow);
	DiagnosticsCriticalSection.Unlock();
}




//-----------------------------------------------------------------------------
/**
 * Initializes the player instance.
 *
 * @param Options
 */
void FAdaptiveStreamingPlayer::Initialize(const FParamDict& Options)
{
	PlayerOptions = Options;
	WorkerThread.SendInitializeMessage();
}

void FAdaptiveStreamingPlayer::ModifyOptions(const FParamDict& InOptionsToSetOrChange, const FParamDict& InOptionsToClear)
{
	// Remove options
	TArray<FString> Keys;
	InOptionsToClear.GetKeysStartingWith(FString(), Keys);
	for(auto &Key : Keys)
	{
		PlayerOptions.Remove(Key);
	}

	InOptionsToSetOrChange.GetKeysStartingWith(FString(), Keys);
	for(auto &Key : Keys)
	{
		FVariantValue Value = InOptionsToSetOrChange.GetValue(Key);
		PlayerOptions.SetOrUpdate(Key, Value);
	}
}


void FAdaptiveStreamingPlayer::SetStaticResourceProviderCallback(const TSharedPtr<IAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe>& InStaticResourceProvider)
{
	StaticResourceProvider = InStaticResourceProvider;
}

void FAdaptiveStreamingPlayer::SetVideoDecoderResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& InResourceDelegate)
{
	VideoDecoderResourceDelegate = InResourceDelegate;
}


void FAdaptiveStreamingPlayer::AddMetricsReceiver(IAdaptiveStreamingPlayerMetrics* InMetricsReceiver)
{
	if (InMetricsReceiver)
	{
		FMediaCriticalSection::ScopedLock lock(MetricListenerCriticalSection);
		if (MetricListeners.Find(InMetricsReceiver) == INDEX_NONE)
		{
			MetricListeners.Push(InMetricsReceiver);
		}
	}
}

void FAdaptiveStreamingPlayer::RemoveMetricsReceiver(IAdaptiveStreamingPlayerMetrics* InMetricsReceiver)
{
	if (InMetricsReceiver)
	{
		FMediaCriticalSection::ScopedLock lock(MetricListenerCriticalSection);
		/*bool bRemoved =*/ MetricListeners.Remove(InMetricsReceiver);
	}
}

void FAdaptiveStreamingPlayer::AddAEMSReceiver(TWeakPtrTS<IAdaptiveStreamingPlayerAEMSReceiver> InReceiver, FString InForSchemeIdUri, FString InForValue, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode InDispatchMode)
{
	AEMSEventHandler->AddAEMSReceiver(InReceiver, InForSchemeIdUri, InForValue, InDispatchMode, true);
}

void FAdaptiveStreamingPlayer::RemoveAEMSReceiver(TWeakPtrTS<IAdaptiveStreamingPlayerAEMSReceiver> InReceiver, FString InForSchemeIdUri, FString InForValue, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode InDispatchMode)
{
	AEMSEventHandler->RemoveAEMSReceiver(InReceiver, InForSchemeIdUri, InForValue, InDispatchMode);
}


void FAdaptiveStreamingPlayer::AddSubtitleReceiver(TWeakPtrTS<IAdaptiveStreamingPlayerSubtitleReceiver> InReceiver)
{
	if (InReceiver.IsValid())
	{
		FScopeLock lock(&SubtitleReceiversCriticalSection);
		SubtitleReceivers.Add(InReceiver);
	}
}

void FAdaptiveStreamingPlayer::RemoveSubtitleReceiver(TWeakPtrTS<IAdaptiveStreamingPlayerSubtitleReceiver> InReceiver)
{
	if (InReceiver.IsValid())
	{
		FScopeLock lock(&SubtitleReceiversCriticalSection);
		SubtitleReceivers.Remove(InReceiver);
	}
}

void FAdaptiveStreamingPlayer::OnDecodedSubtitleReceived(ISubtitleDecoderOutputPtr Subtitle)
{
	FScopeLock lock(&SubtitleReceiversCriticalSection);
	for(auto &Receiver : SubtitleReceivers)
	{
		TSharedPtrTS<IAdaptiveStreamingPlayerSubtitleReceiver> Recv = Receiver.Pin();
		if (Recv.IsValid())
		{
			Recv->OnMediaPlayerSubtitleReceived(Subtitle);
		}
	}
}

void FAdaptiveStreamingPlayer::OnFlushSubtitleReceivers()
{
	FScopeLock lock(&SubtitleReceiversCriticalSection);
	for(auto &Receiver : SubtitleReceivers)
	{
		TSharedPtrTS<IAdaptiveStreamingPlayerSubtitleReceiver> Recv = Receiver.Pin();
		if (Recv.IsValid())
		{
			Recv->OnMediaPlayerFlushSubtitles();
		}
	}
}




//-----------------------------------------------------------------------------
/**
 * Dispatches an event to the listeners.
 *
 * @param pEvent
 */
void FAdaptiveStreamingPlayer::DispatchEvent(TSharedPtrTS<FMetricEvent> Event)
{
	if (EventDispatcher.IsValid())
	{
		Event->Player = SharedThis(this);
		EventDispatcher->DispatchEvent(Event);
	}
}

void FAdaptiveStreamingPlayer::FireSyncEvent(TSharedPtrTS<FMetricEvent> Event)
{
	LockMetricsReceivers();
	const TArray<IAdaptiveStreamingPlayerMetrics*, TInlineAllocator<4>>& Listeners = GetMetricsReceivers();
	for(int32 i=0; i<Listeners.Num(); ++i)
	{
		switch(Event->Type)
		{
			case FMetricEvent::EType::OpenSource:
				Listeners[i]->ReportOpenSource(Event->Param.URL);
				break;
			case FMetricEvent::EType::ReceivedMasterPlaylist:
				Listeners[i]->ReportReceivedMasterPlaylist(Event->Param.URL);
				break;
			case FMetricEvent::EType::ReceivedPlaylists:
				Listeners[i]->ReportReceivedPlaylists();
				break;
			case FMetricEvent::EType::TracksChanged:
				Listeners[i]->ReportTracksChanged();
				break;
			case FMetricEvent::EType::CleanStart:
				Listeners[i]->ReportCleanStart();
				break;
			case FMetricEvent::EType::BufferingStart:
				Listeners[i]->ReportBufferingStart(Event->Param.BufferingReason);
				break;
			case FMetricEvent::EType::BufferingEnd:
				Listeners[i]->ReportBufferingEnd(Event->Param.BufferingReason);
				break;
			case FMetricEvent::EType::Bandwidth:
				Listeners[i]->ReportBandwidth(Event->Param.Bandwidth.EffectiveBps, Event->Param.Bandwidth.ThroughputBps, Event->Param.Bandwidth.Latency);
				break;
			case FMetricEvent::EType::BufferUtilization:
				Listeners[i]->ReportBufferUtilization(Event->Param.BufferStats);
				break;
			case FMetricEvent::EType::PlaylistDownload:
				Listeners[i]->ReportPlaylistDownload(Event->Param.PlaylistStats);
				break;
			case FMetricEvent::EType::SegmentDownload:
				Listeners[i]->ReportSegmentDownload(Event->Param.SegmentStats);
				break;
			case FMetricEvent::EType::LicenseKey:
				Listeners[i]->ReportLicenseKey(Event->Param.LicenseKeyStats);
				break;
			case FMetricEvent::EType::DataAvailabilityChange:
				Listeners[i]->ReportDataAvailabilityChange(Event->Param.DataAvailability);
				break;
			case FMetricEvent::EType::VideoQualityChange:
				Listeners[i]->ReportVideoQualityChange(Event->Param.QualityChange.NewBitrate, Event->Param.QualityChange.PrevBitrate, Event->Param.QualityChange.bIsDrastic);
				break;
			case FMetricEvent::EType::CodecFormatChange:
				Listeners[i]->ReportDecodingFormatChange(Event->Param.CodecFormatChange);
				break;
			case FMetricEvent::EType::PrerollStart:
				Listeners[i]->ReportPrerollStart();
				break;
			case FMetricEvent::EType::PrerollEnd:
				Listeners[i]->ReportPrerollEnd();
				break;
			case FMetricEvent::EType::PlaybackStart:
				Listeners[i]->ReportPlaybackStart();
				break;
			case FMetricEvent::EType::PlaybackPaused:
				Listeners[i]->ReportPlaybackPaused();
				break;
			case FMetricEvent::EType::PlaybackResumed:
				Listeners[i]->ReportPlaybackResumed();
				break;
			case FMetricEvent::EType::PlaybackEnded:
				Listeners[i]->ReportPlaybackEnded();
				break;
			case FMetricEvent::EType::PlaybackJumped:
				Listeners[i]->ReportJumpInPlayPosition(Event->Param.TimeJump.ToNewTime, Event->Param.TimeJump.FromTime, Event->Param.TimeJump.Reason);
				break;
			case FMetricEvent::EType::PlaybackStopped:
				Listeners[i]->ReportPlaybackStopped();
				break;
			case FMetricEvent::EType::SeekCompleted:
				Listeners[i]->ReportSeekCompleted();
				break;
			case FMetricEvent::EType::Errored:
				Listeners[i]->ReportError(Event->Param.ErrorDetail.GetPrintable());
				break;
			case FMetricEvent::EType::LogMessage:
				Listeners[i]->ReportLogMessage(Event->Param.LogMessage.Level, Event->Param.LogMessage.Message, Event->Param.LogMessage.AtMillis);
				break;
		}
	}
	UnlockMetricsReceivers();
	if (Event->EventSignal)
	{
		Event->EventSignal->Signal();
	}
}


//-----------------------------------------------------------------------------
/**
 * Starts the worker thread.
 */
void FAdaptiveStreamingPlayer::StartWorkerThread()
{
	// Get us an event dispatcher and add ourselves to the shared worker thread.
	if (!EventDispatcher.IsValid())
	{
		EventDispatcher = FAdaptiveStreamingPlayerEventHandler::Create();
	}
	if (!SharedWorkerThread.IsValid())
	{
		SharedWorkerThread = FAdaptiveStreamingPlayerWorkerThread::Create(bUseSharedWorkerThread);
		WorkerThread.SetSharedWorkerThread(SharedWorkerThread);
		SharedWorkerThread->AddPlayerInstance(this);
	}
}


//-----------------------------------------------------------------------------
/**
 * Stops the worker thread.
 */
void FAdaptiveStreamingPlayer::StopWorkerThread()
{
	if (SharedWorkerThread.IsValid())
	{
		WorkerThread.SetSharedWorkerThread(nullptr);
		SharedWorkerThread->RemovePlayerInstance(this);
		SharedWorkerThread.Reset();
	}
	EventDispatcher.Reset();
}


//-----------------------------------------------------------------------------
/**
 * Enables or disables frame accurate seeking.
 */
void FAdaptiveStreamingPlayer::EnableFrameAccurateSeeking(bool bEnabled)
{
	GetOptions().Set(OptionKeyFrameAccurateSeek, FVariantValue(bEnabled));
}


//-----------------------------------------------------------------------------
/**
 * Sets the playback range.
 */
void FAdaptiveStreamingPlayer::SetPlaybackRange(const FPlaybackRange& InPlaybackRange)
{
	FTimeValue RangeStartNow = GetOptions().GetValue(OptionPlayRangeStart).SafeGetTimeValue(FTimeValue());
	FTimeValue RangeEndNow = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());
	FTimeValue NewRangeStart = InPlaybackRange.Start.IsSet() ? InPlaybackRange.Start.GetValue() : FTimeValue();
	FTimeValue NewRangeEnd = InPlaybackRange.End.IsSet() ? InPlaybackRange.End.GetValue() : FTimeValue();
	if (NewRangeStart != RangeStartNow || NewRangeEnd != RangeEndNow)
	{
		if (NewRangeStart.IsValid())
		{
			GetOptions().SetOrUpdate(OptionPlayRangeStart, FVariantValue(NewRangeStart));
		}
		else
		{
			GetOptions().Remove(OptionPlayRangeStart);
		}
		if (NewRangeEnd.IsValid())
		{
			GetOptions().SetOrUpdate(OptionPlayRangeEnd, FVariantValue(NewRangeEnd));
		}
		else
		{
			GetOptions().Remove(OptionPlayRangeEnd);
		}
		// Mark the range as dirty so the player can take appropriate action.
		PlaybackState.SetPlayRangeHasChanged(true);
	}
}


//-----------------------------------------------------------------------------
/**
 * Returns the currently set playback range.
 */
void FAdaptiveStreamingPlayer::GetPlaybackRange(FPlaybackRange& OutPlaybackRange)
{
	OutPlaybackRange.Start.Reset();
	OutPlaybackRange.End.Reset();
	if (GetOptions().HaveKey(OptionPlayRangeStart))
	{
		FTimeValue RangeStart = GetOptions().GetValue(OptionPlayRangeStart).SafeGetTimeValue(FTimeValue());
		if (RangeStart.IsValid())
		{
			OutPlaybackRange.Start = RangeStart;
		}
	}
	if (GetOptions().HaveKey(OptionPlayRangeEnd))
	{
		FTimeValue RangeEnd = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());
		if (RangeEnd.IsValid())
		{
			OutPlaybackRange.End = RangeEnd;
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Starts loading the manifest / master playlist.
 */
void FAdaptiveStreamingPlayer::LoadManifest(const FString& InManifestURL)
{
	WorkerThread.SendLoadManifestMessage(InManifestURL, FString());
}


//-----------------------------------------------------------------------------
/**
 * Pauses playback
 */
void FAdaptiveStreamingPlayer::Pause()
{
	WorkerThread.SendPauseMessage();
}



//-----------------------------------------------------------------------------
/**
 * Resumes playback
 */
void FAdaptiveStreamingPlayer::Resume()
{
	WorkerThread.SendResumeMessage();
}



//-----------------------------------------------------------------------------
/**
 * Seek to a new position and play from there. This includes first playstart.
 *
 * @param NewPosition
 */
void FAdaptiveStreamingPlayer::SeekTo(const FSeekParam& NewPosition)
{
	FScopeLock lock(&SeekVars.Lock);
	SeekVars.PendingRequest = NewPosition;
	WorkerThread.TriggerSharedWorkerThread();
}



//-----------------------------------------------------------------------------
/**
 * Stops playback. Playback cannot be resumed. Final player events will be sent to registered listeners.
 */
void FAdaptiveStreamingPlayer::Stop()
{
	FMediaEvent doneSig;
	WorkerThread.SendCloseMessage(&doneSig);
	doneSig.Wait();
}


//-----------------------------------------------------------------------------
/**
 * Puts playback into loop mode if possible. Live streams cannot be made to loop as they have infinite duration.
 *
 * @param InLoopParams
 */
void FAdaptiveStreamingPlayer::SetLooping(const FLoopParam& InLoopParams)
{
	WorkerThread.SendLoopMessage(InLoopParams);
}


//-----------------------------------------------------------------------------
/**
 * Returns whether or not a manifest has been loaded and assigned yet.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::HaveMetadata() const
{
	return PlaybackState.GetHaveMetadata();
}


//-----------------------------------------------------------------------------
/**
 * Returns the duration of the video. Returns invalid time when there is nothing to play. Returns positive infinite for live streams.
 *
 * @return
 */
FTimeValue FAdaptiveStreamingPlayer::GetDuration() const
{
	return PlaybackState.GetDuration();
}

//-----------------------------------------------------------------------------
/**
 * Returns the current play position. Returns -1.0 when there is nothing to play.
 *
 * @return
 */
FTimeValue FAdaptiveStreamingPlayer::GetPlayPosition() const
{
	return PlaybackState.GetPlayPosition();
}


//-----------------------------------------------------------------------------
/**
 * Returns the seekable range.
 *
 * @param OutRange
 */
void FAdaptiveStreamingPlayer::GetSeekableRange(FTimeRange& OutRange) const
{
	PlaybackState.GetSeekableRange(OutRange);
}

//-----------------------------------------------------------------------------
/**
 * Fills the provided array with time values that can be seeked to. These are segment start times from the video (or audio if there is no video) track.
 *
 * @param OutPositions
 */
void FAdaptiveStreamingPlayer::GetSeekablePositions(TArray<FTimespan>& OutPositions) const
{
	PlaybackState.GetSeekablePositions(OutPositions);
}

//-----------------------------------------------------------------------------
/**
 * Returns the timeline range.
 *
 * @param OutRange
 */
void FAdaptiveStreamingPlayer::GetTimelineRange(FTimeRange& OutRange) const
{
	PlaybackState.GetTimelineRange(OutRange);
}

//-----------------------------------------------------------------------------
/**
 * Returns true when playback has finished.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::HasEnded() const
{
	return PlaybackState.GetHasEnded();
}

//-----------------------------------------------------------------------------
/**
 * Returns true when seeking is in progress. False if not.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::IsSeeking() const
{
	return PlaybackState.GetIsSeeking();
}

//-----------------------------------------------------------------------------
/**
 * Returns true when data is being buffered/rebuffered, false otherwise.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::IsBuffering() const
{
	return PlaybackState.GetIsBuffering();
}

//-----------------------------------------------------------------------------
/**
 * Returns true when playing back, false if not.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::IsPlaying() const
{
	return PlaybackState.GetIsPlaying();
}

//-----------------------------------------------------------------------------
/**
 * Returns true when paused, false if not.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::IsPaused() const
{
	return PlaybackState.GetIsPaused();
}


//-----------------------------------------------------------------------------
/**
 * Returns the current loop state.
 *
 * @param OutLoopState
 */
void FAdaptiveStreamingPlayer::GetLoopState(FLoopState& OutLoopState) const
{
	PlaybackState.GetLoopState(OutLoopState);
}

//-----------------------------------------------------------------------------
/**
 * Returns track metadata of the currently active play period.
 *
 * @param OutTrackMetadata
 * @param StreamType
 */
void FAdaptiveStreamingPlayer::GetTrackMetadata(TArray<FTrackMetadata>& OutTrackMetadata, EStreamType StreamType) const
{
	TArray<FTrackMetadata> dummy1, dummy2;
	if (StreamType == EStreamType::Video)
	{
		PlaybackState.GetTrackMetadata(OutTrackMetadata, dummy1, dummy2);
	}
	else if (StreamType == EStreamType::Audio)
	{
		PlaybackState.GetTrackMetadata(dummy1, OutTrackMetadata, dummy2);
	}
	else if (StreamType == EStreamType::Subtitle)
	{
		PlaybackState.GetTrackMetadata(dummy1, dummy2, OutTrackMetadata);
	}
	else
	{
		OutTrackMetadata.Empty();
	}
}


#if 0
//-----------------------------------------------------------------------------
/**
 * Returns the track metadata of the currently active track. If nothing is selected yet the TOptional<> will be unset.
 *
 * @param OutSelectedTrackMetadata
 * @param StreamType
 */
void FAdaptiveStreamingPlayer::GetSelectedTrackMetadata(TOptional<FTrackMetadata>& OutSelectedTrackMetadata, EStreamType StreamType) const
{
}
#endif

void FAdaptiveStreamingPlayer::GetSelectedTrackAttributes(FStreamSelectionAttributes& OutAttributes, EStreamType StreamType) const
{
	TSharedPtrTS<FBufferSourceInfo> BufferInfo;

	auto SetOutAttributes = [&](const TSharedPtrTS<FBufferSourceInfo>& BufferInfo, const FStreamSelectionAttributes& CurrentAttr, bool bIsValidInfo) -> void
	{
		if (BufferInfo.IsValid())
		{
			OutAttributes.Kind = BufferInfo->Kind;
			OutAttributes.Language_ISO639 = BufferInfo->Language;
			OutAttributes.Codec = BufferInfo->Codec;
			OutAttributes.OverrideIndex = BufferInfo->HardIndex;
		}
		else
		{
			OutAttributes = CurrentAttr;
			// If the information is known to be valid then the stream is currently not available.
			// We set the override index to reflect this and return the intended selection.
			if (bIsValidInfo)
			{
				OutAttributes.OverrideIndex = -1;
			}
		}
	};

	FTimeValue Now = PlaybackState.GetPlayPosition();
	bool bHavePeriods;
	bool bFoundTime;
	BufferInfo = GetStreamBufferInfoAtTime(bHavePeriods, bFoundTime, StreamType, Now);
	bool bBufferInfoValid = bHavePeriods && bFoundTime;

	if (StreamType == EStreamType::Video)
	{
		SetOutAttributes(BufferInfo, SelectedStreamAttributesVid, bBufferInfoValid);
	}
	else if (StreamType == EStreamType::Audio)
	{
		SetOutAttributes(BufferInfo, SelectedStreamAttributesAud, bBufferInfoValid);
	}
	else if (StreamType == EStreamType::Subtitle)
	{
		SetOutAttributes(BufferInfo, SelectedStreamAttributesTxt, bBufferInfoValid);
	}
}


//-----------------------------------------------------------------------------
/**
 * Sets the highest bitrate when selecting a candidate stream.
 *
 * @param highestSelectableBitrate
 */
void FAdaptiveStreamingPlayer::SetBitrateCeiling(int32 highestSelectableBitrate)
{
	WorkerThread.SendBitrateMessage(EStreamType::Video, highestSelectableBitrate, 1);
}


//-----------------------------------------------------------------------------
/**
 * Sets the maximum resolution to use. Set both to 0 to disable, set only one to limit widht or height only.
 * Setting both will limit on either width or height, whichever limits first.
 *
 * @param MaxWidth
 * @param MaxHeight
 */
void FAdaptiveStreamingPlayer::SetMaxResolution(int32 MaxWidth, int32 MaxHeight)
{
	WorkerThread.SendResolutionMessage(MaxWidth, MaxHeight);
}


void FAdaptiveStreamingPlayer::SetInitialStreamAttributes(EStreamType StreamType, const FStreamSelectionAttributes& InitialSelection)
{
	WorkerThread.SendInitialStreamAttributeMessage(StreamType, InitialSelection);
}

#if 0
//-----------------------------------------------------------------------------
/**
 * Selects a track based from one of the array members returned by GetStreamMetadata().
 *
 * @param StreamType
 * @param TrackMetadata
 */
void FAdaptiveStreamingPlayer::SelectTrackByMetadata(EStreamType StreamType, const FTrackMetadata& TrackMetadata)
{
	WorkerThread.SendTrackSelectByMetadataMessage(StreamType, TrackMetadata);
}
#endif

void FAdaptiveStreamingPlayer::SelectTrackByAttributes(EStreamType StreamType, const FStreamSelectionAttributes& Attributes)
{
	WorkerThread.SendTrackSelectByAttributeMessage(StreamType, Attributes);
}


//-----------------------------------------------------------------------------
/**
 * Deselect track. The stream will continue to stream to allow for immediate selection/activation but no data will be fed to the decoder.
 *
 * @param StreamType
 */
void FAdaptiveStreamingPlayer::DeselectTrack(EStreamType StreamType)
{
	WorkerThread.SendTrackDeselectMessage(StreamType);
}

bool FAdaptiveStreamingPlayer::IsTrackDeselected(EStreamType StreamType)
{
	switch(StreamType)
	{
		case EStreamType::Video:
			return bIsVideoDeselected;
		case EStreamType::Audio:
			return bIsAudioDeselected;
		case EStreamType::Subtitle:
			return bIsTextDeselected;
		default:
			return true;
	}
}


//-----------------------------------------------------------------------------
/**
 * Starts the renderers.
 *
 * Call this once enough renderable data (both audio and video) is present.
 */
void FAdaptiveStreamingPlayer::StartRendering()
{
	RenderClock->Start();

	if (VideoRender.Renderer)
	{
		FParamDict noOptions;
		VideoRender.Renderer->StartRendering(noOptions);
	}

	if (AudioRender.Renderer)
	{
		FParamDict noOptions;
		AudioRender.Renderer->StartRendering(noOptions);
	}
}

//-----------------------------------------------------------------------------
/**
 * Stops renderers
 */
void FAdaptiveStreamingPlayer::StopRendering()
{
	RenderClock->Stop();

	if (VideoRender.Renderer)
	{
		FParamDict noOptions;
		VideoRender.Renderer->StopRendering(noOptions);
	}
	if (AudioRender.Renderer)
	{
		FParamDict noOptions;
		AudioRender.Renderer->StopRendering(noOptions);
	}
}

//-----------------------------------------------------------------------------
/**
 * Create the necessary AV renderers.
 *
 * @return
 */
int32 FAdaptiveStreamingPlayer::CreateRenderers()
{
	// Set the render clock with the renderes.
	VideoRender.Renderer->SetRenderClock(RenderClock);
	AudioRender.Renderer->SetRenderClock(RenderClock);

	// Hold back all frames during preroll or emit the first frame for scrubbing?
	if (PlayerOptions.HaveKey(OptionKeyDoNotHoldBackFirstVideoFrame))
	{
		VideoRender.Renderer->DisableHoldbackOfFirstRenderableVideoFrame(PlayerOptions.GetValue(OptionKeyDoNotHoldBackFirstVideoFrame).SafeGetBool(false));
	}

	return 0;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the renderers and decoders.
 */
void FAdaptiveStreamingPlayer::DestroyRenderers()
{
	// Decoders must be gone already
	check(!VideoDecoder.Decoder);
	check(!AudioDecoder.Decoder);
	AudioRender.Close();
	VideoRender.Close();
}





void FAdaptiveStreamingPlayer::PostLog(Facility::EFacility FromFacility, IInfoLog::ELevel InLogLevel, const FString &Message)
{
	int64 millisNow = SynchronizedUTCTime->GetTime().GetAsMilliseconds();
	FString msg = FString::Printf(TEXT("%s: %s"), Facility::GetName(FromFacility), *Message);
	DispatchEvent(FMetricEvent::ReportLogMessage(InLogLevel, msg, millisNow));
}

void FAdaptiveStreamingPlayer::PostError(const FErrorDetail& InError)
{
	TSharedPtrTS<FErrorDetail> Error(new FErrorDetail(InError));
	ErrorQueue.Push(Error);
}

void FAdaptiveStreamingPlayer::SendMessageToPlayer(TSharedPtrTS<IPlayerMessage> PlayerMessage)
{
	WorkerThread.SendPlayerSessionMessage(PlayerMessage);
}

//-----------------------------------------------------------------------------
/**
 * Returns the external GUID identifying this player and its associated external texture.
 */
void FAdaptiveStreamingPlayer::GetExternalGuid(FGuid& OutExternalGuid)
{
	OutExternalGuid = ExternalPlayerGUID;
}

ISynchronizedUTCTime* FAdaptiveStreamingPlayer::GetSynchronizedUTCTime()
{
	return SynchronizedUTCTime;
}

TSharedPtr<IAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe> FAdaptiveStreamingPlayer::GetStaticResourceProvider()
{
	return StaticResourceProvider.Pin();
}

TSharedPtrTS<IElectraHttpManager> FAdaptiveStreamingPlayer::GetHTTPManager()
{
	return HttpManager;
}

TSharedPtrTS<IAdaptiveStreamSelector> FAdaptiveStreamingPlayer::GetStreamSelector()
{
	return StreamSelector;
}

TSharedPtrTS<IPlaylistReader> FAdaptiveStreamingPlayer::GetManifestReader()
{
	return ManifestReader;
}

TSharedPtrTS<IPlayerEntityCache> FAdaptiveStreamingPlayer::GetEntityCache()
{
	return EntityCache;
}

TSharedPtrTS<IHTTPResponseCache> FAdaptiveStreamingPlayer::GetHTTPResponseCache()
{
	return HttpResponseCache;
}

IAdaptiveStreamingPlayerAEMSHandler* FAdaptiveStreamingPlayer::GetAEMSEventHandler()
{
	return AEMSEventHandler;
}

FParamDict& FAdaptiveStreamingPlayer::GetOptions()
{
	return PlayerOptions;
}

TSharedPtrTS<FDRMManager> FAdaptiveStreamingPlayer::GetDRMManager()
{
	return DrmManager;
}

void FAdaptiveStreamingPlayer::SetPlaybackEnd(const FTimeValue& InEndAtTime, IPlayerSessionServices::EPlayEndReason InEndingReason, TSharedPtrTS<IPlayerSessionServices::IPlayEndReason> InCustomManifestObject)
{
	WorkerThread.SendPlaybackEndMessage(InEndAtTime, InEndingReason, InCustomManifestObject);
}



//-----------------------------------------------------------------------------
/**
 * Notified when the given fragment will be opened.
 *
 * @param pRequest
 */
void FAdaptiveStreamingPlayer::OnFragmentOpen(TSharedPtrTS<IStreamSegment> pRequest)
{
	WorkerThread.Enqueue(FWorkerThreadMessages::FMessage::EType::FragmentOpen, pRequest);
}


//-----------------------------------------------------------------------------
/**
 * Fragment access unit received callback.
 *
 * NOTE: This must be done in place and cannot be deferred to a worker thread
 *       since we must return whether or not we could store the AU in our buffer right away.
 *
 * @param InAccessUnit
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::OnFragmentAccessUnitReceived(FAccessUnit* InAccessUnit)
{
	// When shutting down we don't accept anything new, but we return we did so the stream reader won't keep pummeling us with new data.
	if (bIsClosing)
	{
		return true;
	}
	TSharedPtrTS<FMultiTrackAccessUnitBuffer> Buffer = GetCurrentReceiveStreamBuffer(InAccessUnit->ESType);
	if (!Buffer.IsValid())
	{
		return true;
	}

	// Get the current buffer stats. Although they are updated regularly once per tick this would not reflect the current
	// state until the the next tick, which would allow the limits to be ignored as new access units arrive quickly.
	FAccessUnitBufferInfo bufferInfo;
	const FAccessUnitBuffer::FConfiguration* BufferConfig = nullptr;
	if (InAccessUnit->ESType == EStreamType::Video)
	{
		GetStreamBufferUtilization(bufferInfo, InAccessUnit->ESType);
		BufferConfig = &PlayerConfig.StreamBufferConfigVideo;
	}
	else if (InAccessUnit->ESType == EStreamType::Audio)
	{
		GetStreamBufferUtilization(bufferInfo, InAccessUnit->ESType);
		BufferConfig = &PlayerConfig.StreamBufferConfigAudio;
	}
	else if (InAccessUnit->ESType == EStreamType::Subtitle)
	{
		GetStreamBufferUtilization(bufferInfo, InAccessUnit->ESType);
		BufferConfig = &PlayerConfig.StreamBufferConfigText;
	}
	FAccessUnitBuffer::FExternalBufferInfo TotalUtilization;
	TotalUtilization.DataSize = bufferInfo.CurrentMemInUse;
	TotalUtilization.Duration = bufferInfo.PlayableDuration;

	// Try to push the data into the receiving buffer
	return Buffer->Push(InAccessUnit, BufferConfig, &TotalUtilization);
}

//-----------------------------------------------------------------------------
/**
 * Fragment reached end-of-stream callback.
 * No additional access units will be received for this fragment.
 *
 * @param InStreamType
 * @param InStreamSourceInfo
 */
void FAdaptiveStreamingPlayer::OnFragmentReachedEOS(EStreamType InStreamType, TSharedPtr<const FBufferSourceInfo, ESPMode::ThreadSafe> InStreamSourceInfo)
{
	if (!bIsClosing)
	{
		TSharedPtrTS<FMultiTrackAccessUnitBuffer> Buffer = GetCurrentReceiveStreamBuffer(InStreamType);
		if (Buffer.IsValid())
		{
			Buffer->PushEndOfDataFor(InStreamSourceInfo);
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Notified when the current fragment is closed.
 * This is called regardless of the error state and can always
 * be used as an indication that the fragment has finished processing.
 *
 * @param InRequest
 */
void FAdaptiveStreamingPlayer::OnFragmentClose(TSharedPtrTS<IStreamSegment> InRequest)
{
	WorkerThread.Enqueue(FWorkerThreadMessages::FMessage::EType::FragmentClose, InRequest);
}




void FAdaptiveStreamingPlayer::InternalHandleOnce()
{
	if (!bIsClosing)
	{
		// Update the play position we return to the interested caller.
		if (!PlaybackState.GetHasEnded())
		{
			if (RenderClock->IsRunning())
			{
				IMediaRenderClock::ERendererType ClockSource = IMediaRenderClock::ERendererType::Audio;
				if (DataAvailabilityStateAud.Availability == Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable)
				{
					ClockSource = IMediaRenderClock::ERendererType::Video;
				}
				FTimeValue playPos = RenderClock->GetInterpolatedRenderTime(ClockSource);
				PlaybackState.SetPlayPosition(ClampTimeToCurrentRange(playPos, false, true));
			}
		}
		else
		{
			// When playback has ended we lock the position to the end of the timeline.
			// This is only in case the application checks for the play position to reach the end of the timeline
			// ie. calling GetPlayPosition() to compare against the end of GetTimelineRange()
			// instead of using the dedicated HasEnded() method.
			PlaybackState.SetPlayPosition(ClampTimeToCurrentRange(FTimeValue::GetPositiveInfinity(), false, true));
		}

		// Handle state changes to match the user request.
		HandlePlayStateChanges();

		// Update diag buffers
		UpdateDiagnostics();
		// Handle seek requests.
		HandleSeeking();
		// Check for end of stream.
		CheckForStreamEnd();
		// Check the error queue for new arrivals.
		CheckForErrors();
		// Handle pending media segment requests.
		HandlePendingMediaSegmentRequests();
		// Handle changes in metadata, like timeline changes or track availability.
		HandleMetadataChanges();
		// Handle AEMS events
		HandleAEMSEvents();
		// Handle subtitle decoder
		HandleSubtitleDecoder();
		// Handle buffer level changes
		HandleNewBufferedData();
		// Handle new output data (finish prerolling)
		HandleNewOutputData();
		// Handle data buffers from deselected tracks to align with selected ones.
		HandleDeselectedBuffers();
		// Handle decoder changes
		HandleDecoderChanges();
		// Handle entity cache expirations.
		if (EntityCache.IsValid())
		{
			EntityCache->HandleEntityExpiration();
		}
		// Handle HTTP response cache expirations.
		if (HttpResponseCache.IsValid())
		{
			HttpResponseCache->HandleEntityExpiration();
		}
		// Handle completed DRM requests.
		if (DrmManager.IsValid())
		{
			DrmManager->Tick();
		}
		// Handle ABR.
		if (StreamSelector.IsValid())
		{
			IAdaptiveStreamSelector::EHandlingAction ABRAction = StreamSelector->PeriodicHandle();
			if (ABRAction == IAdaptiveStreamSelector::EHandlingAction::SeekToLive)
			{
				InternalStartoverAtLiveEdge();
			}
		}
	}
}

bool FAdaptiveStreamingPlayer::InternalHandleThreadMessages()
{
	bool bGotMessage = false;
	FWorkerThreadMessages::FMessage msg;
	while(WorkerThread.WorkMessages.Dequeue(msg))
	{
		// While closed ignore all messages.
		if (bIsClosing)
		{
			if (msg.Data.MediaEvent.Event)
			{
				msg.Data.MediaEvent.Event->Signal();
			}
			continue;
		}

		bGotMessage = true;
		// What is it?
		switch(msg.Type)
		{
			case FWorkerThreadMessages::FMessage::EType::Initialize:
			{
				FAdaptiveStreamingPlayer::InternalInitialize();
				break;
			}
			case FWorkerThreadMessages::FMessage::EType::LoadManifest:
			{
				InternalLoadManifest(msg.Data.ManifestToLoad.URL, msg.Data.ManifestToLoad.MimeType);
				break;
			}
			case FWorkerThreadMessages::FMessage::EType::Pause:
			{
				bShouldBePaused = true;
				bShouldBePlaying = false;
				break;
			}
			case FWorkerThreadMessages::FMessage::EType::Resume:
			{
				bShouldBePaused = false;
				bShouldBePlaying = true;
				break;
			}
			case FWorkerThreadMessages::FMessage::EType::Loop:
			{
				InternalSetLoop(msg.Data.Looping.Loop);
				if (msg.Data.Looping.Signal)
				{
					msg.Data.Looping.Signal->Signal();
				}
				break;
			}
			case FWorkerThreadMessages::FMessage::EType::Close:
			{
				if (!bIsClosing)
				{
					bIsClosing = true;
					InternalStop(false);
					InternalClose();
					DispatchEvent(FMetricEvent::ReportPlaybackStopped());
				}
				if (msg.Data.MediaEvent.Event)
				{
					msg.Data.MediaEvent.Event->Signal();
				}
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::ChangeBitrate:
			{
				BitrateCeiling = msg.Data.Bitrate.Value;
				StreamSelector->SetBandwidthCeiling(BitrateCeiling);
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::LimitResolution:
			{
				VideoResolutionLimitWidth  = msg.Data.Resolution.Width;
				VideoResolutionLimitHeight = msg.Data.Resolution.Height;
				UpdateStreamResolutionLimit();
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::InitialStreamAttributes:
			{
				// Map the language in case it is not yet.
				if (msg.Data.InitialStreamAttribute.InitialSelection.Language_ISO639.IsSet())
				{
					msg.Data.InitialStreamAttribute.InitialSelection.Language_ISO639 = ISO639::MapTo639_1(msg.Data.InitialStreamAttribute.InitialSelection.Language_ISO639.GetValue());
				}
				bool bInitiallyDisabled = msg.Data.InitialStreamAttribute.InitialSelection.OverrideIndex.IsSet() && msg.Data.InitialStreamAttribute.InitialSelection.OverrideIndex.GetValue() < 0;
				if (bInitiallyDisabled)
				{
					msg.Data.InitialStreamAttribute.InitialSelection.ClearOverrideIndex();
				}
				switch(msg.Data.InitialStreamAttribute.StreamType)
				{
					case EStreamType::Video:
						StreamSelectionAttributesVid = msg.Data.InitialStreamAttribute.InitialSelection;
						break;
					case EStreamType::Audio:
						StreamSelectionAttributesAud = msg.Data.InitialStreamAttribute.InitialSelection;
						break;
					case EStreamType::Subtitle:
						StreamSelectionAttributesTxt = msg.Data.InitialStreamAttribute.InitialSelection;
						break;
					default:
						break;
				}
				if (bInitiallyDisabled)
				{
					InternalDeselectStream(msg.Data.InitialStreamAttribute.StreamType);
				}
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::SelectTrackByAttributes:
			{
				if (!bIsClosing)
				{
					// Map the language in case it is not yet.
					if (msg.Data.TrackSelection.TrackAttributes.Language_ISO639.IsSet())
					{
						msg.Data.TrackSelection.TrackAttributes.Language_ISO639 = ISO639::MapTo639_1(msg.Data.TrackSelection.TrackAttributes.Language_ISO639.GetValue());
					}
					switch(msg.Data.TrackSelection.StreamType)
					{
						case EStreamType::Video:
							PendingTrackSelectionVid = MakeSharedTS<FStreamSelectionAttributes>(msg.Data.TrackSelection.TrackAttributes);
							bIsVideoDeselected = false;
							break;
						case EStreamType::Audio:
							PendingTrackSelectionAud = MakeSharedTS<FStreamSelectionAttributes>(msg.Data.TrackSelection.TrackAttributes);
							bIsAudioDeselected = false;
							break;
						case EStreamType::Subtitle:
							PendingTrackSelectionTxt = MakeSharedTS<FStreamSelectionAttributes>(msg.Data.TrackSelection.TrackAttributes);
							bIsTextDeselected = false;
							break;
						default:
							break;
					}
				}
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::SelectTrackByMetadata:
			{
				// Currently not used. May be used again later.
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::DeselectTrack:
			{
				InternalDeselectStream(msg.Data.TrackSelection.StreamType);
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::FragmentOpen:
			{
				TSharedPtrTS<IStreamSegment> pRequest = msg.Data.StreamReader.Request;
				EStreamType reqType = msg.Data.StreamReader.Request->GetType();
				// Check that the request is for this current playback sequence and not an outdated one.
				if (pRequest.IsValid() && pRequest->GetPlaybackSequenceID() == CurrentPlaybackSequenceID[StreamTypeToArrayIndex(reqType)])
				{
					DispatchBufferUtilizationEvent(msg.Data.StreamReader.Request->GetType());

					// Video bitrate change?
					if (msg.Data.StreamReader.Request->GetType() == EStreamType::Video)
					{
						int32 SegmentBitrate = pRequest->GetBitrate();
						int32 SegmentQualityLevel = pRequest->GetQualityIndex();
						if (SegmentBitrate != CurrentVideoStreamBitrate.Bitrate)
						{
							bool bDrastic = CurrentVideoStreamBitrate.Bitrate && SegmentQualityLevel < CurrentVideoStreamBitrate.QualityLevel-1;
							DispatchEvent(FMetricEvent::ReportVideoQualityChange(SegmentBitrate, CurrentVideoStreamBitrate.Bitrate, bDrastic));
							CurrentVideoStreamBitrate.Bitrate      = SegmentBitrate;
							CurrentVideoStreamBitrate.QualityLevel = SegmentQualityLevel;
						}
					}
				}
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::FragmentClose:
			{
				TSharedPtrTS<IStreamSegment> pRequest = msg.Data.StreamReader.Request;
				EStreamType reqType = msg.Data.StreamReader.Request->GetType();
				// Check that the request is for this current playback sequence and not an outdated one.
				if (pRequest.IsValid() && pRequest->GetPlaybackSequenceID() == CurrentPlaybackSequenceID[StreamTypeToArrayIndex(reqType)])
				{
					// Dispatch download event
					DispatchSegmentDownloadedEvent(pRequest);

					// Dispatch buffer utilization
					DispatchBufferUtilizationEvent(reqType);

					// Dispatch average throughput, bandwidth and latency event for video segments only.
					if (reqType == EStreamType::Video)
					{
						DispatchEvent(FMetricEvent::ReportBandwidth(StreamSelector->GetAverageBandwidth(), StreamSelector->GetAverageThroughput(), StreamSelector->GetAverageLatency()));
					}

					// Add to the list of completed requests for which we need to find the next or retry segment to fetch.
					FPendingSegmentRequest NextReq;
					NextReq.Request = pRequest;
					NextPendingSegmentRequests.Enqueue(MoveTemp(NextReq));
				}
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::BufferUnderrun:
			{
				InternalRebuffer();
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::PlayerSession:
			{
				HandleSessionMessage(msg.Data.Session.PlayerMessage);
				break;
			}

			case FWorkerThreadMessages::FMessage::EType::EndPlaybackAt:
			{
				// With a defined end time we just play out until then.
				PlaybackState.SetShouldPlayOnLiveEdge(false);
				PlaybackState.SetPlaybackEndAtTime(msg.Data.EndPlaybackAt.EndAtTime);
				break;
			}

			default:
			{
				checkNoEntry();
				break;
			}
		}

		InternalHandleOnce();
	}
	return bGotMessage;
}

//-----------------------------------------------------------------------------
/**
 * Handling function called by the shared worker thread.
 */
void FAdaptiveStreamingPlayer::HandleOnce()
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AdaptiveWorker);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, AdaptiveStreamingPlayer_Worker);

	if (!InternalHandleThreadMessages())
	{
		InternalHandleOnce();
	}
	// Handle the components that are bound to add new worker messages.
	// This should not be done while processing the messages that were enqueued earlier,
	// especially not since handling those will result in handling all state checks
	// in InternalHandleOnce() again!
	InternalHandleManifestReader();
}



void FAdaptiveStreamingPlayer::HandlePlayStateChanges()
{
	if (CurrentState == EPlayerState::eState_Error)
	{
		return;
	}

	// Should the player be paused?
	if (bShouldBePaused)
	{
		// Are we not paused and in a state in which we can pause?
		if (CurrentState == EPlayerState::eState_Playing)
		{
			PlaybackState.SetShouldPlayOnLiveEdge(false);
			InternalPause();
		}
	}

	// Should the player be playing?
	if (bShouldBePlaying)
	{
		// Are we paused and in a state in which we can play?
		if (CurrentState == EPlayerState::eState_Paused)
		{
			InternalResume();
		}
	}

	// Did the play range or loop state change such that we need to start over?
	if (PlaybackState.GetPlayRangeHasChanged() || PlaybackState.GetLoopStateHasChanged())
	{
		// We do not do this when paused as setting the range usually happens only when paused for seeking or scrubbing.
		if (CurrentState == EPlayerState::eState_Playing)
		{
			InternalStartoverAtCurrentPosition();
		}
	}

	// Update the current live latency.
	PlaybackState.SetCurrentLiveLatency(CalculateCurrentLiveLatency(false));
}


void FAdaptiveStreamingPlayer::HandleSeeking()
{
	// Are we in a state where a seek is possible?
	if (CurrentState == EPlayerState::eState_Idle || CurrentState == EPlayerState::eState_ParsingManifest || CurrentState == EPlayerState::eState_PreparingStreams || CurrentState == EPlayerState::eState_Error ||
		!Manifest.IsValid())
	{
		// No, return.
		return;
	}

	// When playing the last successfully seeked position is irrelevant and cannot be used as a reference any more.
	if (PlaybackState.GetIsPlaying())
	{
		SeekVars.InvalidateLastFinished();
	}

	FScopeLock lock(&SeekVars.Lock);
	// Is there a pending request?
	if (SeekVars.PendingRequest.IsSet())
	{
		// If there is an active request and the new request is for scrubbing we let the active request finish first.
		bool bIsForScrubbing = SeekVars.PendingRequest.GetValue().bOptimizeForScrubbing.Get(PlayerOptions.GetValue(OptionKeyFrameOptimizeSeekForScrubbing).SafeGetBool(false));
		if (SeekVars.ActiveRequest.IsSet() && bIsForScrubbing)
		{
			return;
		}

		// Check the distance to the last seek performed, if there is one.
		if (SeekVars.LastFinishedRequest.IsSet() && SeekVars.PendingRequest.GetValue().DistanceThreshold.IsSet())
		{
			double Distance = Utils::AbsoluteValue(SeekVars.LastFinishedRequest.GetValue().Time.GetAsSeconds() - SeekVars.PendingRequest.GetValue().Time.GetAsSeconds());
			if (Distance <= SeekVars.PendingRequest.GetValue().DistanceThreshold.Get(0.0))
			{
				// Already there
				SeekVars.PendingRequest.Reset();
				DispatchEvent(FMetricEvent::ReportSeekCompleted(true));
				return;
			}
		}

		// Not playing on the Live edge. This may get set to true later on handling the start segment.
		PlaybackState.SetShouldPlayOnLiveEdge(false);
		// Trigger the seek.
		FSeekParam SeekParam = SeekVars.PendingRequest.GetValue();
		SeekVars.PendingRequest.Reset();
		SeekVars.ClearWorkVars();

		// On a user-induced seek the sequence index is supposed to reset to 0.
		CurrentPlaybackSequenceState.SequenceIndex = 0;
		// And since it is a seek on purpose the loop counter is reset as well.
		FInternalLoopState LoopStateNow;
		PlaybackState.GetLoopState(LoopStateNow);
		LoopStateNow.Count = 0;
		PlaybackState.SetLoopState(LoopStateNow);
		CurrentLoopState.Count = 0;

		if (SeekVars.bIsPlayStart)
		{
			CurrentState = EPlayerState::eState_Buffering;
		}
		else
		{
			// Stop everything.
			InternalStop(PlayerConfig.bHoldLastFrameDuringSeek);
			if (Manifest.IsValid())
			{
				Manifest->UpdateDynamicRefetchCounter();
			}
			CurrentState = EPlayerState::eState_Seeking;

			SeekVars.bForScrubbing = bIsForScrubbing;
		}
		SeekVars.ActiveRequest = SeekParam;
		InternalStartAt(SeekVars.ActiveRequest.GetValue());
	}
}



//-----------------------------------------------------------------------------
/**
 * Handle changes in metadata, like timeline changes or track availability.
 */
void FAdaptiveStreamingPlayer::HandleMetadataChanges()
{
	// The timeline can change dynamically. Refresh it on occasion.
	if (Manifest.IsValid() && ManifestType == EMediaFormatType::DASH)
	{
		PlaybackState.SetSeekableRange(Manifest->GetSeekableTimeRange());
		PlaybackState.SetTimelineRange(Manifest->GetTotalTimeRange());
		PlaybackState.SetDuration(Manifest->GetDuration());
	}

	FTimeValue CurrentTime = PlaybackState.GetPlayPosition();

	// See if we may be reaching the next period.
	if (UpcomingPeriods.Num() && CurrentTime.IsValid() && CurrentTime != MetadataHandlingState.LastHandlingTime)
	{
		// Locate the period the position falls into.
		for(int32 i=0; i<UpcomingPeriods.Num(); ++i)
		{
			if (UpcomingPeriods[i].TimeRange.Contains(CurrentTime))
			{
				if (!UpcomingPeriods[i].ID.Equals(MetadataHandlingState.LastSentPeriodID))
				{
					MetadataHandlingState.LastSentPeriodID = UpcomingPeriods[i].ID;

					TArray<FTrackMetadata> MetadataVideo;
					TArray<FTrackMetadata> MetadataAudio;
					TArray<FTrackMetadata> MetadataSubtitle;
					UpcomingPeriods[i].Period->GetMetaData(MetadataVideo, EStreamType::Video);
					UpcomingPeriods[i].Period->GetMetaData(MetadataAudio, EStreamType::Audio);
					UpcomingPeriods[i].Period->GetMetaData(MetadataSubtitle, EStreamType::Subtitle);

					bool bMetadataChanged = PlaybackState.SetTrackMetadata(MetadataVideo, MetadataAudio, MetadataSubtitle);
					PlaybackState.SetHaveMetadata(true);
					if (bMetadataChanged)
					{
						DispatchEvent(FMetricEvent::ReportTracksChanged());
					}

					// Update the data availability for this period.
					UpdateDataAvailabilityState(DataAvailabilityStateVid, MetadataVideo.Num() ? Metrics::FDataAvailabilityChange::EAvailability::DataAvailable : Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
					UpdateDataAvailabilityState(DataAvailabilityStateAud, MetadataAudio.Num() ? Metrics::FDataAvailabilityChange::EAvailability::DataAvailable : Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
					UpdateDataAvailabilityState(DataAvailabilityStateTxt, MetadataSubtitle.Num() ? Metrics::FDataAvailabilityChange::EAvailability::DataAvailable : Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);

					// Inform AEMS handler of the period transition.
					AEMSEventHandler->PlaybackPeriodTransition(UpcomingPeriods[i].ID, UpcomingPeriods[i].TimeRange);
				}
				break;
			}
		}

		// Remove the periods that we have passed for which the loop counter is not in the future.
		// With looping playback the periods to loop back to will be added/updated and must not be removed
		// if they are used in a future loop cycle.
		for(int32 i=0; i<UpcomingPeriods.Num(); ++i)
		{
			if ((UpcomingPeriods[i].TimeRange.End.IsValid() && CurrentTime >= UpcomingPeriods[i].TimeRange.End && UpcomingPeriods[i].LoopCount <= CurrentTime.GetSequenceIndex()) ||
				(UpcomingPeriods[i].TimeRange.Start.IsValid() && CurrentTime < UpcomingPeriods[i].TimeRange.Start && UpcomingPeriods[i].LoopCount < CurrentTime.GetSequenceIndex()))
			{
				UpcomingPeriods.RemoveAt(i);
				--i;
			}
		}

		MetadataHandlingState.LastHandlingTime = CurrentTime;
	}

	// Dump periods that are no longer active because we passed their end with some threshold
	// and whose loop count is also not expired.
	if (CurrentTime.IsValid())
	{
		FTimeValue PastTime = CurrentTime - FTimeValue().SetFromMilliseconds(1000);
		FScopeLock Lock(&ActivePeriodCriticalSection);
		for(int32 i=0; i<ActivePeriods.Num(); ++i)
		{
			if (((ActivePeriods[i].TimeRange.End.IsValid() && PastTime > ActivePeriods[i].TimeRange.End) ||
				(!ActivePeriods[i].TimeRange.End.IsValid() && (i+1) < ActivePeriods.Num() && PastTime >= ActivePeriods[i+1].TimeRange.Start)) &&
				ActivePeriods[i].LoopCount <= CurrentTime.GetSequenceIndex())
			{
				// Do not remove the only active period.
				if (ActivePeriods.Num() > 1)
				{
					ActivePeriods.RemoveAt(i);
					--i;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Handles Application Event or Metadata Stream (AEMS) events triggering on current playback position.
 *
 * @param SessionMessage
 */
void FAdaptiveStreamingPlayer::HandleAEMSEvents()
{
	if (CurrentState == EPlayerState::eState_Playing)
	{
		FTimeValue Current = PlaybackState.GetPlayPosition();
		AEMSEventHandler->ExecutePendingEvents(Current);
	}
}


//-----------------------------------------------------------------------------
/**
 * Handles a player session message sent by one of the player sub-systems.
 *
 * @param SessionMessage
 */
void FAdaptiveStreamingPlayer::HandleSessionMessage(TSharedPtrTS<IPlayerMessage> SessionMessage)
{
	// Playlist downloaded (successful or not)?
	if (SessionMessage->GetType() == IPlaylistReader::PlaylistDownloadMessage::Type())
	{
		IPlaylistReader::PlaylistDownloadMessage* pMsg = static_cast<IPlaylistReader::PlaylistDownloadMessage *>(SessionMessage.Get());
		Metrics::FPlaylistDownloadStats stats;
		stats.bWasSuccessful = !pMsg->GetConnectionInfo().StatusInfo.ErrorDetail.IsSet();
		stats.ListType  	 = pMsg->GetListType();
		stats.LoadType  	 = pMsg->GetLoadType();
		stats.URL   		 = pMsg->GetConnectionInfo().EffectiveURL;
		stats.FailureReason  = pMsg->GetConnectionInfo().StatusInfo.ErrorDetail.GetMessage();
		stats.HTTPStatusCode = pMsg->GetConnectionInfo().StatusInfo.HTTPStatus;
		stats.RetryNumber    = pMsg->GetConnectionInfo().RetryInfo.IsValid() ? pMsg->GetConnectionInfo().RetryInfo->AttemptNumber : 0;
		DispatchEvent(FMetricEvent::ReportPlaylistDownload(stats));
	}
	// Playlist fetched & parsed?
	else if (SessionMessage->GetType() == IPlaylistReader::PlaylistLoadedMessage::Type())
	{
		if (ManifestReader.IsValid())
		{
			IPlaylistReader::PlaylistLoadedMessage* pMsg = static_cast<IPlaylistReader::PlaylistLoadedMessage *>(SessionMessage.Get());
			const FErrorDetail& Result = pMsg->GetResult();
			// Note: Right now only successful messages should get here. We check for failure anyway in case this changes in the future.
			if (!Result.IsError())
			{
				if (!Result.WasAborted())
				{
					if (pMsg->GetListType() == Playlist::EListType::Master && pMsg->GetLoadType() == Playlist::ELoadType::Initial)
					{
						DispatchEvent(FMetricEvent::ReportReceivedMasterPlaylist(pMsg->GetConnectionInfo().EffectiveURL));
					}
					else if (pMsg->GetLoadType() == Playlist::ELoadType::Initial)
					{
						SelectManifest();
						DispatchEvent(FMetricEvent::ReportReceivedPlaylists());
					}
					else
					{
						UpdateManifest();
					}
				}
			}
			else
			{
				PostError(Result);
			}
		}
	}
	// License key?
	else if (SessionMessage->GetType() == FLicenseKeyMessage::Type())
	{
		FLicenseKeyMessage* pMsg = static_cast<FLicenseKeyMessage*>(SessionMessage.Get());
		Metrics::FLicenseKeyStats stats;
		stats.bWasSuccessful = !pMsg->GetResult().IsError();
		stats.URL   		 = pMsg->GetConnectionInfo().EffectiveURL;
		stats.FailureReason  = pMsg->GetResult().IsError() ? pMsg->GetResult().GetPrintable() : FString(); //pMsg->GetConnectionInfo().StatusInfo.ErrorDetail.GetMessage();
		DispatchEvent(FMetricEvent::ReportLicenseKey(stats));
	}
	// Decoder message?
	else if (SessionMessage->GetType() == FDecoderMessage::Type())
	{
		FDecoderMessage* pMsg = static_cast<FDecoderMessage*>(SessionMessage.Get());
		if (pMsg->GetStreamType() == EStreamType::Video)
		{
			VideoDecoder.bDrainingForCodecChangeDone = true;
		}
	}
	else
	{
		checkNoEntry();
	}
}




FTimeValue FAdaptiveStreamingPlayer::CalculateCurrentLiveLatency(bool bViaLatencyElement)
{
	FTimeValue LiveLatency;
	if (Manifest.IsValid())
	{
		if (Manifest->GetPresentationType() != IManifest::EType::OnDemand)
		{
			FTimeValue UTCNow = GetSynchronizedUTCTime()->GetTime();
			FTimeValue PlayPosNow = PlaybackState.GetPlayPosition();
			LiveLatency = UTCNow - PlayPosNow;

			if (bViaLatencyElement)
			{
				TSharedPtrTS<const FLowLatencyDescriptor> llDesc = Manifest->GetLowLatencyDescriptor();
				if (llDesc.IsValid())
				{
					// Low latency Live
					TSharedPtrTS<IProducerReferenceTimeInfo> ProdRefTime = Manifest->GetProducerReferenceTimeInfo(llDesc->Latency.ReferenceID);
					if (ProdRefTime.IsValid())
					{
						FTimeValue EncoderLatency = PlaybackState.GetEncoderLatency();
						if (EncoderLatency.IsValid())
						{
							LiveLatency += EncoderLatency;
						}
					}
				}
			}
		}
	}
	return LiveLatency;
}


//-----------------------------------------------------------------------------
/**
 * Returns the required duration to be available in the buffer before playback can begin.
 */
double FAdaptiveStreamingPlayer::GetMinBufferTimeBeforePlayback()
{
	// Yes. Check if we have enough data buffered up to begin handing off data to the decoders.
	double kMinBufferBeforePlayback = LastBufferingState == EPlayerState::eState_Seeking ? PlayerConfig.SeekBufferMinTimeAvailBeforePlayback :
										LastBufferingState == EPlayerState::eState_Rebuffering ? PlayerConfig.RebufferMinTimeAvailBeforePlayback : PlayerConfig.InitialBufferMinTimeAvailBeforePlayback;

	FTimeValue mbtAbr = StreamSelector.IsValid() ? StreamSelector->GetMinBufferTimeForPlayback(LastBufferingState == EPlayerState::eState_Seeking ? IAdaptiveStreamSelector::EMinBufferType::Seeking
																							 : LastBufferingState == EPlayerState::eState_Rebuffering ? IAdaptiveStreamSelector::EMinBufferType::Rebuffering 
																							 :	IAdaptiveStreamSelector::EMinBufferType::Initial, Manifest->GetMinBufferTime()) : FTimeValue();
	kMinBufferBeforePlayback = mbtAbr.IsValid() ? mbtAbr.GetAsSeconds() : kMinBufferBeforePlayback;
	return kMinBufferBeforePlayback;
}

bool FAdaptiveStreamingPlayer::IsExpectedToStreamNow(EStreamType InType)
{
	if (!StreamReaderHandler)
	{
		return false;
	}
	if (InType == EStreamType::Video)
	{
		return CurrentPlayPeriodVideo.IsValid() ? CurrentPlayPeriodVideo->GetSelectedStreamBufferSourceInfo(InType).IsValid() : false;
	}
	else if (InType == EStreamType::Audio)
	{
		return CurrentPlayPeriodAudio.IsValid() ? CurrentPlayPeriodAudio->GetSelectedStreamBufferSourceInfo(InType).IsValid() : false;
	}
	else if (InType == EStreamType::Subtitle)
	{
		return CurrentPlayPeriodText.IsValid() ? CurrentPlayPeriodText->GetSelectedStreamBufferSourceInfo(InType).IsValid() : false;
	}
	return false;
}


bool FAdaptiveStreamingPlayer::HaveEnoughBufferedDataToStartPlayback()
{
	double kMinBufferBeforePlayback = GetMinBufferTimeBeforePlayback();

	DiagnosticsCriticalSection.Lock();

	bool bHaveEnoughVideo = true;
	if (IsExpectedToStreamNow(EStreamType::Video))
	{
		bHaveEnoughVideo = false;
		// If the video stream has reached the end then there won't be any new data and whatever we have is all there is.
		if (VideoBufferStats.StreamBuffer.bEndOfData)
		{
			bHaveEnoughVideo = true;
		}
		else
		{
			// Does the buffer have the requested amount of data?
			if (VideoBufferStats.StreamBuffer.PlayableDuration.GetAsSeconds() >= kMinBufferBeforePlayback)
			{
				bHaveEnoughVideo = true;
			}
			else
			{
				// There is a chance that the input stream buffer is too small to hold the requested amount of material.
				if (VideoBufferStats.StreamBuffer.bLastPushWasBlocked)
				{
					// We won't be able to receive any more, so we have enough as it is.
					bHaveEnoughVideo = true;
				}
			}
		}
	}

	bool bHaveEnoughAudio = true;
	if (IsExpectedToStreamNow(EStreamType::Audio))
	{
		bHaveEnoughAudio = false;
		// If the audio stream has reached the end then there won't be any new data and whatever we have is all there is.
		if (AudioBufferStats.StreamBuffer.bEndOfData)
		{
			bHaveEnoughAudio = true;
		}
		else
		{
			// Does the buffer have the requested amount of data?
			if (AudioBufferStats.StreamBuffer.PlayableDuration.GetAsSeconds() >= kMinBufferBeforePlayback)
			{
				bHaveEnoughAudio = true;
			}
			else
			{
				// There is a chance that the input stream buffer is too small to hold the requested amount of material.
				if (AudioBufferStats.StreamBuffer.bLastPushWasBlocked)
				{
					// We won't be able to receive any more, so we have enough as it is.
					bHaveEnoughAudio = true;
				}
			}
		}
	}

	// When we are dealing with a single multiplexed stream and one buffer is blocked then essentially all buffers
	// must be considered blocked since demuxing cannot continue.
	// FIXME: do this more elegant somehow
	if (ManifestType == EMediaFormatType::ISOBMFF)
	{
		if (VideoBufferStats.StreamBuffer.bLastPushWasBlocked ||
			AudioBufferStats.StreamBuffer.bLastPushWasBlocked ||
			TextBufferStats.StreamBuffer.bLastPushWasBlocked)
		{
			bHaveEnoughVideo = bHaveEnoughAudio = true;
		}
	}

	DiagnosticsCriticalSection.Unlock();

	return bHaveEnoughVideo && bHaveEnoughAudio;
}


void FAdaptiveStreamingPlayer::PrepareForPrerolling()
{
	PrerollVars.StartTime = MEDIAutcTime::CurrentMSec();

	// We need to check if there is any data available in the buffers that can be decoded.
	// It may be possible that playback was starting on the last segment that did not bring in any playable
	// data or none at all if it was past end of stream.
	DiagnosticsCriticalSection.Lock();

	// Check video buffer
	if (IsExpectedToStreamNow(EStreamType::Video))
	{
		if (VideoBufferStats.StreamBuffer.bEndOfData && (VideoBufferStats.StreamBuffer.PlayableDuration == FTimeValue::GetZero() || VideoBufferStats.StreamBuffer.NumCurrentAccessUnits == 0))
		{
			PrerollVars.bHaveEnoughVideo = true;
			VideoBufferStats.DecoderInputBuffer.bEODSignaled = true;
			VideoBufferStats.DecoderInputBuffer.bEODReached = true;
			VideoBufferStats.DecoderOutputBuffer.bEODreached = true;
		}
	}

	// Check audio buffer
	if (IsExpectedToStreamNow(EStreamType::Audio))
	{
		if (AudioBufferStats.StreamBuffer.bEndOfData && (AudioBufferStats.StreamBuffer.PlayableDuration == FTimeValue::GetZero() || AudioBufferStats.StreamBuffer.NumCurrentAccessUnits == 0))
		{
			PrerollVars.bHaveEnoughAudio = true;
			AudioBufferStats.DecoderInputBuffer.bEODSignaled = true;
			AudioBufferStats.DecoderInputBuffer.bEODReached = true;
			AudioBufferStats.DecoderOutputBuffer.bEODreached = true;
		}
	}

	DiagnosticsCriticalSection.Unlock();
}


//-----------------------------------------------------------------------------
/**
 * Checks if buffers have enough data to advance the play state.
 */
void FAdaptiveStreamingPlayer::HandleNewBufferedData()
{
	// Are we in buffering state? (this includes rebuffering)
	if (bStreamingHasStarted && CurrentState == EPlayerState::eState_Buffering && DecoderState == EDecoderState::eDecoder_Paused)
	{
		if (!SeekVars.bForScrubbing)
		{
			check(LastBufferingState == EPlayerState::eState_Buffering || LastBufferingState == EPlayerState::eState_Rebuffering || LastBufferingState == EPlayerState::eState_Seeking);
			// Check if we have enough data buffered up to begin handing off data to the decoders.
			bool bHaveEnoughData = HaveEnoughBufferedDataToStartPlayback();
			if (bHaveEnoughData)
			{
				PlaybackState.SetIsBuffering(false);
				PlaybackState.SetIsSeeking(false);

				// Activate the stream buffers from which to feed the decoders.
				DataBuffersCriticalSection.Lock();
				if (!ActiveDataOutputBuffers.IsValid())
				{
					TSharedPtrTS<FStreamDataBuffers> NewOutputBuffers;
					check(NextDataBuffers.Num());
					if (NextDataBuffers.Num())
					{
						NewOutputBuffers = NextDataBuffers[0];
						NextDataBuffers.RemoveAt(0);
					}
					ActiveDataOutputBuffers = MoveTemp(NewOutputBuffers);
				}
				DataBuffersCriticalSection.Unlock();

				PrepareForPrerolling();
				PipelineState = EPipelineState::ePipeline_Prerolling;
				DecoderState = EDecoderState::eDecoder_Running;

				PostrollVars.Clear();

				// Send buffering end event
				DispatchBufferingEvent(false, LastBufferingState);
				// Send pre-roll begin event.
				DispatchEvent(FMetricEvent::ReportPrerollStart());
			}
		}
		else
		{
			// Optimized seek for scrubbing is not possible at playstart so we can skip a few things here.
			if (!SeekVars.bScrubPrerollDone)
			{
				DataBuffersCriticalSection.Lock();
				if (!ActiveDataOutputBuffers.IsValid())
				{
					TSharedPtrTS<FStreamDataBuffers> NewOutputBuffers;
					if (NextDataBuffers.Num())
					{
						NewOutputBuffers = NextDataBuffers[0];
						NextDataBuffers.RemoveAt(0);
					}
					ActiveDataOutputBuffers = MoveTemp(NewOutputBuffers);
				}
				bool bHaveActiveOutputBuffer = ActiveDataOutputBuffers.IsValid();
				DataBuffersCriticalSection.Unlock();
				if (bHaveActiveOutputBuffer)
				{
					PrepareForPrerolling();
					PipelineState = EPipelineState::ePipeline_Prerolling;
					DecoderState = EDecoderState::eDecoder_Running;
					PostrollVars.Clear();
					DispatchEvent(FMetricEvent::ReportPrerollStart());
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Handle data in deselected buffers.
 * Since deselected buffers do not feed a decoder the data must be discarded as playback progresses
 * to avoid buffer overflows.
 */
void FAdaptiveStreamingPlayer::HandleDeselectedBuffers()
{
	if (bIsVideoDeselected || bIsAudioDeselected || bIsTextDeselected)
	{
		// Get the current playback position in case nothing is selected.
		FTimeValue DiscardUntilTime = PlaybackState.GetPlayPosition();

		TSharedPtrTS<FMultiTrackAccessUnitBuffer> VidOutBuffer = GetCurrentOutputStreamBuffer(EStreamType::Video);
		TSharedPtrTS<FMultiTrackAccessUnitBuffer> AudOutBuffer = GetCurrentOutputStreamBuffer(EStreamType::Audio);
		TSharedPtrTS<FMultiTrackAccessUnitBuffer> TxtOutBuffer = GetCurrentOutputStreamBuffer(EStreamType::Subtitle);

		// Get last popped video PTS if video track is selected.
		if (!bIsVideoDeselected && VidOutBuffer.IsValid())
		{
			FTimeValue v = VidOutBuffer->GetLastPoppedPTS();
			if (v.IsValid())
			{
				DiscardUntilTime = v;
			}
		}
		// Get last popped audio PTS if audio track is selected.
		if (!bIsAudioDeselected && AudOutBuffer.IsValid())
		{
			FTimeValue v = AudOutBuffer->GetLastPoppedPTS();
			if (v.IsValid())
			{
				DiscardUntilTime = v;
			}
		}
		// FIXME: do we need to consider subtitle tracks as being the only selected ones??

		if (DiscardUntilTime.IsValid())
		{
			if (bIsVideoDeselected && VidOutBuffer.IsValid())
			{
				FMultiTrackAccessUnitBuffer::FScopedLock lock(VidOutBuffer);
				VidOutBuffer->PopDiscardUntil(DiscardUntilTime);
			}
			if (bIsAudioDeselected && AudOutBuffer.IsValid())
			{
				FMultiTrackAccessUnitBuffer::FScopedLock lock(AudOutBuffer);
				AudOutBuffer->PopDiscardUntil(DiscardUntilTime);
			}
			if (bIsTextDeselected && TxtOutBuffer.IsValid())
			{
				FMultiTrackAccessUnitBuffer::FScopedLock lock(TxtOutBuffer);
				TxtOutBuffer->PopDiscardUntil(DiscardUntilTime);
			}
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Checks if the player state can transition to the next state with available decoder output.
 */
void FAdaptiveStreamingPlayer::HandleNewOutputData()
{
	int64 timeNow = MEDIAutcTime::CurrentMSec();

	// Are we currently pre-rolling the media pipeline?
	if (PipelineState == EPipelineState::ePipeline_Prerolling)
	{
		// Yes. Is it "warm" enough to start rendering?
		DiagnosticsCriticalSection.Lock();

		// Check video
		bool bHaveEnoughVideo = PrerollVars.bHaveEnoughVideo;
		if (IsExpectedToStreamNow(EStreamType::Video))
		{
			// If the video decoder output buffer is stalled then we have enough video output. There won't be any more coming in.
			// Also when the stream has reached the end and that has propagated through the decoder's buffer.
			if (VideoBufferStats.DecoderOutputBuffer.bOutputStalled ||
				(VideoBufferStats.StreamBuffer.bEndOfData && VideoBufferStats.DecoderInputBuffer.bEODSignaled && VideoBufferStats.DecoderInputBuffer.bEODReached) ||
				bIsVideoDeselected)
			{
				bHaveEnoughVideo = true;
			}
		}
		else
		{
			bHaveEnoughVideo = true;
		}

		// Check audio
		bool bHaveEnoughAudio = PrerollVars.bHaveEnoughAudio;
		if (IsExpectedToStreamNow(EStreamType::Audio))
		{
			// If the audio decoder output buffer is stalled then we have enough video output. There won't be any more coming in.
			// Also when the stream has reached the end and that has propagated through the decoder's buffer.
			if (AudioBufferStats.DecoderOutputBuffer.bOutputStalled ||
				(AudioBufferStats.StreamBuffer.bEndOfData && AudioBufferStats.DecoderInputBuffer.bEODSignaled && AudioBufferStats.DecoderInputBuffer.bEODReached) ||
				bIsAudioDeselected)
			{
				bHaveEnoughAudio = true;
			}
		}
		else
		{
			bHaveEnoughAudio = true;
		}

		DiagnosticsCriticalSection.Unlock();

		// Keep current decision around to prime the next check.
		if (bHaveEnoughVideo)
		{
			PrerollVars.bHaveEnoughVideo = true;
		}
		if (bHaveEnoughAudio)
		{
			PrerollVars.bHaveEnoughAudio = true;
		}

		// Ready to go?
		if (bHaveEnoughVideo && bHaveEnoughAudio)
		{
			if (!SeekVars.bForScrubbing)
			{
				if (!PrerollVars.bIsMidSequencePreroll)
				{
					PipelineState = EPipelineState::ePipeline_Stopped;
					CurrentState = EPlayerState::eState_Paused;
					PlaybackRate = 0.0;
					PrerollVars.Clear();
					SeekVars.ClearWorkVars();
					SeekVars.SetFinished();
					if (LastBufferingState == EPlayerState::eState_Seeking)
					{
						DispatchEvent(FMetricEvent::ReportSeekCompleted());
					}
					DispatchEvent(FMetricEvent::ReportPrerollEnd());
				}
				else
				{
					PrerollVars.Clear();
					StartRendering();
					PipelineState = EPipelineState::ePipeline_Running;
				}
			}
			else
			{
				// The first time around we send the seek complete event and stop the feeding of the decoders
				// to prevent them consuming and sending output to the renderers while we await the buffers
				// to fill to the amount we need.
				if (!SeekVars.bScrubPrerollDone)
				{
					SeekVars.bScrubPrerollDone = true;
					SeekVars.SetFinished();
					DispatchEvent(FMetricEvent::ReportSeekCompleted());
					// Pause feeding the decoder to stop draining buffers we want to fill.
					DecoderState = EDecoderState::eDecoder_Paused;
				}

				if (HaveEnoughBufferedDataToStartPlayback())
				{
					PlaybackState.SetIsBuffering(false);
					PlaybackState.SetIsSeeking(false);

					SeekVars.ClearWorkVars();
					PipelineState = EPipelineState::ePipeline_Stopped;
					CurrentState = EPlayerState::eState_Paused;
					DecoderState = EDecoderState::eDecoder_Running;
					PlaybackRate = 0.0;
					PrerollVars.Clear();
					DispatchBufferingEvent(false, LastBufferingState);
					DispatchEvent(FMetricEvent::ReportPrerollEnd());
				}
			}
		}
	}
}



void FAdaptiveStreamingPlayer::InternalHandlePendingStartRequest(const FTimeValue& CurrentTime)
{
	// Is there a start request?
	if (PendingStartRequest.IsValid())
	{
		FPlayStartPosition StartAt = PendingStartRequest->StartAt;

		// Need to have a manifest.
		if (Manifest.IsValid())
		{
			if (PendingStartRequest->RetryAtTime.IsValid() && CurrentTime < PendingStartRequest->RetryAtTime)
			{
				return;
			}

			// For sanities sake disable any loop flag that might have been set before loading the playlist if the presentation isn't VoD.
			if (Manifest->GetPresentationType() != IManifest::EType::OnDemand)
			{
				CurrentLoopParam.bEnableLooping = false;
			}

			// If the starting time has not been set we now check if we are dealing with a VoD or a Live stream and choose a starting point.
			if (!StartAt.Time.IsValid())
			{
				// Use the presentation provided start time, if it has one.
				StartAt.Time = Manifest->GetDefaultStartTime();
				if (!StartAt.Time.IsValid())
				{
					FTimeRange Seekable = Manifest->GetSeekableTimeRange();
					if (Manifest->GetPresentationType() == IManifest::EType::OnDemand)
					{
						check(Seekable.Start.IsValid());
						StartAt.Time = Seekable.Start;
					}
					else
					{
						/*
							We need to wait for the timeline to become valid. It may not be yet if the presentation
							is scheduled to start in the future or if the intended distance to the Live edge can not
							be maintained yet when the presentation has just begun.
						*/
						if (Seekable.End - Seekable.Start <= FTimeValue::GetZero())
						{
							return;
						}

						check(Seekable.End.IsValid());
						StartAt.Time = Seekable.End;
						// DASH Live streams provide the means to join at an exact wallclock time, so enable this.
						if (ManifestType == EMediaFormatType::DASH)
						{
							PlaybackState.SetShouldPlayOnLiveEdge(true);
						}
					}
				}
			}
			// Set the current playback range with the start options and clamp the start time to the range.
			SetPlaystartOptions(StartAt.Options);
			ClampStartRequestTime(StartAt.Time);


			// Do we have a play period yet?
			if (!InitialPlayPeriod.IsValid())
			{
				IManifest::FResult Result = Manifest->FindPlayPeriod(InitialPlayPeriod, StartAt, PendingStartRequest->SearchType);
				switch(Result.GetType())
				{
					case IManifest::FResult::EType::TryAgainLater:
					{
						PendingStartRequest->RetryAtTime = Result.GetRetryAgainAtTime();
						break;
					}
					case IManifest::FResult::EType::Found:
					{
						// NOTE: Do *not* reset the pending start request yet. It is still needed.
						break;
					}
					case IManifest::FResult::EType::PastEOS:
					{
						// If the initial start time is beyond the end of the presentation
						// but looping is enabled we set this request up to start at the beginning.
						if (CurrentLoopParam.bEnableLooping)
						{
							FTimeRange Seekable = Manifest->GetSeekableTimeRange();
							PendingStartRequest->SearchType = IManifest::ESearchType::Closest;
							StartAt.Time = Seekable.Start;
							ClampStartRequestTime(StartAt.Time);
							PendingStartRequest->StartAt.Time = StartAt.Time;
						}
						else
						{
							PendingStartRequest.Reset();
							InternalSetPlaybackEnded();
						}
						break;
					}
					case IManifest::FResult::EType::NotFound:
					case IManifest::FResult::EType::BeforeStart:
					case IManifest::FResult::EType::NotLoaded:
					{
						// Throw a playback error for now.
						FErrorDetail err;
						err.SetFacility(Facility::EFacility::Player);
						err.SetMessage("Could not locate the playback period to begin playback at.");
						err.SetCode(INTERR_COULD_NOT_LOCATE_START_PERIOD);
						PostError(err);
						InitialPlayPeriod.Reset();
						PendingStartRequest.Reset();
						break;
					}
				}
			}
			// Do we have a starting play period now?
			if (InitialPlayPeriod.IsValid())
			{
				// Is it ready yet?
				switch(InitialPlayPeriod->GetReadyState())
				{
					case IManifest::IPlayPeriod::EReadyState::NotLoaded:
					{
						// If there are already pending track selections update the initial selections with them and clear the pending ones.
						if (PendingTrackSelectionVid.IsValid())
						{
							StreamSelectionAttributesVid = *PendingTrackSelectionVid;
							PendingTrackSelectionVid.Reset();
						}
						if (PendingTrackSelectionAud.IsValid())
						{
							StreamSelectionAttributesAud = *PendingTrackSelectionAud;
							PendingTrackSelectionAud.Reset();
						}
						if (PendingTrackSelectionTxt.IsValid())
						{
							StreamSelectionAttributesTxt = *PendingTrackSelectionTxt;
							PendingTrackSelectionTxt.Reset();
						}

						// Prepare the play period. This must immediately change the state away from NotReady
						InitialPlayPeriod->SetStreamPreferences(EStreamType::Video, StreamSelectionAttributesVid);
						InitialPlayPeriod->SetStreamPreferences(EStreamType::Audio, StreamSelectionAttributesAud);
						InitialPlayPeriod->SetStreamPreferences(EStreamType::Subtitle, StreamSelectionAttributesTxt);
						InitialPlayPeriod->Load();
						break;
					}
					case IManifest::IPlayPeriod::EReadyState::Loading:
					{
						// Period is not yet ready. Check again on the next iteration.
						break;
					}
					case IManifest::IPlayPeriod::EReadyState::Loaded:
					{
						// Period is loaded and configured according to the stream preferences.
						// Prepare the initial quality/bitrate for playback.
						int64 StartingBitrate = -1;
						bool bForceInitial = false;
						bool bForceStartRate = false;
						if (PendingStartRequest->StartingBitrate.IsSet())
						{
							// This is used for rebuffering in which case we enforce the starting bitrate limits.
							StartingBitrate = PendingStartRequest->StartingBitrate.GetValue();
							bForceInitial = true;
							bForceStartRate = true;
						}
						else
						{
							StartingBitrate = StreamSelector->GetAverageBandwidth();
							if (PendingStartRequest->StartType == FPendingStartRequest::EStartType::PlayStart)
							{
								StartingBitrate = PlayerOptions.GetValue(OptionKeyInitialBitrate).SafeGetInt64(StartingBitrate);
								bForceInitial = true;
								bForceStartRate = true;
							}
							else if (PendingStartRequest->StartType == FPendingStartRequest::EStartType::Seeking)
							{
								if (PlayerOptions.HaveKey(OptionKeySeekStartBitrate))
								{
									StartingBitrate = PlayerOptions.GetValue(OptionKeySeekStartBitrate).SafeGetInt64(StartingBitrate);
									bForceInitial = true;
									bForceStartRate = true;
								}
							}
						}
						// If not set or set to an invalid value try to use defaults.
						if (StartingBitrate <= 0)
						{
							// Ask the manifest for the default starting bitrate, which is usually the first stream listed.
							StartingBitrate = InitialPlayPeriod->GetDefaultStartingBitrate();
							// If still nothing for whatever reason use a reasonable value.
							if (StartingBitrate <= 0)
							{
								StartingBitrate = 1000000;
							}
						}
						if (bForceStartRate)
						{
							StreamSelector->SetBandwidth(StartingBitrate);
						}
						if (bForceInitial)
						{
							StreamSelector->SetForcedNextBandwidth(StartingBitrate, GetMinBufferTimeBeforePlayback());
						}

						// Set the current average video bitrate in the player options for the period to retrieve if necessary.
						PlayerOptions.SetOrUpdate(OptionKeyCurrentAvgStartingVideoBitrate, FVariantValue(StreamSelector->GetAverageBandwidth()));

						InitialPlayPeriod->PrepareForPlay();
						break;
					}
					case IManifest::IPlayPeriod::EReadyState::Preparing:
					{
						// Period is not yet ready. Check again on the next iteration.
						break;
					}
					case IManifest::IPlayPeriod::EReadyState::IsReady:
					{
						// With the period being ready we can now get the initial media segment request.
						check(PendingStartRequest.IsValid());	// must not have been released yet

						// Now that we have a valid first-ever play start established we clear out the internal
						// default start time so we can seek backwards/forward from it to a different time.
						Manifest->ClearDefaultStartTime();

						// At playback start all streams begin in the same period.
						CurrentPlayPeriodVideo = InitialPlayPeriod;
						CurrentPlayPeriodAudio = InitialPlayPeriod;
						CurrentPlayPeriodText = InitialPlayPeriod;

						// Tell the ABR the current playback period. At playback start it is the same for all stream types.
						StreamSelector->SetCurrentPlaybackPeriod(EStreamType::Video, CurrentPlayPeriodVideo);
						StreamSelector->SetCurrentPlaybackPeriod(EStreamType::Audio, CurrentPlayPeriodAudio);

						TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoVid = CurrentPlayPeriodVideo->GetSelectedStreamBufferSourceInfo(EStreamType::Video);
						TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoAud = CurrentPlayPeriodAudio->GetSelectedStreamBufferSourceInfo(EStreamType::Audio);
						TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoTxt = CurrentPlayPeriodText->GetSelectedStreamBufferSourceInfo(EStreamType::Subtitle);
						if (BufferSourceInfoVid.IsValid())
						{
							SelectedStreamAttributesVid.UpdateWith(BufferSourceInfoVid->Kind, BufferSourceInfoVid->Language, BufferSourceInfoVid->Codec, BufferSourceInfoVid->HardIndex);
							if (ManifestType != EMediaFormatType::ISOBMFF)
							{
								StreamSelectionAttributesVid.UpdateIfOverrideSet(BufferSourceInfoVid->Kind, BufferSourceInfoVid->Language, BufferSourceInfoVid->Codec);
							}
						}
						if (BufferSourceInfoAud.IsValid())
						{
							SelectedStreamAttributesAud.UpdateWith(BufferSourceInfoAud->Kind, BufferSourceInfoAud->Language, BufferSourceInfoAud->Codec, BufferSourceInfoAud->HardIndex);
							if (ManifestType != EMediaFormatType::ISOBMFF)
							{
								StreamSelectionAttributesAud.UpdateIfOverrideSet(BufferSourceInfoAud->Kind, BufferSourceInfoAud->Language, BufferSourceInfoAud->Codec);
							}
						}
						if (BufferSourceInfoTxt.IsValid())
						{
							SelectedStreamAttributesTxt.UpdateWith(BufferSourceInfoTxt->Kind, BufferSourceInfoTxt->Language, BufferSourceInfoTxt->Codec, BufferSourceInfoTxt->HardIndex);
							if (ManifestType != EMediaFormatType::ISOBMFF)
							{
								StreamSelectionAttributesTxt.UpdateIfOverrideSet(BufferSourceInfoTxt->Kind, BufferSourceInfoTxt->Language, BufferSourceInfoTxt->Codec);
							}
						}

						// Apply any set resolution limit at the start of the new play period.
						UpdateStreamResolutionLimit();

						// Have the ABR select the initial bandwidth. Pass it an empty segment request to indicate this.
						FTimeValue ActionDelay;
						IAdaptiveStreamSelector::ESegmentAction Action = StreamSelector->SelectSuitableStreams(ActionDelay, TSharedPtrTS<IStreamSegment>());
						check(Action == IAdaptiveStreamSelector::ESegmentAction::FetchNext || Action == IAdaptiveStreamSelector::ESegmentAction::Fail);
						if (Action != IAdaptiveStreamSelector::ESegmentAction::FetchNext)
						{
							FErrorDetail err;
							err.SetFacility(Facility::EFacility::Player);
							err.SetMessage("All streams have failed. There is nothing to play any more.");
							err.SetCode(INTERR_ALL_STREAMS_HAVE_FAILED);
							PostError(err);
							PendingStartRequest.Reset();
							break;
						}

						if (!PendingStartRequest->RetryAtTime.IsValid() || CurrentTime >= PendingStartRequest->RetryAtTime)
						{
							TSharedPtrTS<IStreamSegment> FirstSegmentRequest;
							IManifest::FResult Result;
							if (PendingStartRequest->StartType != FPendingStartRequest::EStartType::LoopPoint)
							{
								// Get the current loop state to fetch the last published loop count.
								// The internal counter in CurrentLoopState may be some iterations ahead since it could have fetched segments from the loop point already.
								FInternalLoopState LoopStateNow;
								PlaybackState.GetLoopState(LoopStateNow);
								CurrentLoopState.Count = LoopStateNow.Count;

								Result = InitialPlayPeriod->GetStartingSegment(FirstSegmentRequest, CurrentPlaybackSequenceState, StartAt, PendingStartRequest->SearchType);
								bFirstSegmentRequestIsForLooping = false;
							}
							else
							{
								FPlayerSequenceState NextPlaybackState = CurrentPlaybackSequenceState;
								++NextPlaybackState.SequenceIndex;
								Result = InitialPlayPeriod->GetLoopingSegment(FirstSegmentRequest, NextPlaybackState, StartAt, PendingStartRequest->SearchType);
								if (Result.GetType() == IManifest::FResult::EType::Found)
								{
									CurrentPlaybackSequenceState = NextPlaybackState;
								}
								bFirstSegmentRequestIsForLooping = true;
							}
							switch(Result.GetType())
							{
								case IManifest::FResult::EType::TryAgainLater:
								{
									PendingStartRequest->RetryAtTime = Result.GetRetryAgainAtTime();
									break;
								}
								case IManifest::FResult::EType::Found:
								{
									// For DASH Live check if it would be better to start on the next segment.
									if (ManifestType == EMediaFormatType::DASH &&
										Manifest->GetPresentationType() != IManifest::EType::OnDemand)
									{
										struct FSegTime { FTimeValue Start, Into, Duration; };
										FSegTime t0;
										if (FirstSegmentRequest->GetStartupDelay(t0.Start, t0.Into, t0.Duration))
										{
											TSharedPtrTS<IStreamSegment> NextStartSegmentRequest;
											FPlayStartPosition LaterStart = StartAt;
											FTimeValue RemainingSegmentDuration = t0.Duration - t0.Into;
											LaterStart.Time += RemainingSegmentDuration;
											LaterStart.Options.bFrameAccuracy = false;
											IManifest::FResult LaterResult = InitialPlayPeriod->GetStartingSegment(NextStartSegmentRequest, CurrentPlaybackSequenceState, LaterStart, IManifest::ESearchType::Closest);
											if (LaterResult.GetType() == IManifest::FResult::EType::Found && NextStartSegmentRequest.IsValid())
											{
												// Are we skipping more than half the segment's duration?
												if (t0.Into >= t0.Duration/2)
												{
													// Start with the next segment if this segment has less than 1s of usable duration.
													bool bStartWithNextSegment = RemainingSegmentDuration.GetAsSeconds() < 1.0;

													// If using the next segment would put us under the minimum required latency we cannot start with it
													// if we are to play on the Live edge.
													if (ABRShouldPlayOnLiveEdge())
													{
														double NextSegStartLiveLatency = (ABRGetWallclockTime() - NextStartSegmentRequest->GetFirstPTS()).GetAsSeconds();
														double MinLiveLatency = 0.0;
														TSharedPtrTS<const FLowLatencyDescriptor> llDesc = ABRGetLowLatencyDescriptor();
														MinLiveLatency = llDesc.IsValid() ? llDesc->GetLatencyMin().GetAsSeconds(MinLiveLatency) : MinLiveLatency;
														if (NextSegStartLiveLatency < MinLiveLatency)
														{
															bStartWithNextSegment = false;
														}
													}

													if (bStartWithNextSegment)
													{
														PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Verbose, FString::Printf(TEXT("Using next segment since it has a nearer start time.")));
														FirstSegmentRequest = NextStartSegmentRequest;
													}
												}
											}
										}
									}

									// Add this period to the list of upcoming ones. This ensures the period metadata gets reported
									// if the period playback starts in is not the first for which metadata was initially reported.
									AddUpcomingPeriod(InitialPlayPeriod);
									// Remember the selected buffer source infos.
									UpdatePeriodStreamBufferSourceInfo(InitialPlayPeriod, EStreamType::Video, BufferSourceInfoVid);
									UpdatePeriodStreamBufferSourceInfo(InitialPlayPeriod, EStreamType::Audio, BufferSourceInfoAud);
									UpdatePeriodStreamBufferSourceInfo(InitialPlayPeriod, EStreamType::Subtitle, BufferSourceInfoTxt);

									PendingFirstSegmentRequest = FirstSegmentRequest;
									// No longer need the start request.
									PendingStartRequest.Reset();
									// Also no longer need the initial play period.
									InitialPlayPeriod.Reset();
									break;
								}
								case IManifest::FResult::EType::NotFound:
								case IManifest::FResult::EType::NotLoaded:
								{
									// Reset the current play period and start over with the initial period selection.
									PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Info, FString::Printf(TEXT("Period start segments not found. Reselecting start period.")));
									InitialPlayPeriod.Reset();
									break;
								}
								case IManifest::FResult::EType::BeforeStart:
								case IManifest::FResult::EType::PastEOS:
								{
									FErrorDetail err;
									err.SetFacility(Facility::EFacility::Player);
									err.SetMessage("Could not locate the stream segment to begin playback at.");
									err.SetCode(INTERR_COULD_NOT_LOCATE_START_SEGMENT);
									PostError(err);
									PendingStartRequest.Reset();
									InitialPlayPeriod.Reset();
									break;
								}
							}
						}
						break;
					}
				}
			}
		}
		else
		{
			// No manifest, no start request. Those are the rules.
			PendingStartRequest.Reset();
		}
	}
}


void FAdaptiveStreamingPlayer::InternalHandlePendingFirstSegmentRequest(const FTimeValue& CurrentTime)
{
	// Is there a play start initial segment download request?
	if (PendingFirstSegmentRequest.IsValid())
	{
		check(StreamReaderHandler);
		if (StreamReaderHandler)
		{
			// Create the new stream data receive buffer
			TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoVid = CurrentPlayPeriodVideo.IsValid() ? CurrentPlayPeriodVideo->GetSelectedStreamBufferSourceInfo(EStreamType::Video) : nullptr;
			TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoAud = CurrentPlayPeriodAudio.IsValid() ? CurrentPlayPeriodAudio->GetSelectedStreamBufferSourceInfo(EStreamType::Audio) : nullptr;
			TSharedPtrTS<FBufferSourceInfo> BufferSourceInfoTxt = CurrentPlayPeriodText.IsValid() ? CurrentPlayPeriodText->GetSelectedStreamBufferSourceInfo(EStreamType::Subtitle) : nullptr;

			TSharedPtrTS<FStreamDataBuffers> NewReceiveBuffers = MakeSharedTS<FStreamDataBuffers>();
			NewReceiveBuffers->VidBuffer = MakeSharedTS<FMultiTrackAccessUnitBuffer>(EStreamType::Video);
			NewReceiveBuffers->AudBuffer = MakeSharedTS<FMultiTrackAccessUnitBuffer>(EStreamType::Audio);
			NewReceiveBuffers->TxtBuffer = MakeSharedTS<FMultiTrackAccessUnitBuffer>(EStreamType::Subtitle);

			if (ManifestType == EMediaFormatType::ISOBMFF)
			{
				// mp4 has multiple tracks in a single stream that provide data all at the same time.
				NewReceiveBuffers->VidBuffer->SetParallelTrackMode();
				NewReceiveBuffers->AudBuffer->SetParallelTrackMode();
				NewReceiveBuffers->TxtBuffer->SetParallelTrackMode();
			}
			NewReceiveBuffers->VidBuffer->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(EStreamType::Video)], BufferSourceInfoVid);
			NewReceiveBuffers->AudBuffer->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(EStreamType::Audio)], BufferSourceInfoAud);
			NewReceiveBuffers->TxtBuffer->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(EStreamType::Subtitle)], BufferSourceInfoTxt);
			// Flag tracks that will not deliver any data accordingly.
			if (!BufferSourceInfoVid.IsValid())
			{
				NewReceiveBuffers->VidBuffer->SetEndOfTrackAll();
			}
			if (!BufferSourceInfoAud.IsValid())
			{
				NewReceiveBuffers->AudBuffer->SetEndOfTrackAll();
			}
			if (!BufferSourceInfoTxt.IsValid())
			{
				NewReceiveBuffers->TxtBuffer->SetEndOfTrackAll();
			}

			// Add the new receive buffer to the list.
			DataBuffersCriticalSection.Lock();
			NextDataBuffers.Emplace(NewReceiveBuffers);
			// And for easier access set it as the current receive buffer to which all new data gets added.
			CurrentDataReceiveBuffers = MoveTemp(NewReceiveBuffers);
			DataBuffersCriticalSection.Unlock();

			if (!bFirstSegmentRequestIsForLooping)
			{
				FTimeValue CurrentPlayPos = PlaybackState.GetPlayPosition();
				FTimeValue NewPlayPos = PendingFirstSegmentRequest->GetFirstPTS();
				// If the PTS is offset by an edit list it could be negative. We do not want to
				// use negative values so we set it to zero instead. This is not a problem.
				if (NewPlayPos < FTimeValue::GetZero())
				{
					NewPlayPos.SetToZero();
				}
				PlaybackState.SetPlayPosition(NewPlayPos);
				if (CurrentState == EPlayerState::eState_Seeking)
				{
					// Get the current loop state to fetch the last published loop count.
					// The internal counter in CurrentLoopState may be some iterations ahead since it could have fetched segments from the loop point.
					FInternalLoopState LoopStateNow;
					PlaybackState.GetLoopState(LoopStateNow);
					CurrentLoopState.Count = LoopStateNow.Count;
				}
				PlaybackState.SetLoopState(CurrentLoopState);

				check(CurrentState == EPlayerState::eState_Buffering || CurrentState == EPlayerState::eState_Rebuffering || CurrentState == EPlayerState::eState_Seeking);
				if (CurrentState == EPlayerState::eState_Seeking)
				{
					DispatchEvent(FMetricEvent::ReportJumpInPlayPosition(NewPlayPos, CurrentPlayPos, Metrics::ETimeJumpReason::UserSeek));
				}

				if (AudioRender.Renderer)
				{
					RenderClock->SetCurrentTime(IMediaRenderClock::ERendererType::Audio, NewPlayPos);
					AudioRender.Renderer->SetNextApproximatePresentationTime(NewPlayPos);
				}
				if (VideoRender.Renderer)
				{
					RenderClock->SetCurrentTime(IMediaRenderClock::ERendererType::Video, NewPlayPos);
					VideoRender.Renderer->SetNextApproximatePresentationTime(NewPlayPos);
				}

				PlaybackState.SetIsBuffering(true);
				PlaybackState.SetIsSeeking(true);

				// Send QoS buffering event
				DispatchBufferingEvent(true, CurrentState);

				// Remember why we are buffering so we can send the proper QoS event when buffering is done.
				LastBufferingState = CurrentState;

				// Whether we were seeking or rebuffering, we're now just buffering.
				CurrentState = EPlayerState::eState_Buffering;
			}
			else
			{
				PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Verbose, FString::Printf(TEXT("Enqueuing stream loop start request.")));
				// Add the updated loop state to the upcoming loop queue.
				CurrentLoopState.To = PendingFirstSegmentRequest->GetFirstPTS();
				NextLoopStates.Enqueue(CurrentLoopState);
			}

			// Get the stream types that have already reached EOS in this start request because the streams are of different duration.
			// The streams are there in principle so we need to set them up (in case we loop back into them) but we won't see any data arriving.
			TArray<TSharedPtrTS<IStreamSegment>> AlreadyEndedStreams;
			PendingFirstSegmentRequest->GetEndedStreams(AlreadyEndedStreams);
			for(int32 i=0; i<AlreadyEndedStreams.Num(); ++i)
			{
				CompletedSegmentRequests.Add(AlreadyEndedStreams[i]->GetType(), AlreadyEndedStreams[i]);
			}

			// Get the requested streams from the initial request (which may be a wrapper for several individual streams)
			// and add them to the request queue.
			TArray<TSharedPtrTS<IStreamSegment>> RequestedStreams;
			PendingFirstSegmentRequest->GetRequestedStreams(RequestedStreams);
			for(auto &RequestedStream : RequestedStreams)
			{
				ReadyWaitingSegmentRequests.Enqueue(RequestedStream);
			}
		}
		PendingFirstSegmentRequest.Reset();
		bStreamingHasStarted = true;
	}
}


void FAdaptiveStreamingPlayer::InternalHandleCompletedSegmentRequests(const FTimeValue& CurrentTime)
{
	// Check the streams that reached EOS. If all of them are done decide what to do, end playback or loop.
	if (CompletedSegmentRequests.Num())
	{
		bool bAllAtEOS = true;

		auto CheckStreamTypeEOS = [this](bool& bInOutAllEOS, EStreamType InStreamType) -> void
		{
			TSharedPtrTS<FMultiTrackAccessUnitBuffer> RcvBuffer = GetCurrentReceiveStreamBuffer(InStreamType);
			if (IsExpectedToStreamNow(InStreamType))
			{
				if (!CompletedSegmentRequests.Contains(InStreamType))
				{
					bInOutAllEOS = false;
				}
				else if (RcvBuffer.IsValid())
				{
					RcvBuffer->SetEndOfTrackAll();
					RcvBuffer->PushEndOfDataAll();
				}
			}
			else if (RcvBuffer.IsValid())
			{
				RcvBuffer->SetEndOfTrackAll();
				RcvBuffer->PushEndOfDataAll();
			}
		};

		CheckStreamTypeEOS(bAllAtEOS, EStreamType::Video);
		CheckStreamTypeEOS(bAllAtEOS, EStreamType::Audio);
		CheckStreamTypeEOS(bAllAtEOS, EStreamType::Subtitle);

		if (bAllAtEOS)
		{
			// Move the finished requests into a local work map, emptying the original.
			TMultiMap<EStreamType, TSharedPtrTS<IStreamSegment>> LocalFinishedRequests(MoveTemp(CompletedSegmentRequests));

			// Looping enabled?
			if (CurrentLoopParam.bEnableLooping)
			{
				if (Manifest.IsValid())
				{
					FTimeRange Seekable = Manifest->GetSeekableTimeRange();

					PendingStartRequest = MakeSharedTS<FPendingStartRequest>();
					// Loop back to the beginning. If a play range has been set that has a larger in-point
					// the time will be clamped to that. The in-point cannot be before the start point.
					PendingStartRequest->SearchType = IManifest::ESearchType::Closest;
					PendingStartRequest->StartAt.Time = Seekable.Start;
					SetPlaystartOptions(PendingStartRequest->StartAt.Options);
					PendingStartRequest->StartType = FPendingStartRequest::EStartType::LoopPoint;
					// Increase the internal loop count
					++CurrentLoopState.Count;
					CurrentLoopState.To = ClampTimeToCurrentRange(Seekable.Start, true, false);
					CurrentLoopState.From = ClampTimeToCurrentRange(FTimeValue::GetPositiveInfinity(), false, true);
				}
			}
		}
	}
}


void FAdaptiveStreamingPlayer::RequestNewPeriodStreams(EStreamType InType, FPendingSegmentRequest& InOutCurrentRequest)
{
	// Check if the next period has streams the current one does not have.
	// We need to trigger segment requests for those.
	if (!InOutCurrentRequest.bDidRequestNewPeriodStreams)
	{
		InOutCurrentRequest.bDidRequestNewPeriodStreams = true;

		auto RequestIfNeeded = [this, &InOutCurrentRequest](EStreamType InStreamType)
		{
			if (!IsExpectedToStreamNow(InStreamType))
			{
				FPendingSegmentRequest NextReq;
				NextReq.Period = InOutCurrentRequest.Period;
				NextReq.StreamType = InStreamType;
				NextReq.StartoverPosition.Time = InOutCurrentRequest.Period->GetMediaAsset()->GetTimeRange().Start;
				SetPlaystartOptions(NextReq.StartoverPosition.Options);
				NextReq.bStartOver = true;
				NextReq.bDidRequestNewPeriodStreams = true;
				NextPendingSegmentRequests.Enqueue(MoveTemp(NextReq));
			}
		};

		if (InType == EStreamType::Video)
		{
			RequestIfNeeded(EStreamType::Audio);
			RequestIfNeeded(EStreamType::Subtitle);
		}
		else if (InType == EStreamType::Audio)
		{
			RequestIfNeeded(EStreamType::Video);
			RequestIfNeeded(EStreamType::Subtitle);
		}
		else if (InType == EStreamType::Subtitle)
		{
			RequestIfNeeded(EStreamType::Video);
			RequestIfNeeded(EStreamType::Audio);
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Handles pending media segment requests.
 */
void FAdaptiveStreamingPlayer::HandlePendingMediaSegmentRequests()
{
	if (CurrentState == EPlayerState::eState_Error)
	{
		return;
	}

	FTimeValue UTCNow = GetSynchronizedUTCTime()->GetTime();
	FTimeValue CurrentTime = MEDIAutcTime::Current();
	InternalHandlePendingStartRequest(CurrentTime);
	InternalHandlePendingFirstSegmentRequest(CurrentTime);
	InternalHandleSegmentTrackChanges(CurrentTime);

	FTimeValue CurrentPlaybackPos = GetPlayPosition();

	// Are there completed downloads for which we need to find the next segment?
	TQueue<FPendingSegmentRequest> pendingRequests;
	Swap(pendingRequests, NextPendingSegmentRequests);
	while(!pendingRequests.IsEmpty())
	{
		FPendingSegmentRequest FinishedReq;
		pendingRequests.Dequeue(FinishedReq);

		TSharedPtrTS<IManifest::IPlayPeriod> SegmentPlayPeriod;

		EStreamType StreamType = FinishedReq.StreamType != EStreamType::Unsupported ? FinishedReq.StreamType : FinishedReq.Request.IsValid() ? FinishedReq.Request->GetType() : EStreamType::Unsupported;
		switch(StreamType)
		{
			default:
			{
				break;
			}
			case EStreamType::Video:
			{
				SegmentPlayPeriod = CurrentPlayPeriodVideo;
				break;
			}
			case EStreamType::Audio:
			{
				SegmentPlayPeriod = CurrentPlayPeriodAudio;
				break;
			}
			case EStreamType::Subtitle:
			{
				SegmentPlayPeriod = CurrentPlayPeriodText;
				break;
			}
		}

		// If not yet ready to check again put the request back into the list.
		if (FinishedReq.AtTime.IsValid() && FinishedReq.AtTime > CurrentTime)
		{
			NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
		}
		else if (SegmentPlayPeriod.IsValid())
		{
			// Is this a startover for a new period?
			if ((FinishedReq.bStartOver || FinishedReq.bPlayPosAutoReselect) && !FinishedReq.Period.IsValid())
			{
				// Locate the period the startover time is in.
				TSharedPtrTS<IManifest::IPlayPeriod> StartoverPeriod;
				IManifest::FResult PeriodResult = Manifest->FindPlayPeriod(StartoverPeriod, FinishedReq.StartoverPosition, IManifest::ESearchType::After);
				// When found or at the end of the stream
				if (PeriodResult.GetType() == IManifest::FResult::EType::Found)
				{
					FinishedReq.Period = MoveTemp(StartoverPeriod);
				}
				else if (PeriodResult.GetType() == IManifest::FResult::EType::PastEOS)
				{
					// At the end of the stream we just don't do anything. Just ignore the switch.
					continue;
				}
				else
				{
					FErrorDetail err;
					err.SetFacility(Facility::EFacility::Player);
					err.SetMessage("Could not locate startover period for track switch");
					err.SetCode(INTERR_COULD_NOT_LOCATE_START_PERIOD);
					PostError(err);
					break;
				}
			}

			// Did we move into a new or a startover period that needs to be made ready?
			if (FinishedReq.Period.IsValid())
			{
				IManifest::IPlayPeriod::EReadyState PeriodState = FinishedReq.Period->GetReadyState();
				if (PeriodState == IManifest::IPlayPeriod::EReadyState::NotLoaded)
				{
					FinishedReq.Period->SetStreamPreferences(EStreamType::Video, StreamSelectionAttributesVid);
					FinishedReq.Period->SetStreamPreferences(EStreamType::Audio, StreamSelectionAttributesAud);
					FinishedReq.Period->SetStreamPreferences(EStreamType::Subtitle, StreamSelectionAttributesTxt);
					FinishedReq.Period->Load();
					NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
					continue;
				}
				else if (PeriodState == IManifest::IPlayPeriod::EReadyState::Loading)
				{
					NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
					continue;
				}
				else if (PeriodState == IManifest::IPlayPeriod::EReadyState::Loaded)
				{
					// Set the current average video bitrate in the player options for the period to retrieve if necessary.
					PlayerOptions.SetOrUpdate(OptionKeyCurrentAvgStartingVideoBitrate, FVariantValue(StreamSelector->GetAverageBandwidth()));
					FinishedReq.Period->PrepareForPlay();
					NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
					continue;
				}
				else if (PeriodState == IManifest::IPlayPeriod::EReadyState::Preparing)
				{
					NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
					continue;
				}
				else if (PeriodState == IManifest::IPlayPeriod::EReadyState::IsReady)
				{
					RequestNewPeriodStreams(StreamType, FinishedReq);

					TSharedPtrTS<FMultiTrackAccessUnitBuffer> StreamRcvBuffer = GetCurrentReceiveStreamBuffer(StreamType);
					if (StreamType == EStreamType::Video)
					{
						SegmentPlayPeriod = FinishedReq.Period;
						CurrentPlayPeriodVideo = FinishedReq.Period;
						StreamSelector->SetCurrentPlaybackPeriod(EStreamType::Video, CurrentPlayPeriodVideo);
						UpdateStreamResolutionLimit();
					}
					else if (StreamType == EStreamType::Audio)
					{
						SegmentPlayPeriod = FinishedReq.Period;
						CurrentPlayPeriodAudio = FinishedReq.Period;
						StreamSelector->SetCurrentPlaybackPeriod(EStreamType::Audio, CurrentPlayPeriodAudio);
					}
					else if (StreamType == EStreamType::Subtitle)
					{
						SegmentPlayPeriod = FinishedReq.Period;
						CurrentPlayPeriodText = FinishedReq.Period;
						//StreamSelector->SetCurrentPlaybackPeriod(EStreamType::Subtitle, CurrentPlayPeriodText);
					}
					// Add this to the upcoming periods the play position will move into and metadata will need to be updated then
					// unless this is a startover request.
					if (!FinishedReq.bStartOver || FinishedReq.bPlayPosAutoReselect)
					{
						AddUpcomingPeriod(SegmentPlayPeriod);
					}
					UpdatePeriodStreamBufferSourceInfo(SegmentPlayPeriod, StreamType, SegmentPlayPeriod->GetSelectedStreamBufferSourceInfo(StreamType));
				}
			}

			// Evaluate ABR to select the next stream quality.
			FTimeValue ActionDelay(FTimeValue::GetZero());
			IAdaptiveStreamSelector::ESegmentAction Action = StreamSelector->SelectSuitableStreams(ActionDelay, FinishedReq.Request);
			if (Action == IAdaptiveStreamSelector::ESegmentAction::Fail)
			{
				FErrorDetail err;
				err.SetFacility(Facility::EFacility::Player);
				err.SetMessage("All streams have failed. There is nothing to play any more.");
				err.SetCode(INTERR_ALL_STREAMS_HAVE_FAILED);
				PostError(err);
				break;
			}

			TSharedPtrTS<IStreamSegment> NextSegment;
			IManifest::FResult Result;
			if (!FinishedReq.bStartOver && !FinishedReq.bPlayPosAutoReselect)
			{
				FPlayStartOptions Options;
				SetPlaystartOptions(Options);
				if (Action == IAdaptiveStreamSelector::ESegmentAction::FetchNext)
				{
					Result = SegmentPlayPeriod->GetNextSegment(NextSegment, FinishedReq.Request, Options);
				}
				else if (Action == IAdaptiveStreamSelector::ESegmentAction::Retry || Action == IAdaptiveStreamSelector::ESegmentAction::Fill)
				{
					Result = SegmentPlayPeriod->GetRetrySegment(NextSegment, FinishedReq.Request, Options, Action == IAdaptiveStreamSelector::ESegmentAction::Fill);
				}
			}
			else
			{
				Result = SegmentPlayPeriod->GetContinuationSegment(NextSegment, StreamType, CurrentPlaybackSequenceState, FinishedReq.StartoverPosition, IManifest::ESearchType::After);
			}
			switch(Result.GetType())
			{
				case IManifest::FResult::EType::TryAgainLater:
				{
					FinishedReq.AtTime = Result.GetRetryAgainAtTime();
					NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
					break;
				}
				case IManifest::FResult::EType::Found:
				{
					// Switching tracks?
					if (FinishedReq.bStartOver)
					{
						// Tell the multi stream buffer where to switch to and update the selection defaults.
						TSharedPtrTS<FBufferSourceInfo> BufferSourceInfo = SegmentPlayPeriod->GetSelectedStreamBufferSourceInfo(StreamType);
						if (BufferSourceInfo.IsValid())
						{
							TSharedPtrTS<FMultiTrackAccessUnitBuffer> StreamRcvBuffer = GetCurrentReceiveStreamBuffer(StreamType);
							if (StreamRcvBuffer.IsValid())
							{
								StreamRcvBuffer->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(StreamType)], BufferSourceInfo);
							}
							switch(StreamType)
							{
								case EStreamType::Video:
								{
									SelectedStreamAttributesVid.UpdateWith(BufferSourceInfo->Kind, BufferSourceInfo->Language, BufferSourceInfo->Codec, BufferSourceInfo->HardIndex);
									if (ManifestType != EMediaFormatType::ISOBMFF)
									{
										StreamSelectionAttributesVid.ClearOverrideIndex();
									}
									break;
								}
								case EStreamType::Audio:
								{
									SelectedStreamAttributesAud.UpdateWith(BufferSourceInfo->Kind, BufferSourceInfo->Language, BufferSourceInfo->Codec, BufferSourceInfo->HardIndex);
									if (ManifestType != EMediaFormatType::ISOBMFF)
									{
										StreamSelectionAttributesAud.ClearOverrideIndex();
									}
									break;
								}
								case EStreamType::Subtitle:
								{
									SelectedStreamAttributesTxt.UpdateWith(BufferSourceInfo->Kind, BufferSourceInfo->Language, BufferSourceInfo->Codec, BufferSourceInfo->HardIndex);
									if (ManifestType != EMediaFormatType::ISOBMFF)
									{
										StreamSelectionAttributesTxt.ClearOverrideIndex();
									}
									break;
								}
								default:
								{
									break;
								}
							}
						}
						UpdatePeriodStreamBufferSourceInfo(SegmentPlayPeriod, StreamType, BufferSourceInfo);
					}

					NextSegment->SetExecutionDelay(UTCNow, ActionDelay);
					ReadyWaitingSegmentRequests.Enqueue(NextSegment);
					break;
				}
				case IManifest::FResult::EType::PastEOS:
				{
					TSharedPtrTS<IManifest::IPlayPeriod> NextPeriod;
					IManifest::FResult NextPeriodResult = Manifest->FindNextPlayPeriod(NextPeriod, FinishedReq.Request);
					switch(NextPeriodResult.GetType())
					{
						case IManifest::FResult::EType::TryAgainLater:
						{
							// Next period is either not there yet or is being resolved right now.
							FinishedReq.AtTime = Result.GetRetryAgainAtTime();
							NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
							break;
						}
						case IManifest::FResult::EType::Found:
						{
							// Get the next period ready for playback. Put it back into the queue.
							FinishedReq.Period = NextPeriod;
							NextPendingSegmentRequests.Enqueue(MoveTemp(FinishedReq));
							break;
						}
						case IManifest::FResult::EType::PastEOS:
						case IManifest::FResult::EType::NotFound:
						case IManifest::FResult::EType::BeforeStart:
						case IManifest::FResult::EType::NotLoaded:
						{
							// Get the dependent stream types, if any.
							// This is mainly for multiplexed streams like an .mp4 at this point.
							// All stream types have reached EOS now.
							// NOTE: It is possible that there is no actual request here. This happens when a stream
							// gets enabled (like a subtitle stream) but is immediately at EOS and thus has no actual segment request!
							if (FinishedReq.Request.IsValid())
							{
								TArray<TSharedPtrTS<IStreamSegment>> DependentStreams;
								FinishedReq.Request->GetDependentStreams(DependentStreams);
								for(int32 i=0; i<DependentStreams.Num(); ++i)
								{
									CompletedSegmentRequests.Add(DependentStreams[i]->GetType(), FinishedReq.Request);
								}
							}
							// Add the primary stream type to the list as well.
							CompletedSegmentRequests.Add(StreamType, FinishedReq.Request);
							break;
						}
					}
					break;
				}
				case IManifest::FResult::EType::NotFound:
				{
					// Not found here should indicate that the type of stream is not available (or not selected) in the new period.
					TSharedPtrTS<ITimelineMediaAsset> ThisPeriod = SegmentPlayPeriod->GetMediaAsset();
					if (ThisPeriod.IsValid())
					{
						UpdatePeriodStreamBufferSourceInfo(SegmentPlayPeriod, StreamType, TSharedPtrTS<FBufferSourceInfo>());
						TSharedPtrTS<FMultiTrackAccessUnitBuffer> RcvBuffer = GetCurrentReceiveStreamBuffer(StreamType);
						if (RcvBuffer.IsValid())
						{
							RcvBuffer->SetEndOfTrackAll();
						}
					}
					break;
				}
				case IManifest::FResult::EType::BeforeStart:
				case IManifest::FResult::EType::NotLoaded:
				{
					// Throw a playback error for now.
					FErrorDetail err;
					err.SetFacility(Facility::EFacility::Player);
					err.SetMessage("Could not locate next segment");
					err.SetCode(INTERR_FRAGMENT_NOT_AVAILABLE);
					PostError(err);
					break;
				}
			}
		}
	}


	// Go through the queue of ready-to-fetch segments to see if they should be fetched now.
	TQueue<TSharedPtrTS<IStreamSegment>> readyRequests;
	Swap(readyRequests, ReadyWaitingSegmentRequests);
	while(!readyRequests.IsEmpty())
	{
		TSharedPtrTS<IStreamSegment> ReadyReq;
		readyRequests.Dequeue(ReadyReq);

		bool bExecuteNow = true;
		bool bSkipOver = false;

		// Does this request have an availability window?
		FTimeValue When = ReadyReq->GetExecuteAtUTCTime();
		if (When.IsValid() && When > UTCNow)
		{
			bExecuteNow = false;
		}

		// Subtitles need to be throttled based on play position.
		// Due them being sparse and small they would essentially all get fetched right away.
		if (ReadyReq->GetType() == EStreamType::Subtitle)
		{
			// Fetch up to 20 seconds ahead of time.
			const FTimeValue AdvanceFetchThreshold(FTimeValue::MillisecondsToHNS(1000 * 20));
			FTimeValue NextPTS = ReadyReq->GetFirstPTS();
			if (CurrentPlaybackPos.IsValid() && NextPTS.IsValid() && CurrentPlaybackPos + AdvanceFetchThreshold < NextPTS)
			{
				bExecuteNow = false;
			}
			// When subtitles are disabled we do not actually fetch the segment.
			bSkipOver = SelectedStreamAttributesTxt.IsDeselected();
		}

		if (bExecuteNow)
		{
			if (!bSkipOver)
			{
				IStreamReader::EAddResult ReqResult = StreamReaderHandler->AddRequest(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(ReadyReq->GetType())], ReadyReq);
				if (ReqResult != IStreamReader::EAddResult::Added)
				{
					FErrorDetail err;
					err.SetFacility(Facility::EFacility::Player);
					err.SetMessage("Failed to add stream segment request to reader");
					err.SetCode(INTERR_FRAGMENT_READER_REQUEST);
					PostError(err);
					break;
				}
			}
			else
			{
				// Skip over this segment. Add to the list of completed requests for which we need to find the next segment to fetch.
				FPendingSegmentRequest NextReq;
				NextReq.Request = ReadyReq;
				NextPendingSegmentRequests.Enqueue(MoveTemp(NextReq));
			}
		}
		else
		{
			ReadyWaitingSegmentRequests.Enqueue(ReadyReq);
		}
	}

	InternalHandleCompletedSegmentRequests(CurrentTime);
}

void FAdaptiveStreamingPlayer::CancelPendingMediaSegmentRequests(EStreamType StreamType)
{
	if (StreamReaderHandler)
	{
		StreamReaderHandler->CancelRequest(StreamType, true);
	}
	// Go through the queue of ready-to-fetch segments and remove those that are to be canceled.
	TQueue<TSharedPtrTS<IStreamSegment>> readyRequests;
	Swap(readyRequests, ReadyWaitingSegmentRequests);
	while(!readyRequests.IsEmpty())
	{
		TSharedPtrTS<IStreamSegment> ReadyReq;
		readyRequests.Dequeue(ReadyReq);
		if (ReadyReq->GetType() != StreamType)
		{
			ReadyWaitingSegmentRequests.Enqueue(ReadyReq);
		}
	}
}


void FAdaptiveStreamingPlayer::InternalDeselectStream(EStreamType StreamType)
{
	switch(StreamType)
	{
		case EStreamType::Video:
			bIsVideoDeselected = true;
			SelectedStreamAttributesVid.Deselect();
			VideoDecoder.Flush();
			VideoRender.Flush(false);
			break;
		case EStreamType::Audio:
			bIsAudioDeselected = true;
			SelectedStreamAttributesAud.Deselect();
			AudioDecoder.Flush();
			AudioRender.Flush();
			break;
		case EStreamType::Subtitle:
			bIsTextDeselected = true;
			SelectedStreamAttributesTxt.Deselect();
			SubtitleDecoder.Flush();
			break;
		default:
			break;
	}
}

void FAdaptiveStreamingPlayer::InternalStartoverAtCurrentPosition()
{
	{
		// Check if a seek is currently pending. If so then let us perform the seek instead.
		FScopeLock lock(&SeekVars.Lock);
		// Is there a pending request?
		if (SeekVars.PendingRequest.IsSet())
		{
			// To ensure the seek will execute we clear out the active and last finished requests.
			SeekVars.ActiveRequest.Reset();
			SeekVars.LastFinishedRequest.Reset();
			return;
		}
	}
	FSeekParam NewPosition;
	NewPosition.Time = PlaybackState.GetPlayPosition();
	InternalStop(PlayerConfig.bHoldLastFrameDuringSeek);
	CurrentState = EPlayerState::eState_Seeking;
	// Get the current loop state back from the playback state to keep the loop counter as it is now.
	PlaybackState.GetLoopState(CurrentLoopState);
	InternalStartAt(NewPosition);
}

void FAdaptiveStreamingPlayer::InternalStartoverAtLiveEdge()
{
	FSeekParam NewPosition;
	InternalStop(PlayerConfig.bHoldLastFrameDuringSeek);
	CurrentState = EPlayerState::eState_Seeking;
	PlaybackState.GetLoopState(CurrentLoopState);
	InternalStartAt(NewPosition);
}


void FAdaptiveStreamingPlayer::InternalHandleSegmentTrackChanges(const FTimeValue& CurrentTime)
{
	bool bChangeMade = false;

	if (ManifestType == EMediaFormatType::ISOBMFF)
	{
		auto HandleISOBMFFTrackSelection = [&bChangeMade, this](FStreamSelectionAttributes& OutSelectionAttributes, FInternalStreamSelectionAttributes& InOutSelectedAttributes, EStreamType InType,
																TSharedPtrTS<FStreamSelectionAttributes>& InPendingSelection, TSharedPtrTS<IManifest::IPlayPeriod> InCurrentPeriod) -> void
		{
			if (InPendingSelection.IsValid() && InCurrentPeriod.IsValid())
			{
				IManifest::IPlayPeriod::ETrackChangeResult Result = InCurrentPeriod->ChangeTrackStreamPreference(InType, *InPendingSelection);
				TSharedPtrTS<FBufferSourceInfo> BufferSourceInfo = InCurrentPeriod->GetSelectedStreamBufferSourceInfo(InType);
				if (Result == IManifest::IPlayPeriod::ETrackChangeResult::Changed)
				{
					TSharedPtrTS<FMultiTrackAccessUnitBuffer> StreamRcvBuf = GetCurrentReceiveStreamBuffer(InType);
					if (StreamRcvBuf.IsValid())
					{
						StreamRcvBuf->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(InType)], BufferSourceInfo);
					}
					OutSelectionAttributes = *InPendingSelection;
					bChangeMade = true;
				}
				InOutSelectedAttributes.UpdateWith(BufferSourceInfo->Kind, BufferSourceInfo->Language, BufferSourceInfo->Codec, BufferSourceInfo->HardIndex);
				InOutSelectedAttributes.Select();
				InPendingSelection.Reset();
			}
		};

		HandleISOBMFFTrackSelection(StreamSelectionAttributesVid, SelectedStreamAttributesVid, EStreamType::Video, PendingTrackSelectionVid, CurrentPlayPeriodVideo);
		HandleISOBMFFTrackSelection(StreamSelectionAttributesAud, SelectedStreamAttributesAud, EStreamType::Audio, PendingTrackSelectionAud, CurrentPlayPeriodAudio);
		HandleISOBMFFTrackSelection(StreamSelectionAttributesTxt, SelectedStreamAttributesTxt, EStreamType::Subtitle, PendingTrackSelectionTxt, CurrentPlayPeriodText);
	}
	else if (ManifestType == EMediaFormatType::HLS)
	{
		if (PendingTrackSelectionAud.IsValid() && CurrentPlayPeriodAudio.IsValid())
		{
			TSharedPtrTS<FBufferSourceInfo> BufferSourceInfo = CurrentPlayPeriodAudio->GetSelectedStreamBufferSourceInfo(EStreamType::Audio);
			TSharedPtrTS<FMultiTrackAccessUnitBuffer> StreamRcvBuf = GetCurrentReceiveStreamBuffer(EStreamType::Audio);
			if (StreamRcvBuf.IsValid())
			{
				StreamRcvBuf->SelectTrackWhenAvailable(CurrentPlaybackSequenceID[StreamTypeToArrayIndex(EStreamType::Audio)], BufferSourceInfo);
			}
			StreamSelectionAttributesAud = *PendingTrackSelectionAud;
			SelectedStreamAttributesAud.UpdateWith(BufferSourceInfo->Kind, BufferSourceInfo->Language, BufferSourceInfo->Codec, BufferSourceInfo->HardIndex);
			PendingTrackSelectionAud.Reset();
			bChangeMade = true;
		}
		// Ignore video and subtitle changes for now.
		PendingTrackSelectionVid.Reset();
		PendingTrackSelectionTxt.Reset();
	}
	else if (ManifestType == EMediaFormatType::DASH)
	{
		if (PendingStartRequest.IsValid() || PendingFirstSegmentRequest.IsValid())
		{
			return;
		}

		auto HandleDASHTrackSelection = [&bChangeMade, this](FStreamSelectionAttributes& OutSelectionAttributes, FInternalStreamSelectionAttributes& InOutSelectedAttributes, EStreamType InType,
															 TSharedPtrTS<FStreamSelectionAttributes>& InPendingSelection, TSharedPtrTS<IManifest::IPlayPeriod> InCurrentPeriod) -> void
		{
			if (InPendingSelection.IsValid() && InCurrentPeriod.IsValid())
			{
				if (InOutSelectedAttributes.IsDeselected() || !InOutSelectedAttributes.IsCompatibleWith(*InPendingSelection))
				{
					IManifest::IPlayPeriod::ETrackChangeResult TrackChangeResult = InCurrentPeriod->ChangeTrackStreamPreference(InType, *InPendingSelection);
					if (TrackChangeResult == IManifest::IPlayPeriod::ETrackChangeResult::NewPeriodNeeded)
					{
						// Make sure any finishing current requests will be ignored by increasing the sequence ID.
						++CurrentPlaybackSequenceID[StreamTypeToArrayIndex(InType)];
						// Cancel any ongoing segment download now.
						CancelPendingMediaSegmentRequests(InType);

						OutSelectionAttributes = *InPendingSelection;
						InOutSelectedAttributes.Select();

						FPendingSegmentRequest NextReq;
						NextReq.bStartOver = true;
						NextReq.StartoverPosition.Time = PlaybackState.GetPlayPosition();
						SetPlaystartOptions(NextReq.StartoverPosition.Options);
						NextReq.StreamType = InType;
						NextPendingSegmentRequests.Enqueue(MoveTemp(NextReq));

						bChangeMade = true;
					}
				}
				InOutSelectedAttributes.Select();
				InPendingSelection.Reset();
			}
		};

		PendingTrackSelectionVid.Reset();	// Ignore video changes for now.
		HandleDASHTrackSelection(StreamSelectionAttributesAud, SelectedStreamAttributesAud, EStreamType::Audio, PendingTrackSelectionAud, CurrentPlayPeriodAudio);
		HandleDASHTrackSelection(StreamSelectionAttributesTxt, SelectedStreamAttributesTxt, EStreamType::Subtitle, PendingTrackSelectionTxt, CurrentPlayPeriodText);
	}
	else
	{
		PendingTrackSelectionVid.Reset();
		PendingTrackSelectionAud.Reset();
		PendingTrackSelectionTxt.Reset();
	}

	// Check if a change was made when there are already future buffers enqueued in which case a switch is
	// no longer possible as we cannot switch on the current buffer.
	if (bChangeMade)
	{
		DataBuffersCriticalSection.Lock();
		bool bNeedStartover = (ActiveDataOutputBuffers.IsValid() && NextDataBuffers.Num() > 0) || NextDataBuffers.Num() > 1;
		DataBuffersCriticalSection.Unlock();
		if (bNeedStartover)
		{
			InternalStartoverAtCurrentPosition();
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Adds the given period to the list of periods playback will eventually move into
 * and a metadata change event may need to be generated.
 */
void FAdaptiveStreamingPlayer::AddUpcomingPeriod(TSharedPtrTS<IManifest::IPlayPeriod> InUpcomingPeriod)
{
	/*
		We maintain two lists of periods.
		 - ActivePeriods
		     holds all the periods for which segments are to be requested. As the play position moves forward
			 periods in this get removed when they have fallen out of range with a threshold.
			 This list is iterated when searching for codec information and selected buffer source information.
		 - UpcomingPeriods
		     this is used solely to report changes in metadata to the user as playback progresses.
			 entries here are removed immediately without threshold, so this list tends to be shorter than
			 ActivePeriods.
	*/

	TSharedPtrTS<ITimelineMediaAsset> Period = InUpcomingPeriod->GetMediaAsset();
	if (Period.IsValid())
	{
		FString PeriodID = Period->GetUniqueIdentifier();
		int64 LoopCountNow = CurrentLoopState.Count;

		// Check if this period is in the active and/or upcoming list already and update the
		// loop counter in the list if it is so it does not get removed when the play position
		// of an earlier loop iteration passes it.
		bool bHaveAsActive = false;
		ActivePeriodCriticalSection.Lock();
		for(auto& P : ActivePeriods)
		{
			if (P.ID.Equals(PeriodID))
			{
				bHaveAsActive = true;
				P.LoopCount = LoopCountNow;
			}
		}
		ActivePeriodCriticalSection.Unlock();

		bool bHaveAsUpcoming = false;
		for(auto& P : UpcomingPeriods)
		{
			if (P.ID.Equals(PeriodID))
			{
				bHaveAsUpcoming = true;
				P.LoopCount = LoopCountNow;
			}
		}

		FPeriodInformation Next;
		Next.ID = MoveTemp(PeriodID);
		Next.TimeRange = Period->GetTimeRange();
		Next.Period = MoveTemp(Period);
		Next.LoopCount = LoopCountNow;

		if (!bHaveAsActive)
		{
			// Tell the AEMS handler that a new period will be coming up. It needs this information to cut overlapping events.
			AEMSEventHandler->NewUpcomingPeriod(Next.ID, Next.TimeRange);

			ActivePeriodCriticalSection.Lock();
			ActivePeriods.Emplace(Next);
			ActivePeriods.Sort([](const FPeriodInformation& e1, const FPeriodInformation& e2)
			{
				return e1.TimeRange.Start < e2.TimeRange.Start;
			});
			ActivePeriodCriticalSection.Unlock();
		}

		if (!bHaveAsUpcoming)
		{
			UpcomingPeriods.Emplace(Next);
			UpcomingPeriods.Sort([](const FPeriodInformation& e1, const FPeriodInformation& e2)
			{
				return e1.TimeRange.Start < e2.TimeRange.Start;
			});
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Updates the active or upcoming period's selected buffer source information
 * for the given stream type.
 */
void FAdaptiveStreamingPlayer::UpdatePeriodStreamBufferSourceInfo(TSharedPtrTS<IManifest::IPlayPeriod> InForPeriod, EStreamType InStreamType, TSharedPtrTS<FBufferSourceInfo> InBufferSourceInfo)
{
	TSharedPtrTS<ITimelineMediaAsset> Period = InForPeriod->GetMediaAsset();
	if (Period.IsValid())
	{
		FString PeriodID = Period->GetUniqueIdentifier();

		auto UpdatePeriods = [&](TArray<FPeriodInformation>& Periods) -> void
		{
			for(auto &P : Periods)
			{
				if (P.ID.Equals(PeriodID))
				{
					switch(InStreamType)
					{
						case EStreamType::Video:
							P.BufferSourceInfoVid = InBufferSourceInfo;
							break;
						case EStreamType::Audio:
							P.BufferSourceInfoAud = InBufferSourceInfo;
							break;
						case EStreamType::Subtitle:
							P.BufferSourceInfoTxt = InBufferSourceInfo;
							break;
						default:
							break;
					}
				}
			}
		};

		// Check if we have to update upcoming periods
		UpdatePeriods(UpcomingPeriods);

		// Update the active periods if necessary.
		ActivePeriodCriticalSection.Lock();
		UpdatePeriods(ActivePeriods);
		ActivePeriodCriticalSection.Unlock();
	}
}



TSharedPtrTS<FBufferSourceInfo> FAdaptiveStreamingPlayer::GetStreamBufferInfoAtTime(bool& bOutHavePeriods, bool& bOutFoundTime, EStreamType InStreamType, const FTimeValue& InAtTime) const
{
	TSharedPtrTS<FBufferSourceInfo> BufferSourceInfo;
	// Locate the period for the specified time.
	ActivePeriodCriticalSection.Lock();
	bOutHavePeriods = ActivePeriods.Num() != 0;
	bOutFoundTime = false;
	for(int32 i=0; i<ActivePeriods.Num(); ++i)
	{
		if (InAtTime >= ActivePeriods[i].TimeRange.Start && InAtTime < (ActivePeriods[i].TimeRange.End.IsValid() ? ActivePeriods[i].TimeRange.End : FTimeValue::GetPositiveInfinity()))
		{
			bOutFoundTime = true;
			switch(InStreamType)
			{
				case EStreamType::Video:
					BufferSourceInfo = ActivePeriods[i].BufferSourceInfoVid;
					break;
				case EStreamType::Audio:
					BufferSourceInfo = ActivePeriods[i].BufferSourceInfoAud;
					break;
				case EStreamType::Subtitle:
					BufferSourceInfo = ActivePeriods[i].BufferSourceInfoTxt;
					break;
				default:
					break;
			}
		}
	}
	ActivePeriodCriticalSection.Unlock();
	return BufferSourceInfo;
}


void FAdaptiveStreamingPlayer::SetPlaystartOptions(FPlayStartOptions& OutOptions)
{
	FTimeValue RangeStart = GetOptions().GetValue(OptionPlayRangeStart).SafeGetTimeValue(FTimeValue());
	FTimeValue RangeEnd = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());

	OutOptions.PlaybackRange.Start = RangeStart.IsValid() ? RangeStart : FTimeValue::GetZero();
	OutOptions.PlaybackRange.End = RangeEnd.IsValid() ? RangeEnd : FTimeValue::GetPositiveInfinity();

	OutOptions.bFrameAccuracy = GetOptions().GetValue(OptionKeyFrameAccurateSeek).SafeGetBool(false);
}

void FAdaptiveStreamingPlayer::ClampStartRequestTime(FTimeValue& InOutTimeToClamp)
{
	if (PendingStartRequest.IsValid())
	{
		// Clamp to within any set playback range
		FTimeValue RangeStart = PendingStartRequest->StartAt.Options.PlaybackRange.Start;
		FTimeValue RangeEnd = PendingStartRequest->StartAt.Options.PlaybackRange.End;
		if (InOutTimeToClamp < RangeStart)
		{
			InOutTimeToClamp = RangeStart;
		}
		if (InOutTimeToClamp > RangeEnd)
		{
			InOutTimeToClamp = RangeEnd;
			// Going to the end will of course result in an 'end reached' and end of playback.
			// If the player is set to loop then we should start from the beginning.
			if (CurrentLoopParam.bEnableLooping)
			{
				InOutTimeToClamp = RangeStart;
			}
		}
	}
}

FTimeValue FAdaptiveStreamingPlayer::ClampTimeToCurrentRange(const FTimeValue& InTime, bool bClampToStart, bool bClampToEnd)
{
	FTimeValue Out(InTime);

	FTimeRange TimelineRange;
	PlaybackState.GetTimelineRange(TimelineRange);
	if (bClampToEnd)
	{
		FTimeValue RangeEnd = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());
		if (RangeEnd.IsValid() && RangeEnd < TimelineRange.End)
		{
			TimelineRange.End = RangeEnd;
		}
		if (Out > TimelineRange.End)
		{
			Out = TimelineRange.End;
		}
	}
	if (bClampToStart)
	{
		FTimeValue RangeStart = GetOptions().GetValue(OptionPlayRangeStart).SafeGetTimeValue(FTimeValue());
		if (RangeStart.IsValid() && RangeStart > TimelineRange.Start)
		{
			TimelineRange.Start = RangeStart;
		}
		if (Out < TimelineRange.Start)
		{
			Out = TimelineRange.Start;
		}
	}
	return Out;
}



//-----------------------------------------------------------------------------
/**
 * Checks the error collectors for any errors thrown by the decoders.
 */
void FAdaptiveStreamingPlayer::CheckForErrors()
{
	while(!ErrorQueue.IsEmpty())
	{
		TSharedPtrTS<FErrorDetail> Error = ErrorQueue.Pop();
		// Do this only once in case there will be several decoder errors thrown.
		if (CurrentState != EPlayerState::eState_Error)
		{
			// Pause before setting up error.
			InternalPause();

			CurrentState = EPlayerState::eState_Error;
			// Only keep the first error, not any errors after that which may just be the avalanche and not the cause.
			if (!LastErrorDetail.IsSet())
			{
				DiagnosticsCriticalSection.Lock();
				LastErrorDetail = *Error;
				DiagnosticsCriticalSection.Unlock();
				DispatchEvent(FMetricEvent::ReportError(LastErrorDetail));
			}
			// In error state we do not need any periodic manifest refetches any more.
			InternalCloseManifestReader();
		}
	}
}



//-----------------------------------------------------------------------------
/**
 * Updates the data availability state if it has changed and dispatches the
 * corresponding metrics event.
 *
 * @param DataAvailabilityState
 * @param NewAvailability
 */
void FAdaptiveStreamingPlayer::UpdateDataAvailabilityState(Metrics::FDataAvailabilityChange& DataAvailabilityState, Metrics::FDataAvailabilityChange::EAvailability NewAvailability)
{
	if (DataAvailabilityState.Availability != NewAvailability)
	{
		DataAvailabilityState.Availability = NewAvailability;
		DispatchEvent(FMetricEvent::ReportDataAvailabilityChange(DataAvailabilityState));
	}
}


//-----------------------------------------------------------------------------
/**
 * Updates the ABR and video decoder with maximum stream resolution limits.
 */
void FAdaptiveStreamingPlayer::UpdateStreamResolutionLimit()
{
	StreamSelector->SetMaxVideoResolution(VideoResolutionLimitWidth, VideoResolutionLimitHeight);
	VideoDecoder.bApplyNewLimits = true;
}


//-----------------------------------------------------------------------------
/**
 * Checks if the stream readers, decoders and renderers are finished.
 */
void FAdaptiveStreamingPlayer::CheckForStreamEnd()
{
	if (CurrentState == EPlayerState::eState_Playing)
	{
		if (StreamState == EStreamState::eStream_Running)
		{
			// First check if there is an end time set at which we need to stop.
			FTimeValue EndAtTime = PlaybackState.GetPlaybackEndAtTime();
			if (EndAtTime.IsValid())
			{
				if (PlaybackState.GetPlayPosition() >= EndAtTime)
				{
					// A forced end time is intended to stop playback. We do NOT look at the loop state here.
					InternalSetPlaybackEnded();
					DataBuffersCriticalSection.Lock();
					NextDataBuffers.Empty();
					DataBuffersCriticalSection.Unlock();
					return;
				}
			}

			DiagnosticsCriticalSection.Lock();
			FBufferStats vidStats = VideoBufferStats;
			FBufferStats audStats = AudioBufferStats;
			DiagnosticsCriticalSection.Unlock();

			// As long as no buffer is at EOD we don't need to check further
			if (!vidStats.StreamBuffer.bEndOfData && !audStats.StreamBuffer.bEndOfData)
			{
				return;
			}

			// Look at the last period's buffer source infos to see what streams we are expecting to be currently active.
			bool bHaveVid = false;
			bool bHaveAud = false;
			bool bHavetxt = false;
			ActivePeriodCriticalSection.Lock();
			if (ActivePeriods.Num())
			{
				bHaveVid = ActivePeriods.Last().BufferSourceInfoVid.IsValid();
				bHaveAud = ActivePeriods.Last().BufferSourceInfoAud.IsValid();
				bHavetxt = ActivePeriods.Last().BufferSourceInfoTxt.IsValid();
			}
			ActivePeriodCriticalSection.Unlock();

			// For simplicity we assume all streams are done and check for those that are active at the moment.
			bool bEndVid = true;
			bool bEndAud = true;
			bool bEndTxt = true;
			int64 VidStalled = 0;
			int64 AudStalled = 0;

			// Check for end of video stream
			if (bHaveVid)
			{
				// Source buffer and decoder input buffer at end of data?
				// We do not check the decoder output buffer since it is possible that no decodable output was produced
				// which tends to happen at the end of the stream or when there is no video any more when audio is longer.
				bEndVid = (vidStats.StreamBuffer.bEndOfData && vidStats.DecoderInputBuffer.bEODSignaled && vidStats.DecoderInputBuffer.bEODReached);
				VidStalled = vidStats.StreamBuffer.bEndOfData ? vidStats.GetStalledDurationMillisec() : 0;
				if (bEndVid)
				{
					UpdateDataAvailabilityState(DataAvailabilityStateVid, Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
				}
			}

			// Check for end of audio stream
			if (bHaveAud)
			{
				// Source buffer and decoder input buffer at end of data?
				// We do not check the decoder output buffer since it is possible that no decodable output was produced
				// which tends to happen at the end of the stream or when there is no audio any more when video is longer.
				bEndAud = (audStats.StreamBuffer.bEndOfData && audStats.DecoderInputBuffer.bEODSignaled && audStats.DecoderInputBuffer.bEODReached);
				AudStalled = audStats.StreamBuffer.bEndOfData ? audStats.GetStalledDurationMillisec() : 0;
				if (bEndAud)
				{
					UpdateDataAvailabilityState(DataAvailabilityStateAud, Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
				}
			}

			// Text stream
			// ...


			// If either primary stream has reliably ended do a check if the other may have stalled because the application is no longer
			// consuming decoder output, which will prevent the other stream from ending.
			if ((bHaveVid && bHaveAud) && (bEndVid || bEndAud))
			{
				int64 OtherStallTime = bEndAud ? VidStalled : AudStalled;
				if (OtherStallTime > 500)
				{
					bEndVid = bEndAud = true;
				}
			}

			// Everything at EOD?
			if (bEndVid && bEndAud && bEndTxt)
			{
				// When not looping we are done.
				if (!CurrentLoopParam.bEnableLooping)
				{
					InternalSetPlaybackEnded();
					DataBuffersCriticalSection.Lock();
					NextDataBuffers.Empty();
					DataBuffersCriticalSection.Unlock();
				}
				else
				{
					// With loop data the decoders will receive more data. Clear their end of data states.
					ClearAllDecoderEODs();
					// There needs to be another data buffer with the data at the loop point.
					DataBuffersCriticalSection.Lock();
					if (NextDataBuffers.Num())
					{
						TSharedPtrTS<FStreamDataBuffers> NewOutputBuffers;
						NewOutputBuffers = NextDataBuffers[0];
						NextDataBuffers.RemoveAt(0);
						DataBuffersCriticalSection.Unlock();

						// Check if the video stream is seamlessly decodable (next AU is a keyframe and it will also be rendered and not cut off)
						// Note: at this point we assume audio is always decodable on every AU.
						bool bSeamlessSwitchPossible = IsSeamlessBufferSwitchPossible(EStreamType::Video, NewOutputBuffers);
						if (!bSeamlessSwitchPossible)
						{
							StopRendering();
							PipelineState = EPipelineState::ePipeline_Prerolling;
							PrerollVars.Clear();
							PrerollVars.bIsMidSequencePreroll = true;
							PrerollVars.StartTime = MEDIAutcTime::CurrentMSec();
							PostrollVars.Clear();
						}

						DataBuffersCriticalSection.Lock();
						ActiveDataOutputBuffers = MoveTemp(NewOutputBuffers);
						DataBuffersCriticalSection.Unlock();

						// Update the current loop state.
						FInternalLoopState newLoopState;
						if (NextLoopStates.Peek(newLoopState))
						{
							NextLoopStates.Pop();
							PlaybackState.SetLoopState(newLoopState);
							DispatchEvent(FMetricEvent::ReportJumpInPlayPosition(newLoopState.To, newLoopState.From, Metrics::ETimeJumpReason::Looping));
						}
						// Update the metadata state again.
						MetadataHandlingState.Reset();
					}
					else
					{
						DataBuffersCriticalSection.Unlock();
					}
					// Update the buffer statistics immediately to reflect the end of data states.
					UpdateDiagnostics();
				}
			}
		}
		else if (StreamState == EStreamState::eStream_ReachedEnd)
		{
			// ... anything special we could still do here?
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Finds fragments for a given time, creates the stream readers
 * and issues the first fragment request.
 *
 * @param NewPosition
 */
void FAdaptiveStreamingPlayer::InternalStartAt(const FSeekParam& NewPosition)
{
	uint32 NextPlaybackID = Utils::Max(Utils::Max(CurrentPlaybackSequenceID[0], CurrentPlaybackSequenceID[1]), CurrentPlaybackSequenceID[2]) + 1;
	CurrentPlaybackSequenceID[0] = NextPlaybackID;
	CurrentPlaybackSequenceID[1] = NextPlaybackID;
	CurrentPlaybackSequenceID[2] = NextPlaybackID;

	StreamState = EStreamState::eStream_Running;

	PlaybackState.SetHasEnded(false);
	PlaybackState.SetPlayRangeHasChanged(false);
	PlaybackState.SetLoopStateHasChanged(false);
	PlaybackState.SetEncoderLatency(FTimeValue::GetInvalid());

	VideoBufferStats.Clear();
	AudioBufferStats.Clear();
	TextBufferStats.Clear();

	// Update data availability states in case this wasn't done yet.
	UpdateDataAvailabilityState(DataAvailabilityStateVid, Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
	UpdateDataAvailabilityState(DataAvailabilityStateAud, Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);
	UpdateDataAvailabilityState(DataAvailabilityStateTxt, Metrics::FDataAvailabilityChange::EAvailability::DataNotAvailable);

	// (Re-)configure the stream selector
	StreamSelector->SetBandwidthCeiling(BitrateCeiling);
	StreamSelector->SetMaxVideoResolution(VideoResolutionLimitWidth, VideoResolutionLimitHeight);

	// Create the stream reader
	check(StreamReaderHandler == nullptr);
	StreamReaderHandler = Manifest->CreateStreamReaderHandler();
	check(StreamReaderHandler);
	IStreamReader::CreateParam srhParam;
	srhParam.EventListener  = this;
	srhParam.MemoryProvider = this;
	if (StreamReaderHandler->Create(this, srhParam) != UEMEDIA_ERROR_OK)
	{
		FErrorDetail err;
		err.SetFacility(Facility::EFacility::Player);
		err.SetMessage("Failed to create stream reader");
		err.SetCode(INTERR_CREATE_FRAGMENT_READER);
		PostError(err);
		return;
	}

	check(!PendingStartRequest.IsValid());
	check(!InitialPlayPeriod.IsValid());
	check(!PendingFirstSegmentRequest.IsValid());
	check(NextPendingSegmentRequests.IsEmpty());
	check(ReadyWaitingSegmentRequests.IsEmpty());
	check(CompletedSegmentRequests.Num() == 0);
	NextPendingSegmentRequests.Empty();
	ReadyWaitingSegmentRequests.Empty();
	PendingFirstSegmentRequest.Reset();
	CompletedSegmentRequests.Empty();
	InitialPlayPeriod.Reset();
	CurrentPlayPeriodVideo.Reset();
	CurrentPlayPeriodAudio.Reset();
	CurrentPlayPeriodText.Reset();
	PendingStartRequest = MakeSharedTS<FPendingStartRequest>();
	PendingStartRequest->SearchType = IManifest::ESearchType::Closest;
	PendingStartRequest->StartAt.Time = NewPosition.Time;
	PendingStartRequest->StartingBitrate = NewPosition.StartingBitrate;
	PendingStartRequest->StartType = SeekVars.bIsPlayStart ? FPendingStartRequest::EStartType::PlayStart : FPendingStartRequest::EStartType::Seeking;
	SetPlaystartOptions(PendingStartRequest->StartAt.Options);

	// The fragment should be the closest to the time stamp unless we are rebuffering in which case it should be the one we failed on.
/*
	if (mLastBufferingState == eState_Rebuffering)
	{
		mpPendingStartRequest->SearchType = IManifest::ESearchType::Before;
	}
	else
*/
	{
		PendingStartRequest->SearchType = IManifest::ESearchType::Closest;
	}
	bFirstSegmentRequestIsForLooping = false;
	bStreamingHasStarted = false;

	// Let the AEMS handler know that we are starting up.
	AEMSEventHandler->PlaybackStartingUp();

	// From this point on any further start will not be the very first one any more.
	SeekVars.bIsPlayStart = false;

	// Starting with a clean slate. Fire this event synchronously directly to the receivers.
	// This is intended to release any upper layer blocks of passing decoded output to sinks
	// that may be in place to prevent receiving decoder output that may be "stale" from the
	// perspective of the upper layer.
	// It is IMPERATIVE for the receiver of this event to do NO BLOCKING tasks OR call in
	// any of the player API!
	FireSyncEvent(FMetricEvent::ReportCleanStart());
}


void FAdaptiveStreamingPlayer::InternalSetPlaybackEnded()
{
	PostrollVars.Clear();

	FTimeRange TimelineRange;
	PlaybackState.GetTimelineRange(TimelineRange);
	FTimeValue RangeEnd = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());
	if (RangeEnd.IsValid() && RangeEnd < TimelineRange.End)
	{
		TimelineRange.End = RangeEnd;
	}
	PlaybackState.SetPlayPosition(TimelineRange.End);

	if (LastBufferingState == EPlayerState::eState_Seeking)
	{
		DispatchEvent(FMetricEvent::ReportSeekCompleted());
	}
	SeekVars.Reset();

	PlaybackState.SetHasEnded(true);
	PlaybackState.SetShouldPlayOnLiveEdge(false);
	StreamState = EStreamState::eStream_ReachedEnd;

	// Go to pause mode now.
	InternalPause();
	DispatchEvent(FMetricEvent::ReportPlaybackEnded());
	// In case a seek back into the stream will happen we reset the first start state.
	PrerollVars.bIsVeryFirstStart = true;
	// Let the AEMS handler know as well.
	AEMSEventHandler->PlaybackReachedEnd(TimelineRange.End);
}


//-----------------------------------------------------------------------------
/**
 * Pauses playback.
 */
void FAdaptiveStreamingPlayer::InternalPause()
{
	// Pause decoders to stop feeding them data.
	// NOTE: A decoder may be in InputNeeded() / FeedDecoder() at this point!
	//       This is to prevent their next ask for new data.
	DecoderState = EDecoderState::eDecoder_Paused;
	SubtitleDecoder.Stop();

	// Stop rendering.
	StopRendering();
	PipelineState = EPipelineState::ePipeline_Stopped;

	// Pause playing.
	if (CurrentState != EPlayerState::eState_Error)
	{
		PlaybackRate = 0.0;
		CurrentState = EPlayerState::eState_Paused;
		PlaybackState.SetPausedAndPlaying(true, false);
		DispatchEvent(FMetricEvent::ReportPlaybackPaused());
	}
}


//-----------------------------------------------------------------------------
/**
 * Resumes playback.
 */
void FAdaptiveStreamingPlayer::InternalResume()
{
	// Cannot resume when in error state or when stream has reached the end.
	if (CurrentState != EPlayerState::eState_Error && StreamState != EStreamState::eStream_ReachedEnd)
	{
		// Start rendering.
		check(PipelineState == EPipelineState::ePipeline_Stopped);
		StartRendering();
		PipelineState = EPipelineState::ePipeline_Running;

		// Resume decoders.
		DecoderState = EDecoderState::eDecoder_Running;
		SubtitleDecoder.Start();

		if (PrerollVars.bIsVeryFirstStart)
		{
			PrerollVars.bIsVeryFirstStart = false;
			// Send play start event
			DispatchEvent(FMetricEvent::ReportPlaybackStart());
		}

		// Resume playing.
		CurrentState = EPlayerState::eState_Playing;
		PlaybackRate = 1.0;
		PlaybackState.SetPausedAndPlaying(false, true);
		PostrollVars.Clear();
		DispatchEvent(FMetricEvent::ReportPlaybackResumed());
	}
}





void FAdaptiveStreamingPlayer::InternalStop(bool bHoldCurrentFrame)
{
	// Increase the playback sequence ID. This will cause all async segment request message
	// to be discarded when they are received.
	++CurrentPlaybackSequenceID[0];
	++CurrentPlaybackSequenceID[1];
	++CurrentPlaybackSequenceID[2];

	// Pause decoders to stop feeding them data.
	// NOTE: A decoder may be in InputNeeded() / FeedDecoder() at this point!
	//       This is to prevent their next ask for new data.
	DecoderState = EDecoderState::eDecoder_Paused;

	// Stop rendering.
	StopRendering();
	PipelineState = EPipelineState::ePipeline_Stopped;

	// Destroy the stream reader handler.
	IStreamReader* CurrentStreamReader = TMediaInterlockedExchangePointer(StreamReaderHandler, (IStreamReader*)nullptr);
	if (CurrentStreamReader)
	{
		CurrentStreamReader->Close();
	}
	delete CurrentStreamReader;
	bStreamingHasStarted = false;

	// Release any pending segment requests.
	PendingStartRequest.Reset();
	PendingFirstSegmentRequest.Reset();
	NextPendingSegmentRequests.Empty();
	ReadyWaitingSegmentRequests.Empty();
	CompletedSegmentRequests.Empty();
	InitialPlayPeriod.Reset();
	CurrentPlayPeriodVideo.Reset();
	CurrentPlayPeriodAudio.Reset();
	CurrentPlayPeriodText.Reset();
	ActivePeriodCriticalSection.Lock();
	ActivePeriods.Empty();
	ActivePeriodCriticalSection.Unlock();
	UpcomingPeriods.Empty();
	MetadataHandlingState.Reset();
	NextLoopStates.Empty();
	SeekVars.Reset();

	PlaybackRate = 0.0;
	CurrentState = EPlayerState::eState_Paused;
	PlaybackState.SetPausedAndPlaying(false, false);
	PlaybackState.SetPlaybackEndAtTime(FTimeValue::GetInvalid());

	// Flush all access unit buffers.
	DataBuffersCriticalSection.Lock();
	CurrentDataReceiveBuffers.Reset();
	NextDataBuffers.Empty();
	ActiveDataOutputBuffers.Reset();
	DataBuffersCriticalSection.Unlock();

	// Flush the renderers once before flushing the decoders.
	// In case the media samples hold references to the decoder (because of shared frame resources) it could be a possibility
	// that the decoder cannot flush as long as those references are "out there".
	AudioRender.Flush();
	VideoRender.Flush(bHoldCurrentFrame);

	// Flush the decoders.
	AudioDecoder.Flush();
	VideoDecoder.Flush();
	SubtitleDecoder.Flush();
	// Decoders are no longer at end of data now.
	ClearAllDecoderEODs();

	// Flush the renderers again, this time to discard everything the decoders may have emitted while being flushed.
	AudioRender.Flush();
	VideoRender.Flush(bHoldCurrentFrame);

	// Flush dynamic events from the AEMS handler.
	AEMSEventHandler->FlushDynamic();
}


void FAdaptiveStreamingPlayer::InternalInitialize()
{
	// Get the HTTP manager. This is a shared instance for all players.
	HttpManager = IElectraHttpManager::Create();

	// Create the DRM manager.
	DrmManager = FDRMManager::Create(this);

	// Create an entity cache.
	EntityCache = IPlayerEntityCache::Create(this, PlayerOptions);

	// Create an HTTP response cache.
	HttpResponseCache = IHTTPResponseCache::Create(this, PlayerOptions);

	// Create the ABR stream selector.
	StreamSelector = IAdaptiveStreamSelector::Create(this, this);
	AddMetricsReceiver(StreamSelector.Get());

	// Create renderers.
	CreateRenderers();

	// Set up video decoder resolution limits. As the media playlists are parsed the video streams will be
	// compared against these limits and those that exceed the limit will not be considered for playback.

	// Maximum allowed vertical resolution specified?
	if (PlayerOptions.HaveKey(TEXT("max_resoY")))
	{
		PlayerConfig.H264LimitUpto30fps.MaxResolution.Height = (int32)PlayerOptions.GetValue(TEXT("max_resoY")).GetInt64();
		PlayerConfig.H264LimitAbove30fps.MaxResolution.Height = (int32)PlayerOptions.GetValue(TEXT("max_resoY")).GetInt64();
	}
	// A limit in vertical resolution for streams with more than 30fps?
	if (PlayerOptions.HaveKey(TEXT("max_resoY_above_30fps")))
	{
		PlayerConfig.H264LimitAbove30fps.MaxResolution.Height = (int32)PlayerOptions.GetValue(TEXT("max_resoY_above_30fps")).GetInt64();
	}
	// Note: We could add additional limits here if need be.
	//       Eventually these need to be differentiated based on codec as well.

	// Get global video decoder capabilities and if supported, set the stream resolution limit accordingly.
	IVideoDecoderH264::FStreamDecodeCapability Capability, StreamParam;
	// Do a one-time global capability check with a default-empty stream param structure.
	// This then gets used in the individual stream capability checks.
	if (IVideoDecoderH264::GetStreamDecodeCapability(Capability, StreamParam))
	{
		if (Capability.Profile && Capability.Level)
		{
			PlayerConfig.H264LimitUpto30fps.MaxTierProfileLevel.Profile = Capability.Profile;
			PlayerConfig.H264LimitUpto30fps.MaxTierProfileLevel.Level = Capability.Level;
			PlayerConfig.H264LimitAbove30fps.MaxTierProfileLevel.Profile = Capability.Profile;
			PlayerConfig.H264LimitAbove30fps.MaxTierProfileLevel.Level = Capability.Level;
		}
		if (Capability.Height)
		{
			PlayerConfig.H264LimitUpto30fps.MaxResolution.Height = Utils::Min(PlayerConfig.H264LimitUpto30fps.MaxResolution.Height, Capability.Height);
			PlayerConfig.H264LimitAbove30fps.MaxResolution.Height = Utils::Min(PlayerConfig.H264LimitAbove30fps.MaxResolution.Height, Capability.Height);
		}

		// If this is software decoding only and there is limit for this in place on Windows, apply it.
		if (PlayerOptions.HaveKey(TEXT("max_resoY_windows_software")) && Capability.DecoderSupportType == IVideoDecoderH264::FStreamDecodeCapability::ESupported::SoftwareOnly)
		{
			int32 MaxWinSWHeight = (int32)PlayerOptions.GetValue(TEXT("max_resoY_windows_software")).GetInt64();
			PlayerConfig.H264LimitUpto30fps.MaxResolution.Height = Utils::Min(PlayerConfig.H264LimitUpto30fps.MaxResolution.Height, MaxWinSWHeight);
			PlayerConfig.H264LimitAbove30fps.MaxResolution.Height = Utils::Min(PlayerConfig.H264LimitAbove30fps.MaxResolution.Height, MaxWinSWHeight);
		}

		// If the maximum fps is only up to 30 fps set the resolution for streams above 30fps so small
		// that they will get rejected.
		if (Capability.FPS > 0.0 && Capability.FPS <= 30.0)
		{
			PlayerConfig.H264LimitAbove30fps.MaxResolution.Height = 16;
			PlayerConfig.H264LimitAbove30fps.MaxTierProfileLevel.Profile = 66;
			PlayerConfig.H264LimitAbove30fps.MaxTierProfileLevel.Level = 10;
		}
	}

	// Check for codecs that are not to be used as per the user's choice, even if the device supports them.
	auto GetExcludedCodecPrefixes = [](TArray<FString>& OutList, const FParamDict& InOptions, const FString& InKey) -> void
	{
		if (InOptions.HaveKey(InKey))
		{
			const TCHAR* const CommaDelimiter = TEXT(",");
			TArray<FString> Prefixes;
			FString Value = InOptions.GetValue(InKey).GetFString();
			Value.TrimQuotesInline();
			Value.ParseIntoArray(Prefixes, CommaDelimiter, true);
			for (auto& Prefix : Prefixes)
			{
				OutList.Emplace(Prefix.TrimStartAndEnd().TrimQuotes().TrimStartAndEnd());
			}
		}
	};
	GetExcludedCodecPrefixes(ExcludedVideoDecoderPrefixes, PlayerOptions, TEXT("excluded_codecs_video"));
	GetExcludedCodecPrefixes(ExcludedAudioDecoderPrefixes, PlayerOptions, TEXT("excluded_codecs_audio"));
	GetExcludedCodecPrefixes(ExcludedSubtitleDecoderPrefixes, PlayerOptions, TEXT("excluded_codecs_subtitles"));

	auto GetCodecSelectionPriorities = [this](FCodecSelectionPriorities& OutPriorities, const FParamDict& InOptions, const FString& InKey, const TCHAR* const InType) -> void
	{
		if (InOptions.HaveKey(InKey))
		{
			if (!OutPriorities.Initialize(InOptions.GetValue(InKey).GetFString()))
			{
				PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Warning, FString::Printf(TEXT("Failed to parse %s codec selection priority configuration"), InType));
			}
		}
	};
	GetCodecSelectionPriorities(CodecPrioritiesVideo, PlayerOptions, TEXT("preferred_codecs_video"), TEXT("video"));
	GetCodecSelectionPriorities(CodecPrioritiesAudio, PlayerOptions, TEXT("preferred_codecs_audio"), TEXT("audio"));
	GetCodecSelectionPriorities(CodecPrioritiesSubtitles, PlayerOptions, TEXT("preferred_codecs_subtitles"), TEXT("subtitle"));


	// Unless already specified in the options to either value, enable frame accurate seeking now.
	if (!PlayerOptions.HaveKey(OptionKeyFrameAccurateSeek))
	{
		PlayerOptions.Set(OptionKeyFrameAccurateSeek, FVariantValue(true));
	}
}


void FAdaptiveStreamingPlayer::InternalClose()
{
	// No longer need the manifest reader/updater
	InternalCancelLoadManifest();
	InternalCloseManifestReader();

	DestroyDecoders();
	DestroyRenderers();

	// Do not clear the playback state to allow GetPlayPosition() to query the last play position.
	// This also means queries like IsBuffering() will return the last state but calling those
	// after a Stop() can be considered at least weird practice.
	//PlaybackState.Reset();
	Manifest.Reset();

	if (DrmManager.IsValid())
	{
		DrmManager->Close();
		DrmManager.Reset();
	}

	RemoveMetricsReceiver(StreamSelector.Get());
	StreamSelector.Reset();

	HttpManager.Reset();
	EntityCache.Reset();
	HttpResponseCache.Reset();

	// Reset remaining internal state
	CurrentState   	= EPlayerState::eState_Idle;
	PlaybackRate   	= 0.0;
	StreamState		= EStreamState::eStream_Running;
	RebufferCause = ERebufferCause::None;
	LastBufferingState = EPlayerState::eState_Buffering;
	RebufferDetectedAtPlayPos.SetToInvalid();
	PrerollVars.Clear();
	PostrollVars.Clear();

	bShouldBePaused = false;
	bShouldBePlaying = false;
	CurrentLoopState = {};
	NextLoopStates.Empty();

	CurrentPlaybackSequenceState.Reset();

	PendingTrackSelectionVid.Reset();
	PendingTrackSelectionAud.Reset();
	PendingTrackSelectionTxt.Reset();

	// Flush all events from the AEMS handler
	AEMSEventHandler->FlushEverything();

	// Clear error state.
	LastErrorDetail.Clear();
	ErrorQueue.Clear();

	// Clear diagnostics.
	DiagnosticsCriticalSection.Lock();
	VideoBufferStats.Clear();
	AudioBufferStats.Clear();
	TextBufferStats.Clear();
	DiagnosticsCriticalSection.Unlock();
}


void FAdaptiveStreamingPlayer::InternalSetLoop(const FLoopParam& LoopParam)
{
	// Looping only makes sense for on demand presentations since Live does not have an end to loop at.
	// In case this is called that early on that we can't determine the presentation type yet we allow
	// looping to be set. It won't get evaluated anyway since the Live presentation doesn't end.
	// The only issue with this is that until a manifest is loaded querying loop enable will return true.
	if (!Manifest || Manifest->GetPresentationType() == IManifest::EType::OnDemand)
	{
		bool bStartOver = false;
		// When looping is to be enabled we need to check if we can do this without having to start over.
		if (!CurrentLoopParam.bEnableLooping && LoopParam.bEnableLooping)
		{
			// If there are any completed segment requests the loop state will be checked when all reach
			// the end. Otherwise, if any of the receive buffers has the end-of-data flag set that
			// that streams reached the end and will not loop, so we need to start over.
			if (CompletedSegmentRequests.Num() == 0)
			{
				TSharedPtrTS<FMultiTrackAccessUnitBuffer> RcvBuffer;
				bool bAnyReceiveBufferAtEOD = false;
				RcvBuffer = GetCurrentReceiveStreamBuffer(EStreamType::Video);
				bAnyReceiveBufferAtEOD |= RcvBuffer.IsValid() ? RcvBuffer->IsEODFlagSet() || RcvBuffer->IsEndOfTrack() : false;
				RcvBuffer = GetCurrentReceiveStreamBuffer(EStreamType::Audio);
				bAnyReceiveBufferAtEOD |= RcvBuffer.IsValid() ? RcvBuffer->IsEODFlagSet() || RcvBuffer->IsEndOfTrack() : false;
				RcvBuffer = GetCurrentReceiveStreamBuffer(EStreamType::Subtitle);
				bAnyReceiveBufferAtEOD |= RcvBuffer.IsValid() ? RcvBuffer->IsEODFlagSet() || RcvBuffer->IsEndOfTrack() : false;

				bStartOver = bAnyReceiveBufferAtEOD;
			}
		}
		CurrentLoopParam = LoopParam;
		// For the lack of better knowledge update the current state immediately.
		CurrentLoopState.bIsEnabled = LoopParam.bEnableLooping;
		PlaybackState.SetLoopStateEnable(LoopParam.bEnableLooping);
		if (bStartOver)
		{
			PlaybackState.SetLoopStateHasChanged(true);
		}
	}
}



//-----------------------------------------------------------------------------
/**
 * Rebuffers at the current play position.
 */
void FAdaptiveStreamingPlayer::InternalRebuffer()
{
	IAdaptiveStreamSelector::FRebufferAction Action;
	check(StreamSelector.IsValid());
	if (StreamSelector.IsValid())
	{
		Action = StreamSelector->GetRebufferAction(GetOptions());
	}

	// Check if we are configured to throw a playback error instead of doing a rebuffer.
	// This can be used by a higher logic to force playback at a different position instead.
	if (Action.Action == IAdaptiveStreamSelector::FRebufferAction::EAction::ThrowError)
	{
		FErrorDetail err;
		err.SetFacility(Facility::EFacility::Player);
		err.SetMessage("Rebuffering is set to generate a playback error");
		err.SetCode(INTERR_REBUFFER_SHALL_THROW_ERROR);
		PostError(err);
	}
	else
	{
		// Pause decoders to stop feeding them data.
		// NOTE: A decoder may be in InputNeeded() / FeedDecoder() at this point!
		//       This is to prevent their next ask for new data.
		DecoderState = EDecoderState::eDecoder_Paused;

		// Stop rendering.
		StopRendering();
		PipelineState = EPipelineState::ePipeline_Stopped;

		CurrentState = EPlayerState::eState_Rebuffering;
		LastBufferingState = EPlayerState::eState_Rebuffering;

		// If rebuffering triggered while switching tracks then the spacing of sync frames may be too big and we
		// have none to switch over to the new track yet. To overcome this we restart at the current position.
		if (RebufferCause == ERebufferCause::TrackswitchUnderrun && Action.Action == IAdaptiveStreamSelector::FRebufferAction::EAction::ContinueLoading)
		{
			Action.Action = IAdaptiveStreamSelector::FRebufferAction::EAction::Restart;
			PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Info, FString::Printf(TEXT("Rebuffering during track change switches rebuffering policy from continue to restart.")));
		}
		RebufferCause = ERebufferCause::None;

		// Check if we should just continue loading data
		if (Action.Action == IAdaptiveStreamSelector::FRebufferAction::EAction::ContinueLoading)
		{
			RebufferDetectedAtPlayPos.SetToInvalid();
			PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Info, FString::Printf(TEXT("Rebuffering is waiting for new data.")));
			DispatchBufferingEvent(true, CurrentState);
			CurrentState = EPlayerState::eState_Buffering;
		}
		else
		{
			// We do not wait on the current segment that caused the rebuffering to complete and start over.
			FSeekParam StartAtTime;
			if (Action.Action == IAdaptiveStreamSelector::FRebufferAction::EAction::GoToLive)
			{
				StartAtTime.Time.SetToInvalid();
				PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Info, FString::Printf(TEXT("Rebuffering jumps to the live edge")));
			}
			else
			{
				if (!RebufferDetectedAtPlayPos.IsValid())
				{
					RebufferDetectedAtPlayPos = PlaybackState.GetPlayPosition();
				}
				StartAtTime.Time = RebufferDetectedAtPlayPos;
				PostLog(Facility::EFacility::Player, IInfoLog::ELevel::Info, FString::Printf(TEXT("Rebuffering starts over at current position")));
			}
			RebufferDetectedAtPlayPos.SetToInvalid();
			InternalStop(PlayerConfig.bHoldLastFrameDuringSeek);
			CurrentState = EPlayerState::eState_Rebuffering;
			if (Action.SuggestedRestartBitrate > 0)
			{
				StartAtTime.StartingBitrate = Action.SuggestedRestartBitrate;
			}

			InternalStartAt(StartAtTime);
		}
	}
}








FTimeValue FAdaptiveStreamingPlayer::ABRGetPlayPosition() const
{
	return PlaybackState.GetPlayPosition();
}
FTimeRange FAdaptiveStreamingPlayer::ABRGetTimeline() const
{
	FTimeRange Timeline;
	PlaybackState.GetTimelineRange(Timeline);
	return Timeline;
}
FTimeValue FAdaptiveStreamingPlayer::ABRGetWallclockTime() const
{
	return SynchronizedUTCTime->GetTime();
}
bool FAdaptiveStreamingPlayer::ABRIsLive() const
{
	TSharedPtrTS<IManifest> Current = Manifest;
	return Current.IsValid() ? Current->GetPresentationType() == IManifest::EType::Live : false;
}
bool FAdaptiveStreamingPlayer::ABRShouldPlayOnLiveEdge() const
{
	return PlaybackState.GetShouldPlayOnLiveEdge();
}
TSharedPtrTS<const FLowLatencyDescriptor> FAdaptiveStreamingPlayer::ABRGetLowLatencyDescriptor() const
{
	TSharedPtrTS<IManifest> Current = Manifest;
	return Current.IsValid() ? Current->GetLowLatencyDescriptor() : nullptr;
}
FTimeValue FAdaptiveStreamingPlayer::ABRGetDesiredLiveEdgeLatency() const
{
	TSharedPtrTS<IManifest> Current = Manifest;
	return Current.IsValid() ? Current->GetDesiredLiveLatency() : FTimeValue();
}
FTimeValue FAdaptiveStreamingPlayer::ABRGetLatency() const
{
	return PlaybackState.GetCurrentLiveLatency();
}
FTimeValue FAdaptiveStreamingPlayer::ABRGetPlaySpeed() const
{
	return FTimeValue(RenderClock->IsRunning() ? 1.0 : 0.0);
}
void FAdaptiveStreamingPlayer::ABRGetStreamBufferStats(IAdaptiveStreamSelector::IPlayerLiveControl::FABRBufferStats& OutBufferStats, EStreamType ForStream)
{
	UpdateDiagnostics();
	DiagnosticsCriticalSection.Lock();
	switch(ForStream)
	{
		case EStreamType::Video:
			OutBufferStats.PlayableContentDuration = VideoBufferStats.StreamBuffer.PlayableDuration;
			OutBufferStats.bReachedEnd = VideoBufferStats.StreamBuffer.bEndOfData;
			OutBufferStats.bEndOfTrack = VideoBufferStats.StreamBuffer.bEndOfTrack;
			break;
		case EStreamType::Audio:
			OutBufferStats.PlayableContentDuration = AudioBufferStats.StreamBuffer.PlayableDuration;
			OutBufferStats.bReachedEnd = AudioBufferStats.StreamBuffer.bEndOfData;
			OutBufferStats.bEndOfTrack = AudioBufferStats.StreamBuffer.bEndOfTrack;
			break;
		case EStreamType::Subtitle:
			OutBufferStats.PlayableContentDuration = TextBufferStats.StreamBuffer.PlayableDuration;
			OutBufferStats.bReachedEnd = TextBufferStats.StreamBuffer.bEndOfData;
			OutBufferStats.bEndOfTrack = TextBufferStats.StreamBuffer.bEndOfTrack;
			break;
		default:
			break;
	}
	DiagnosticsCriticalSection.Unlock();
	if (ForStream == EStreamType::Video && VideoRender.Renderer.IsValid())
	{
		OutBufferStats.PlayableContentDuration += VideoRender.Renderer->GetEnqueuedSampleDuration();
	}
	else if (ForStream == EStreamType::Audio && AudioRender.Renderer.IsValid())
	{
		OutBufferStats.PlayableContentDuration += AudioRender.Renderer->GetEnqueuedSampleDuration();
	}
}
void FAdaptiveStreamingPlayer::ABRSetRenderRateScale(double InRenderRateScale)
{
	RenderRateScale = InRenderRateScale;
	if (VideoRender.Renderer.IsValid())
	{
		VideoRender.Renderer->SetPlayRateScale(InRenderRateScale);
	}
	if (AudioRender.Renderer.IsValid())
	{
		AudioRender.Renderer->SetPlayRateScale(InRenderRateScale);
	}
}
FTimeRange FAdaptiveStreamingPlayer::ABRGetSupportedRenderRateScale()
{
	FTimeRange Range;
	if (AudioRender.Renderer.IsValid())
	{
		Range = AudioRender.Renderer->GetSupportedRenderRateScale();
	}
	else if (VideoRender.Renderer.IsValid())
	{
		Range = VideoRender.Renderer->GetSupportedRenderRateScale();
	}
	return Range;
}

double FAdaptiveStreamingPlayer::ABRGetRenderRateScale() const
{
	return RenderRateScale;
}

void FAdaptiveStreamingPlayer::ABRTriggerClockSync(IAdaptiveStreamSelector::IPlayerLiveControl::EClockSyncType InClockSyncType)
{
	if (Manifest.IsValid())
	{
		Manifest->TriggerClockSync(InClockSyncType == IAdaptiveStreamSelector::IPlayerLiveControl::EClockSyncType::Required ?
									IManifest::EClockSyncType::Required : IManifest::EClockSyncType::Recommended);
	}
}

void FAdaptiveStreamingPlayer::ABRTriggerPlaylistRefresh()
{
	if (Manifest.IsValid())
	{
		Manifest->TriggerPlaylistRefresh();
	}
}










//-----------------------------------------------------------------------------
/**
*/
#if PLATFORM_ANDROID
void FAdaptiveStreamingPlayer::Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer>& Surface)
{
	if (VideoDecoder.Decoder)
	{
		VideoDecoder.Decoder->Android_UpdateSurface(Surface);
	}
}

void FAdaptiveStreamingPlayer::Android_SuspendOrResumeDecoder(bool bSuspend)
{
	VideoDecoder.SuspendOrResume(bSuspend);
	AudioDecoder.SuspendOrResume(bSuspend);
}


FParamDict& FAdaptiveStreamingPlayer::Android_Workarounds(FStreamCodecInformation::ECodec InForCodec)
{
	if (InForCodec == FStreamCodecInformation::ECodec::H264)
	{
		return IVideoDecoderH264::Android_Workarounds();
	}
#if ELECTRA_PLATFORM_HAS_H265_DECODER
	else if (InForCodec == FStreamCodecInformation::ECodec::H265)
	{
		return IVideoDecoderH265::Android_Workarounds();
	}
#endif
	else
	{
		static FParamDict Dummy;
		return Dummy;
	}
}

#endif

//-----------------------------------------------------------------------------
/**
 * Debug prints information to the screen
 *
 * @param pPrintFN
 */
void FAdaptiveStreamingPlayer::DebugPrint(void* pPlayer, void (*pPrintFN)(void* pPlayer, const char *pFmt, ...))
{
#if 1
	DiagnosticsCriticalSection.Lock();
	const FAccessUnitBufferInfo *pD = nullptr;
	if(1)
	{
		pD = &VideoBufferStats.StreamBuffer;
		pPrintFN(pPlayer, "Video buffer  : EOT %d; EOS %d; %3u AUs; %8u/%8u bytes in; %#7.4fs", pD->bEndOfTrack, pD->bEndOfData, (uint32)pD->NumCurrentAccessUnits, (uint32)pD->CurrentMemInUse, (uint32)PlayerConfig.StreamBufferConfigVideo.MaxDataSize, pD->PlayableDuration.GetAsSeconds());
		pPrintFN(pPlayer, "Video decoder : %2u in decoder, %zu total, %s; EOD in %d; EOD out %d", (uint32)VideoBufferStats.DecoderOutputBuffer.NumElementsInDecoder, (uint32)VideoBufferStats.DecoderOutputBuffer.MaxDecodedElementsReady, VideoBufferStats.DecoderOutputBuffer.bOutputStalled?"    stalled":"not stalled", VideoBufferStats.DecoderInputBuffer.bEODReached, VideoBufferStats.DecoderOutputBuffer.bEODreached);
		pPrintFN(pPlayer, "Video renderer: %#7.4fs enqueued", VideoRender.Renderer.IsValid() ? VideoRender.Renderer->GetEnqueuedSampleDuration().GetAsSeconds() : 0.0);
	}
	if(1)
	{
		pD = &AudioBufferStats.StreamBuffer;
		pPrintFN(pPlayer, "Audio buffer  : EOT %d; EOS %d; %3u AUs; %8u/%8u bytes in; %#7.4fs", pD->bEndOfTrack, pD->bEndOfData, (uint32)pD->NumCurrentAccessUnits, (uint32)pD->CurrentMemInUse, (uint32)PlayerConfig.StreamBufferConfigAudio.MaxDataSize, pD->PlayableDuration.GetAsSeconds());
		pPrintFN(pPlayer, "Audio decoder : %2u in decoder, %2u total, %s; EOD in %d; EOD out %d", (uint32)AudioBufferStats.DecoderOutputBuffer.NumElementsInDecoder, (uint32)AudioBufferStats.DecoderOutputBuffer.MaxDecodedElementsReady, AudioBufferStats.DecoderOutputBuffer.bOutputStalled ? "    stalled" : "not stalled", AudioBufferStats.DecoderInputBuffer.bEODReached, AudioBufferStats.DecoderOutputBuffer.bEODreached);
		pPrintFN(pPlayer, "Audio renderer: %#7.4fs enqueued", AudioRender.Renderer.IsValid() ? AudioRender.Renderer->GetEnqueuedSampleDuration().GetAsSeconds() : 0.0);
	}
	if(1)
	{
		pD = &TextBufferStats.StreamBuffer;
		pPrintFN(pPlayer, "Text buffer   : EOT %d; EOS %d; %3u AUs; %8u/%8u bytes in; eod %d; %#7.4fs", pD->bEndOfTrack, pD->bEndOfData, (uint32)pD->NumCurrentAccessUnits, (uint32)pD->CurrentMemInUse, (uint32)PlayerConfig.StreamBufferConfigText.MaxDataSize, pD->PlayableDuration.GetAsSeconds());
	}
	DiagnosticsCriticalSection.Unlock();

	pPrintFN(pPlayer, "Player state  : %s", GetPlayerStateName(CurrentState));
	pPrintFN(pPlayer, "Decoder state : %s", GetDecoderStateName(DecoderState));
	pPrintFN(pPlayer, "Pipeline state: %s", GetPipelineStateName(PipelineState));
	pPrintFN(pPlayer, "Stream state  : %s", GetStreamStateName(StreamState));
	pPrintFN(pPlayer, "-----------------------------------");
	FTimeValue RangeStart = GetOptions().GetValue(OptionPlayRangeStart).SafeGetTimeValue(FTimeValue());
	FTimeValue RangeEnd = GetOptions().GetValue(OptionPlayRangeEnd).SafeGetTimeValue(FTimeValue());
	FTimeRange seekable, timeline;
	GetSeekableRange(seekable);
	PlaybackState.GetTimelineRange(timeline);
	pPrintFN(pPlayer, " have metadata: %s", HaveMetadata() ? "Yes" : "No");
	pPrintFN(pPlayer, "      duration: %.3f", GetDuration().GetAsSeconds());
	pPrintFN(pPlayer, "timeline range: %.3f - %.3f", timeline.Start.GetAsSeconds(), timeline.End.GetAsSeconds());
	pPrintFN(pPlayer, "seekable range: %.3f - %.3f", seekable.Start.GetAsSeconds(), seekable.End.GetAsSeconds());
	pPrintFN(pPlayer, "playback range: %.3f - %.3f", RangeStart.GetAsSeconds(), RangeEnd.GetAsSeconds());
	pPrintFN(pPlayer, " play position: [%d] %.3f", (int32) GetPlayPosition().GetSequenceIndex(), GetPlayPosition().GetAsSeconds());
	pPrintFN(pPlayer, "  is buffering: %s", IsBuffering() ? "Yes" : "No");
	pPrintFN(pPlayer, "    is playing: %s", IsPlaying() ? "Yes" : "No");
	pPrintFN(pPlayer, "     is paused: %s", IsPaused() ? "Yes" : "No");
	pPrintFN(pPlayer, "    is seeking: %s", IsSeeking() ? "Yes" : "No");
	pPrintFN(pPlayer, "     has ended: %s", HasEnded() ? "Yes" : "No");
#endif
	if (StreamSelector.IsValid())
	{
		StreamSelector->DebugPrint(pPlayer, pPrintFN);
	}
}
void FAdaptiveStreamingPlayer::DebugHandle(void* pPlayer, void (*debugDrawPrintf)(void* pPlayer, const char *pFmt, ...))
{
	if (PointerToLatestPlayer)
	{
		PointerToLatestPlayer->DebugPrint(pPlayer, debugDrawPrintf);
	}
}



//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------


TWeakPtrTS<FAdaptiveStreamingPlayerWorkerThread>	FAdaptiveStreamingPlayerWorkerThread::SingletonSelf;
FCriticalSection									FAdaptiveStreamingPlayerWorkerThread::SingletonLock;

TSharedPtrTS<FAdaptiveStreamingPlayerWorkerThread> FAdaptiveStreamingPlayerWorkerThread::Create(bool bUseSharedWorkerThread)
{
	if (bUseSharedWorkerThread)
	{
		FScopeLock lock(&SingletonLock);
		TSharedPtrTS<FAdaptiveStreamingPlayerWorkerThread> Self = SingletonSelf.Pin();
		if (!Self.IsValid())
		{
			FAdaptiveStreamingPlayerWorkerThread* Handler = new FAdaptiveStreamingPlayerWorkerThread;
			Handler->bIsDedicatedWorker = !bUseSharedWorkerThread;
			Handler->StartWorkerThread();
			Self = MakeShareable(Handler);
			SingletonSelf = Self;
		}
		return Self;
	}
	else
	{
		FAdaptiveStreamingPlayerWorkerThread* Handler = new FAdaptiveStreamingPlayerWorkerThread;
		Handler->bIsDedicatedWorker = !bUseSharedWorkerThread;
		Handler->StartWorkerThread();
		return MakeShareable(Handler);
	}
}

FAdaptiveStreamingPlayerWorkerThread::~FAdaptiveStreamingPlayerWorkerThread()
{
	StopWorkerThread();
}

void FAdaptiveStreamingPlayerWorkerThread::StartWorkerThread()
{
	if (bIsDedicatedWorker)
	{
		WorkerThread.ThreadSetName("ElectraPlayer::Worker");
	}
	else
	{
		WorkerThread.ThreadSetName("ElectraPlayer::SharedWorker");
	}
	WorkerThread.ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FAdaptiveStreamingPlayerWorkerThread::WorkerThreadFN));
}

void FAdaptiveStreamingPlayerWorkerThread::StopWorkerThread()
{
	bTerminate = true;
	HaveWorkSignal.Signal();
	WorkerThread.ThreadWaitDone();
	WorkerThread.ThreadReset();
}

void FAdaptiveStreamingPlayerWorkerThread::WorkerThreadFN()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	auto RemoveInstances = [this]() -> void
	{
		FInstanceToRemove DelInstance;
		while(InstancesToRemove.Dequeue(DelInstance))
		{
			PlayerInstances.Remove(DelInstance.Player);
			if (DelInstance.DoneSignal)
			{
				DelInstance.DoneSignal->Signal();
			}
		}
	};

	while(!bTerminate)
	{
		HaveWorkSignal.WaitTimeoutAndReset(1000 * 20);

		FAdaptiveStreamingPlayer* NewInstance;
		while(InstancesToAdd.Dequeue(NewInstance))
		{
			PlayerInstances.Add(NewInstance);
		}

		for(int32 i=0; i<PlayerInstances.Num(); ++i)
		{
			PlayerInstances[i]->HandleOnce();
		}

		RemoveInstances();
	}
	RemoveInstances();
}


//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------

TWeakPtrTS<FAdaptiveStreamingPlayerEventHandler>	FAdaptiveStreamingPlayerEventHandler::SingletonSelf;
FCriticalSection									FAdaptiveStreamingPlayerEventHandler::SingletonLock;

TSharedPtrTS<FAdaptiveStreamingPlayerEventHandler> FAdaptiveStreamingPlayerEventHandler::Create()
{
	FScopeLock lock(&SingletonLock);
	TSharedPtrTS<FAdaptiveStreamingPlayerEventHandler> Self = SingletonSelf.Pin();
	if (!Self.IsValid())
	{
		FAdaptiveStreamingPlayerEventHandler* Handler = new FAdaptiveStreamingPlayerEventHandler;
		Handler->StartWorkerThread();
		Self = MakeShareable(Handler);
		SingletonSelf = Self;
	}
	return Self;
}

void FAdaptiveStreamingPlayerEventHandler::DispatchEvent(TSharedPtrTS<FMetricEvent> InEvent)
{
	EventQueue.SendMessage(InEvent);
}

FAdaptiveStreamingPlayerEventHandler::~FAdaptiveStreamingPlayerEventHandler()
{
	StopWorkerThread();
}

void FAdaptiveStreamingPlayerEventHandler::StartWorkerThread()
{
	WorkerThread.ThreadSetName("ElectraPlayer::EventDispatch");
	WorkerThread.ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FAdaptiveStreamingPlayerEventHandler::WorkerThreadFN));
}

void FAdaptiveStreamingPlayerEventHandler::StopWorkerThread()
{
	EventQueue.SendMessage(TSharedPtrTS<FMetricEvent>());
	WorkerThread.ThreadWaitDone();
	WorkerThread.ThreadReset();
}

void FAdaptiveStreamingPlayerEventHandler::WorkerThreadFN()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);
	while(1)
	{
		TSharedPtrTS<FMetricEvent> pEvt = EventQueue.ReceiveMessage();
		if (!pEvt.IsValid())
		{
			break;
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_EventWorker);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AdaptiveStreamingPlayer_Event);

			// Get the player that sends the event. If it no longer exists ignore the event.
			TSharedPtrTS<FAdaptiveStreamingPlayer>	Player = pEvt->Player.Pin();
			if (Player.IsValid())
			{
				Player->FireSyncEvent(pEvt);
			}
		}
	}
}





} // namespace Electra
