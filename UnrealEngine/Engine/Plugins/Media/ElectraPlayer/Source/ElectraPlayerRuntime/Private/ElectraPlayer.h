// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Containers/UnrealString.h"
#include "Containers/Queue.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "IAnalyticsProviderET.h"

#include "IElectraPlayerInterface.h"

#include "Player/AdaptiveStreamingPlayer.h"
#include "PlayerRuntimeGlobal.h"

class FVideoDecoderOutput;
using FVideoDecoderOutputPtr = TSharedPtr<FVideoDecoderOutput, ESPMode::ThreadSafe>;
class IAudioDecoderOutput;
using IAudioDecoderOutputPtr = TSharedPtr<IAudioDecoderOutput, ESPMode::ThreadSafe>;
class IMetaDataDecoderOutput;
using IMetaDataDecoderOutputPtr = TSharedPtr<IMetaDataDecoderOutput, ESPMode::ThreadSafe>;
class ISubtitleDecoderOutput;
using ISubtitleDecoderOutputPtr = TSharedPtr<ISubtitleDecoderOutput, ESPMode::ThreadSafe>;

namespace Electra
{
class IVideoDecoderResourceDelegate;
}

class FElectraRendererVideo;
class FElectraRendererAudio;

using namespace Electra;

DECLARE_MULTICAST_DELEGATE_TwoParams(FElectraPlayerSendAnalyticMetricsDelegate, const TSharedPtr<IAnalyticsProviderET>& /*AnalyticsProvider*/, const FGuid& /*PlayerGuid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FElectraPlayerSendAnalyticMetricsPerMinuteDelegate, const TSharedPtr<IAnalyticsProviderET>& /*AnalyticsProvider*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FElectraPlayerReportVideoStreamingErrorDelegate, const FGuid& /*PlayerGuid*/, const FString& /*LastError*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FElectraPlayerReportSubtitlesMetricsDelegate, const FGuid& /*PlayerGuid*/, const FString& /*URL*/, double /*ResponseTime*/, const FString& /*LastError*/);

class FElectraPlayer
	: public IElectraPlayerInterface
	, public IAdaptiveStreamingPlayerMetrics
{
public:
	FElectraPlayer(const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate,
			  FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
			  FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
			  FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
			  FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate);
	~FElectraPlayer();

	void OnVideoDecoded(const FVideoDecoderOutputPtr& DecoderOutput, bool bDoNotRender);
	void OnVideoFlush();
	void OnAudioDecoded(const IAudioDecoderOutputPtr& DecoderOutput);
	void OnAudioFlush();
	void OnSubtitleDecoded(ISubtitleDecoderOutputPtr DecoderOutput);
	void OnSubtitleFlush();

	void OnVideoRenderingStarted();
	void OnVideoRenderingStopped();
	void OnAudioRenderingStarted();
	void OnAudioRenderingStopped();

	void SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider, const FGuid& InPlayerGuid);
	void SendAnalyticMetricsPerMinute(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider);
	void SendPendingAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider);
	void ReportVideoStreamingError(const FGuid& InPlayerGuid, const FString& LastError);
	void ReportSubtitlesMetrics(const FGuid& InPlayerGuid, const FString& URL, double ResponseTime, const FString& LastError);

	void DropOldFramesFromPresentationQueue();

	bool CanPresentVideoFrames(uint64 NumFrames);
	bool CanPresentAudioFrames(uint64 NumFrames);

	FString GetUrl() const
	{
		return MediaUrl;
	}

	void SetGuid(const FGuid& Guid)
	{
		PlayerGuid = Guid;
	}

	void SetAsyncResourceReleaseNotification(IAsyncResourceReleaseNotifyContainer* AsyncResourceReleaseNotification) override;

	// -------- PlayerAdapter (Plugin/Native) API

	bool OpenInternal(const FString& Url, const FParamDict& PlayerOptions, const FPlaystartOptions& InPlaystartOptions) override;
	void CloseInternal(bool bKillAfterClose) override;

	void Tick(FTimespan DeltaTime, FTimespan Timecode) override;

	bool IsKillAfterCloseAllowed() const override { return bAllowKillAfterCloseEvent;  }

	EPlayerState GetState() const override;
	EPlayerStatus GetStatus() const override;

	bool IsLooping() const override;
	bool SetLooping(bool bLooping) override;
	int32 GetLoopCount() const override;

	FTimespan GetTime() const override;
	FTimespan GetDuration() const override;

	bool IsLive() const override;
	FTimespan GetSeekableDuration() const override;

	void SetPlaybackRange(const FPlaybackRange& InPlaybackRange) override;
	void GetPlaybackRange(FPlaybackRange& OutPlaybackRange) const override;

	float GetRate() const override;
	bool SetRate(float Rate) override;

	bool Seek(const FTimespan& Time) override;
	bool Seek(const FTimespan& Time, const FSeekParam& Param) override;
	void SetFrameAccurateSeekMode(bool bEnableFrameAccuracy) override;

	void ModifyOptions(const FParamDict& InOptionsToSetOrChange, const FParamDict& InOptionsToClear) override;

	bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FAudioTrackFormat& OutFormat) const override;
	bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FVideoTrackFormat& OutFormat) const override;

	int32 GetNumTracks(EPlayerTrackType TrackType) const override;
	int32 GetNumTrackFormats(EPlayerTrackType TrackType, int32 TrackIndex) const override;
	int32 GetSelectedTrack(EPlayerTrackType TrackType) const override;
	FText GetTrackDisplayName(EPlayerTrackType TrackType, int32 TrackIndex) const override;
	int32 GetTrackFormat(EPlayerTrackType TrackType, int32 TrackIndex) const override;
	FString GetTrackLanguage(EPlayerTrackType TrackType, int32 TrackIndex) const override;
	FString GetTrackName(EPlayerTrackType TrackType, int32 TrackIndex) const override;
	bool SelectTrack(EPlayerTrackType TrackType, int32 TrackIndex) override;

	int32 GetNumVideoStreams(int32 TrackIndex) const override;
	bool GetVideoStreamFormat(FVideoStreamFormat& OutFormat, int32 InTrackIndex, int32 InStreamIndex) const override;
	bool GetActiveVideoStreamFormat(FVideoStreamFormat& OutFormat) const override;

	void NotifyOfOptionChange() override;

	void SuspendOrResumeDecoders(bool bSuspend) override;

private:
	DECLARE_DELEGATE_TwoParams(FOnMediaPlayerEventReceivedDelegate, TSharedPtrTS<IAdaptiveStreamingPlayerAEMSEvent> /*InEvent*/, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode /*InDispatchMode*/);
	class FAEMSEventReceiver : public IAdaptiveStreamingPlayerAEMSReceiver
	{
	public:
		virtual ~FAEMSEventReceiver() = default;
		FOnMediaPlayerEventReceivedDelegate& GetEventReceivedDelegate()
		{ return EventReceivedDelegate; }
	private:
		virtual void OnMediaPlayerEventReceived(TSharedPtrTS<IAdaptiveStreamingPlayerAEMSEvent> InEvent, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode InDispatchMode) override
		{ EventReceivedDelegate.ExecuteIfBound(InEvent, InDispatchMode); }
		FOnMediaPlayerEventReceivedDelegate EventReceivedDelegate;
	};

	DECLARE_DELEGATE_OneParam(FOnMediaPlayerSubtitleReceivedDelegate, ISubtitleDecoderOutputPtr);
	DECLARE_DELEGATE(FOnMediaPlayerSubtitleFlushDelegate);
	class FSubtitleEventReceiver : public IAdaptiveStreamingPlayerSubtitleReceiver
	{
	public:
		virtual ~FSubtitleEventReceiver() = default;
		FOnMediaPlayerSubtitleReceivedDelegate& GetSubtitleReceivedDelegate()
		{ return SubtitleReceivedDelegate; }
		FOnMediaPlayerSubtitleFlushDelegate& GetSubtitleFlushDelegate()
		{ return SubtitleFlushDelegate; }
	private:
		virtual void OnMediaPlayerSubtitleReceived(ISubtitleDecoderOutputPtr Subtitle) override
		{ SubtitleReceivedDelegate.ExecuteIfBound(Subtitle); }
		virtual void OnMediaPlayerFlushSubtitles() override
		{ SubtitleFlushDelegate.ExecuteIfBound(); }		 
		FOnMediaPlayerSubtitleReceivedDelegate SubtitleReceivedDelegate;
		FOnMediaPlayerSubtitleFlushDelegate SubtitleFlushDelegate;
	};



	struct FPlayerMetricEventBase
	{
		enum class EType
		{
			OpenSource,
			ReceivedMasterPlaylist,
			ReceivedPlaylists,
			TracksChanged,
			PlaylistDownload,
			CleanStart,
			BufferingStart,
			BufferingEnd,
			Bandwidth,
			BufferUtilization,
			SegmentDownload,
			LicenseKey,
			DataAvailabilityChange,
			VideoQualityChange,
			CodecFormatChange,
			PrerollStart,
			PrerollEnd,
			PlaybackStart,
			PlaybackPaused,
			PlaybackResumed,
			PlaybackEnded,
			JumpInPlayPosition,
			PlaybackStopped,
			SeekCompleted,
			Error,
			LogMessage,
			DroppedVideoFrame,
			DroppedAudioFrame
		};
		FPlayerMetricEventBase(EType InType) : Type(InType) {}
		virtual ~FPlayerMetricEventBase() = default;
		EType Type;
	};
	struct FPlayerMetricEvent_OpenSource : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_OpenSource(const FString& InURL) : FPlayerMetricEventBase(EType::OpenSource), URL(InURL) {}
		FString URL;
	};
	struct FPlayerMetricEvent_ReceivedMasterPlaylist : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_ReceivedMasterPlaylist(const FString& InEffectiveURL) : FPlayerMetricEventBase(EType::ReceivedMasterPlaylist), EffectiveURL(InEffectiveURL) {}
		FString EffectiveURL;
	};
	struct FPlayerMetricEvent_PlaylistDownload : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_PlaylistDownload(const Metrics::FPlaylistDownloadStats& InPlaylistDownloadStats) : FPlayerMetricEventBase(EType::PlaylistDownload), PlaylistDownloadStats(InPlaylistDownloadStats) {}
		Metrics::FPlaylistDownloadStats PlaylistDownloadStats;
	};
	struct FPlayerMetricEvent_BufferingStart : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_BufferingStart(Metrics::EBufferingReason InBufferingReason) : FPlayerMetricEventBase(EType::BufferingStart), BufferingReason(InBufferingReason) {}
		Metrics::EBufferingReason BufferingReason;
	};
	struct FPlayerMetricEvent_BufferingEnd : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_BufferingEnd(Metrics::EBufferingReason InBufferingReason) : FPlayerMetricEventBase(EType::BufferingEnd), BufferingReason(InBufferingReason) {}
		Metrics::EBufferingReason BufferingReason;
	};
	struct FPlayerMetricEvent_Bandwidth : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_Bandwidth(int64 InEffectiveBps, int64 InThroughputBps, double InLatencyInSeconds) : FPlayerMetricEventBase(EType::Bandwidth), EffectiveBps(InEffectiveBps), ThroughputBps(InThroughputBps), LatencyInSeconds(InLatencyInSeconds) {}
		int64 EffectiveBps;
		int64 ThroughputBps;
		double LatencyInSeconds;
	};
	struct FPlayerMetricEvent_BufferUtilization : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_BufferUtilization(const Metrics::FBufferStats& InBufferStats) : FPlayerMetricEventBase(EType::BufferUtilization), BufferStats(InBufferStats) {}
		Metrics::FBufferStats BufferStats;
	};
	struct FPlayerMetricEvent_SegmentDownload : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_SegmentDownload(const Metrics::FSegmentDownloadStats& InSegmentDownloadStats) : FPlayerMetricEventBase(EType::SegmentDownload), SegmentDownloadStats(InSegmentDownloadStats) {}
		Metrics::FSegmentDownloadStats SegmentDownloadStats;
	};
	struct FPlayerMetricEvent_LicenseKey : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_LicenseKey(const Metrics::FLicenseKeyStats& InLicenseKeyStats) : FPlayerMetricEventBase(EType::LicenseKey), LicenseKeyStats(InLicenseKeyStats) {}
		Metrics::FLicenseKeyStats LicenseKeyStats;
	};
	struct FPlayerMetricEvent_DataAvailabilityChange : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_DataAvailabilityChange(const Metrics::FDataAvailabilityChange& InDataAvailability) : FPlayerMetricEventBase(EType::DataAvailabilityChange), DataAvailability(InDataAvailability) {}
		Metrics::FDataAvailabilityChange DataAvailability;
	};
	struct FPlayerMetricEvent_VideoQualityChange : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_VideoQualityChange(int32 InNewBitrate, int32 InPreviousBitrate, bool bInIsDrasticDownswitch) : FPlayerMetricEventBase(EType::VideoQualityChange), NewBitrate(InNewBitrate), PreviousBitrate(InPreviousBitrate), bIsDrasticDownswitch(bInIsDrasticDownswitch) {}
		int32 NewBitrate;
		int32 PreviousBitrate;
		bool bIsDrasticDownswitch;
	};
	struct FPlayerMetricEvent_CodecFormatChange : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_CodecFormatChange(const FStreamCodecInformation& InNewDecodingFormat) : FPlayerMetricEventBase(EType::CodecFormatChange), NewDecodingFormat(InNewDecodingFormat) {}
		FStreamCodecInformation NewDecodingFormat;
	};
	struct FPlayerMetricEvent_JumpInPlayPosition : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_JumpInPlayPosition(const FTimeValue& InToNewTime, const FTimeValue& InFromTime, Metrics::ETimeJumpReason InTimejumpReason) : FPlayerMetricEventBase(EType::JumpInPlayPosition), ToNewTime(InToNewTime), FromTime(InFromTime), TimejumpReason(InTimejumpReason) {}
		FTimeValue ToNewTime;
		FTimeValue FromTime;
		Metrics::ETimeJumpReason TimejumpReason;
	};
	struct FPlayerMetricEvent_Error : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_Error(const FString& InErrorReason) : FPlayerMetricEventBase(EType::Error), ErrorReason(InErrorReason) {}
		FString ErrorReason;
	};
	struct FPlayerMetricEvent_LogMessage : public FPlayerMetricEventBase
	{
		FPlayerMetricEvent_LogMessage(IInfoLog::ELevel InLogLevel, const FString& InLogMessage, int64 InPlayerWallclockMilliseconds) : FPlayerMetricEventBase(EType::LogMessage), LogLevel(InLogLevel), LogMessage(InLogMessage), PlayerWallclockMilliseconds(InPlayerWallclockMilliseconds) {}
		IInfoLog::ELevel LogLevel;
		FString LogMessage;
		int64 PlayerWallclockMilliseconds;
	};


	void CalculateTargetSeekTime(FTimespan& OutTargetTime, const FTimespan& InTime);

	bool PresentVideoFrame(const FVideoDecoderOutputPtr& InVideoFrame);
	bool PresentAudioFrame(const IAudioDecoderOutputPtr& DecoderOutput);
	bool PresentSubtitle(const ISubtitleDecoderOutputPtr& DecoderOutput);

	void PlatformNotifyOfOptionChange();
	void PlatformSuspendOrResumeDecoders(bool bSuspend);

	void OnMediaPlayerEventReceived(TSharedPtrTS<IAdaptiveStreamingPlayerAEMSEvent> InEvent, IAdaptiveStreamingPlayerAEMSReceiver::EDispatchMode InDispatchMode);

	// Methods from IAdaptiveStreamingPlayerMetrics
	virtual void ReportOpenSource(const FString& URL) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_OpenSource>(URL)); }
	virtual void ReportReceivedMasterPlaylist(const FString& EffectiveURL) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_ReceivedMasterPlaylist>(EffectiveURL)); }
	virtual void ReportReceivedPlaylists() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::ReceivedPlaylists)); }
	virtual void ReportTracksChanged() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::TracksChanged)); }
	virtual void ReportPlaylistDownload(const Metrics::FPlaylistDownloadStats& PlaylistDownloadStats) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_PlaylistDownload>(PlaylistDownloadStats)); }
	virtual void ReportCleanStart() override
	{ /*DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::CleanStart));*/ 
		bDiscardOutputUntilCleanStart = false;
	}
	virtual void ReportBufferingStart(Metrics::EBufferingReason BufferingReason) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_BufferingStart>(BufferingReason)); }
	virtual void ReportBufferingEnd(Metrics::EBufferingReason BufferingReason) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_BufferingEnd>(BufferingReason)); }
	virtual void ReportBandwidth(int64 EffectiveBps, int64 ThroughputBps, double LatencyInSeconds) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_Bandwidth>(EffectiveBps, ThroughputBps, LatencyInSeconds)); }
	virtual void ReportBufferUtilization(const Metrics::FBufferStats& BufferStats) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_BufferUtilization>(BufferStats)); }
	virtual void ReportSegmentDownload(const Metrics::FSegmentDownloadStats& SegmentDownloadStats) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_SegmentDownload>(SegmentDownloadStats)); }
	virtual void ReportLicenseKey(const Metrics::FLicenseKeyStats& LicenseKeyStats) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_LicenseKey>(LicenseKeyStats)); }
	virtual void ReportDataAvailabilityChange(const Metrics::FDataAvailabilityChange& DataAvailability) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_DataAvailabilityChange>(DataAvailability)); }
	virtual void ReportVideoQualityChange(int32 NewBitrate, int32 PreviousBitrate, bool bIsDrasticDownswitch) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_VideoQualityChange>(NewBitrate, PreviousBitrate, bIsDrasticDownswitch)); }
	virtual void ReportDecodingFormatChange(const FStreamCodecInformation& NewDecodingFormat) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_CodecFormatChange>(NewDecodingFormat)); }
	virtual void ReportPrerollStart() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PrerollStart)); }
	virtual void ReportPrerollEnd() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PrerollEnd)); }
	virtual void ReportPlaybackStart() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PlaybackStart)); }
	virtual void ReportPlaybackPaused() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PlaybackPaused)); }
	virtual void ReportPlaybackResumed() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PlaybackResumed)); }
	virtual void ReportPlaybackEnded() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PlaybackEnded)); }
	virtual void ReportJumpInPlayPosition(const FTimeValue& ToNewTime, const FTimeValue& FromTime, Metrics::ETimeJumpReason TimejumpReason) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_JumpInPlayPosition>(ToNewTime, FromTime, TimejumpReason)); }
	virtual void ReportPlaybackStopped() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::PlaybackStopped)); }
	virtual void ReportSeekCompleted() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::SeekCompleted)); }
	virtual void ReportError(const FString& ErrorReason) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_Error>(ErrorReason)); }
	virtual void ReportLogMessage(IInfoLog::ELevel InLogLevel, const FString& InLogMessage, int64 InPlayerWallclockMilliseconds) override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEvent_LogMessage>(InLogLevel, InLogMessage, InPlayerWallclockMilliseconds)); }
	virtual void ReportDroppedVideoFrame() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::DroppedVideoFrame)); }
	virtual void ReportDroppedAudioFrame() override
	{ DeferredPlayerEvents.Enqueue(MakeSharedTS<FPlayerMetricEventBase>(FPlayerMetricEventBase::EType::DroppedAudioFrame)); }

	void LogPresentationFramesQueues(FTimespan DeltaTime);

	void ClearToDefaultState();

	void MediaStateOnPreparingFinished();
	bool MediaStateOnPlay();
	bool MediaStateOnPause();
	void MediaStateOnEndReached();
	void MediaStateOnSeekFinished();
	void TriggerFirstSeekIfNecessary();

	TSharedPtr<FTrackMetadata, ESPMode::ThreadSafe> GetTrackStreamMetadata(EPlayerTrackType TrackType, int32 TrackIndex) const;

	// Delegate to talk back to adapter host
	TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>	AdapterDelegate;

	// Contains number of audio tracks available to expose it later.
	int32											NumTracksAudio;
	int32											NumTracksVideo;
	int32											NumTracksSubtitle;
	int32											SelectedQuality;
	mutable int32									SelectedVideoTrackIndex;
	mutable int32									SelectedAudioTrackIndex;
	mutable int32									SelectedSubtitleTrackIndex;
	mutable bool									bVideoTrackIndexDirty;
	mutable bool									bAudioTrackIndexDirty;
	mutable bool									bSubtitleTrackIndexDirty;

	bool											bInitialSeekPerformed;
	bool											bDiscardOutputUntilCleanStart;

	FPlaybackRange									CurrentPlaybackRange;
	TOptional<bool>									bFrameAccurateSeeking;
	TOptional<bool>									bEnableLooping;

	TOptional<FVideoStreamFormat>					CurrentlyActiveVideoStreamFormat;

	FIntPoint										LastPresentedFrameDimension;


	struct FPlayerState
	{
		TOptional<float>		IntendedPlayRate;
		float					CurrentPlayRate = 0.0f;

		TAtomic<EPlayerState>	State;
		TAtomic<EPlayerStatus>	Status;

		bool					bUseInternal = false;

		void Reset()
		{
			IntendedPlayRate.Reset();
			CurrentPlayRate = 0.0f;
			State = EPlayerState::Closed;
			Status = EPlayerStatus::None;
		}

		float GetRate() const;
		EPlayerState GetState() const;
		EPlayerStatus GetStatus() const;

		void SetIntendedPlayRate(float InIntendedRate);
		void SetPlayRateFromPlayer(float InCurrentPlayerPlayRate);
	};


	/** Media player Guid */
	FGuid											PlayerGuid;
	/** Metric delegates */
	FElectraPlayerSendAnalyticMetricsDelegate&			SendAnalyticMetricsDelegate;
	FElectraPlayerSendAnalyticMetricsPerMinuteDelegate&	SendAnalyticMetricsPerMinuteDelegate;
	FElectraPlayerReportVideoStreamingErrorDelegate&		ReportVideoStreamingErrorDelegate;
	FElectraPlayerReportSubtitlesMetricsDelegate&		ReportSubtitlesMetricsDelegate;

	/** Option interface **/
	FPlaystartOptions								PlaystartOptions;

	FPlayerState									PlayerState;

	TAtomic<bool>									bPlayerHasClosed;
	TAtomic<bool>									bHasPendingError;

	bool											bAllowKillAfterCloseEvent;

	/** Queued events */
	TQueue<IElectraPlayerAdapterDelegate::EPlayerEvent>	DeferredEvents;
	TQueue<TSharedPtrTS<FPlayerMetricEventBase>>	DeferredPlayerEvents;
	TSharedPtrTS<FAEMSEventReceiver>				MediaPlayerEventReceiver;
	TSharedPtrTS<FSubtitleEventReceiver>			MediaPlayerSubtitleReceiver;


	/** The URL of the currently opened media. */
	FString											MediaUrl;

	class FInternalPlayerImpl
	{
	public:
		/** The media player itself **/
		TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe>	AdaptivePlayer;

		/** Renderers to use **/
		TSharedPtr<FElectraRendererVideo, ESPMode::ThreadSafe>		RendererVideo;
		TSharedPtr<FElectraRendererAudio, ESPMode::ThreadSafe>		RendererAudio;

		/** */
		static void DoCloseAsync(TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe> && Player, TSharedPtr<IAsyncResourceReleaseNotifyContainer, ESPMode::ThreadSafe> AsyncDestructNotification);
	};

	mutable FCriticalSection										PlayerLock;
	TSharedPtr<FInternalPlayerImpl, ESPMode::ThreadSafe>			CurrentPlayer;
	FEvent*															WaitForPlayerDestroyedEvent;
	TSharedPtr<Electra::FApplicationTerminationHandler, ESPMode::ThreadSafe> AppTerminationHandler;

	TSharedPtr<IAsyncResourceReleaseNotifyContainer, ESPMode::ThreadSafe> AsyncResourceReleaseNotification;

	class FAdaptiveStreamingPlayerResourceProvider : public IAdaptiveStreamingPlayerResourceProvider
	{
	public:
		FAdaptiveStreamingPlayerResourceProvider(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> & AdapterDelegate);
		virtual ~FAdaptiveStreamingPlayerResourceProvider() = default;

		virtual void ProvideStaticPlaybackDataForURL(TSharedPtr<IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe> InOutRequest) override;

		void ProcessPendingStaticResourceRequests();
		void ClearPendingRequests();

	private:
		/** Requests for static resource fetches we want to perform on the main thread **/
		TQueue<TSharedPtr<IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe>, EQueueMode::Mpsc> PendingStaticResourceRequests;

		// Player adapter delegate
		TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>	AdapterDelegate;
	};

	TSharedPtr<FAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe> StaticResourceProvider;
	TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> VideoDecoderResourceDelegate;


	class FAverageValue
	{
	public:
		FAverageValue()
		: Samples(nullptr)
		, NumSamples(0)
		, MaxSamples(0)
		{
		}
		~FAverageValue()
		{
			delete[] Samples;
		}
		void SetNumSamples(int32 InMaxSamples)
		{
			check(InMaxSamples > 0);
			delete[] Samples;
			NumSamples = 0;
			MaxSamples = InMaxSamples;
			Samples = new double[MaxSamples];
		}
		void AddValue(double Value)
		{
			Samples[NumSamples % MaxSamples] = Value;
			++NumSamples;
		}
		void Reset()
		{
			NumSamples = 0;
		}
		double GetAverage() const
		{
			double Avg = 0.0;
			if (NumSamples > 0)
			{
				double Sum = 0.0;
				int32 Last = NumSamples <= MaxSamples ? NumSamples : MaxSamples;
				for (int32 i = 0; i < Last; ++i)
				{
					Sum += Samples[i];
				}
				Avg = Sum / Last;
			}
			return Avg;
		}
	private:
		double*	Samples;
		int32	NumSamples;
		int32	MaxSamples;
	};

	struct FDroppedFrameStats
	{
		enum class EFrameType
		{
			Undefined,
			VideoFrame,
			AudioFrame
		};
		FDroppedFrameStats()
		{
			FrameType = EFrameType::Undefined;
			Reset();
		}
		void SetFrameType(EFrameType InFrameType)
		{
			FrameType = InFrameType;
		}
		void Reset()
		{
			NumTotalDropped = 0;
			WorstDeltaTime = FTimespan::Zero();
			PlayerTimeAtLastReport = FTimespan::MinValue();
			SystemTimeAtLastReport = 0.0;
			LogWarningAfterSeconds = 10.0;
		}
		void SetLogWarningInterval(double InSecondsBetweenWarnings)
		{
			LogWarningAfterSeconds = InSecondsBetweenWarnings;
		}
		void AddNewDrop(const FTimespan& InFrameTime, const FTimespan& InPlayerTime, void* InPtrElectraPlayer, void* InPtrCurrentPlayer);

		EFrameType	FrameType;
		uint32		NumTotalDropped;
		FTimespan	WorstDeltaTime;
		FTimespan	PlayerTimeAtLastReport;
		double		SystemTimeAtLastReport;
		double		LogWarningAfterSeconds;
	};

	struct FStatistics
	{
		struct FBandwidth
		{
			FBandwidth()
			{
				Bandwidth.SetNumSamples(3);
				Latency.SetNumSamples(3);
				Reset();
			}
			void Reset()
			{
				Bandwidth.Reset();
				Latency.Reset();
			}
			void AddSample(double InBytesPerSecond, double InLatency)
			{
				Bandwidth.AddValue(InBytesPerSecond);
				Latency.AddValue(InLatency);
			}
			double GetAverageBandwidth() const
			{
				return Bandwidth.GetAverage();
			}
			double GetAverageLatency() const
			{
				return Latency.GetAverage();
			}
			FAverageValue	Bandwidth;
			FAverageValue	Latency;
		};
		FStatistics()
		{
			DroppedVideoFrames.SetFrameType(FDroppedFrameStats::EFrameType::VideoFrame);
			DroppedAudioFrames.SetFrameType(FDroppedFrameStats::EFrameType::AudioFrame);
			Reset();
		}
		void Reset()
		{
			InitialURL.Empty();
			CurrentlyActivePlaylistURL.Empty();
			LastError.Empty();
			LastState = "Empty";
			TimeAtOpen  					= -1.0;
			TimeToLoadMasterPlaylist		= -1.0;
			TimeToLoadStreamPlaylists   	= -1.0;
			InitialBufferingDuration		= -1.0;
			InitialStreamBitrate			= 0;
			TimeAtPrerollBegin  			= -1.0;
			TimeForInitialPreroll   		= -1.0;
			NumTimesRebuffered  			= 0;
			NumTimesForwarded   			= 0;
			NumTimesRewound 				= 0;
			NumTimesLooped  				= 0;
			TimeAtBufferingBegin			= 0.0;
			TotalRebufferingDuration		= 0.0;
			LongestRebufferingDuration  	= 0.0;
			PlayPosAtStart  				= -1.0;
			PlayPosAtEnd					= -1.0;
			NumQualityUpswitches			= 0;
			NumQualityDownswitches  		= 0;
			NumQualityDrasticDownswitches   = 0;
			NumVideoDatabytesStreamed   	= 0;
			NumAudioDatabytesStreamed   	= 0;
			NumSegmentDownloadsAborted  	= 0;
			CurrentlyActiveResolutionWidth  = 0;
			CurrentlyActiveResolutionHeight = 0;
			VideoSegmentBitratesStreamed.Empty();
			InitialBufferingBandwidth.Reset();
			bIsInitiallyDownloading 		= false;
			bDidPlaybackEnd 				= false;
			SubtitlesURL.Empty();
			SubtitlesResponseTime			= 0.0;
			SubtitlesLastError.Empty();
			DroppedVideoFrames.Reset();
			DroppedAudioFrames.Reset();
			MediaTimelineAtStart.Reset();
			MediaTimelineAtEnd.Reset();
			MediaDuration = 0.0;
			MessageHistoryBuffer.Empty();
		}
		void AddMessageToHistory(FString InMessage)
		{
			if (MessageHistoryBuffer.Num() >= 20)
			{
				MessageHistoryBuffer.RemoveAt(0);
			}
			MessageHistoryBuffer.Emplace(MoveTemp(InMessage));
		}

		FString					InitialURL;
		FString					CurrentlyActivePlaylistURL;
		FString					LastError;
		FString					LastState;		// "Empty", "Opening", "Preparing", "Buffering", "Idle", "Ready", "Playing", "Paused", "Seeking", "Rebuffering", "Ended"
		double					TimeAtOpen;
		double					TimeToLoadMasterPlaylist;
		double					TimeToLoadStreamPlaylists;
		double					InitialBufferingDuration;
		int32					InitialStreamBitrate;
		double					TimeAtPrerollBegin;
		double					TimeForInitialPreroll;
		int32					NumTimesRebuffered;
		int32					NumTimesForwarded;
		int32					NumTimesRewound;
		int32					NumTimesLooped;
		double					TimeAtBufferingBegin;
		double					TotalRebufferingDuration;
		double					LongestRebufferingDuration;
		double					PlayPosAtStart;
		double					PlayPosAtEnd;
		int32					NumQualityUpswitches;
		int32					NumQualityDownswitches;
		int32					NumQualityDrasticDownswitches;
		int64					NumVideoDatabytesStreamed;
		int64					NumAudioDatabytesStreamed;
		int32					NumSegmentDownloadsAborted;
		int32					CurrentlyActiveResolutionWidth;
		int32					CurrentlyActiveResolutionHeight;
		TMap<int32, uint32>		VideoSegmentBitratesStreamed;		// key=video stream bitrate, value=number of segments loaded at this rate
		FBandwidth				InitialBufferingBandwidth;
		bool					bIsInitiallyDownloading;
		bool					bDidPlaybackEnd;
		FString					SubtitlesURL;
		double					SubtitlesResponseTime;
		FString					SubtitlesLastError;
		FDroppedFrameStats		DroppedVideoFrames;
		FDroppedFrameStats		DroppedAudioFrames;
		FTimeRange				MediaTimelineAtStart;
		FTimeRange				MediaTimelineAtEnd;
		double					MediaDuration;
		TArray<FString>			MessageHistoryBuffer;
	};

	struct FAnalyticsEvent
	{
		FString EventName;
		TArray<FAnalyticsEventAttribute> ParamArray;
	};

	void UpdatePlayEndStatistics();
	void LogStatistics();
	void AddCommonAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& InOutParamArray);
	TSharedPtr<FAnalyticsEvent> CreateAnalyticsEvent(FString InEventName);
	void EnqueueAnalyticsEvent(TSharedPtr<FAnalyticsEvent> InAnalyticEvent);

	FCriticalSection						StatisticsLock;
	FStatistics								Statistics;

	TQueue<TSharedPtr<FAnalyticsEvent>>		QueuedAnalyticEvents;
	int32									NumQueuedAnalyticEvents = 0;
	FString									AnalyticsOSVersion;
	FString									AnalyticsGPUType;

	/** Unique player instance GUID sent with each analytics event. This allows finding all events of a particular playback session. **/
	FString									AnalyticsInstanceGuid;
	/** Sequential analytics event number. Helps sorting events. **/
	uint32									AnalyticsInstanceEventCount;


	void HandleDeferredPlayerEvents();
	void HandlePlayerEventOpenSource(const FString& URL);
	void HandlePlayerEventReceivedMasterPlaylist(const FString& EffectiveURL);
	void HandlePlayerEventReceivedPlaylists();
	void HandlePlayerEventTracksChanged();
	void HandlePlayerEventPlaylistDownload(const Metrics::FPlaylistDownloadStats& PlaylistDownloadStats);
	void HandlePlayerEventBufferingStart(Metrics::EBufferingReason BufferingReason);
	void HandlePlayerEventBufferingEnd(Metrics::EBufferingReason BufferingReason);
	void HandlePlayerEventBandwidth(int64 EffectiveBps, int64 ThroughputBps, double LatencyInSeconds);
	void HandlePlayerEventBufferUtilization(const Metrics::FBufferStats& BufferStats);
	void HandlePlayerEventSegmentDownload(const Metrics::FSegmentDownloadStats& SegmentDownloadStats);
	void HandlePlayerEventLicenseKey(const Metrics::FLicenseKeyStats& LicenseKeyStats);
	void HandlePlayerEventDataAvailabilityChange(const Metrics::FDataAvailabilityChange& DataAvailability);
	void HandlePlayerEventVideoQualityChange(int32 NewBitrate, int32 PreviousBitrate, bool bIsDrasticDownswitch);
	void HandlePlayerEventCodecFormatChange(const Electra::FStreamCodecInformation& NewDecodingFormat);
	void HandlePlayerEventPrerollStart();
	void HandlePlayerEventPrerollEnd();
	void HandlePlayerEventPlaybackStart();
	void HandlePlayerEventPlaybackPaused();
	void HandlePlayerEventPlaybackResumed();
	void HandlePlayerEventPlaybackEnded();
	void HandlePlayerEventJumpInPlayPosition(const FTimeValue& ToNewTime, const FTimeValue& FromTime, Metrics::ETimeJumpReason TimejumpReason);
	void HandlePlayerEventPlaybackStopped();
	void HandlePlayerEventSeekCompleted();
	void HandlePlayerEventError(const FString& ErrorReason);
	void HandlePlayerEventLogMessage(IInfoLog::ELevel InLogLevel, const FString& InLogMessage, int64 InPlayerWallclockMilliseconds);
	void HandlePlayerEventDroppedVideoFrame();
	void HandlePlayerEventDroppedAudioFrame();
};

ENUM_CLASS_FLAGS(FElectraPlayer::EPlayerStatus);

