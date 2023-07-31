// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MediaDecoderOutput.h"
#include "IAnalyticsProviderET.h"
#include "ParameterDictionary.h"

#include "ElectraPlayerMisc.h"
#include "ElectraPlayerResourceDelegate.h"

class FVideoDecoderOutput;
using FVideoDecoderOutputPtr = TSharedPtr<FVideoDecoderOutput, ESPMode::ThreadSafe>;
class IAudioDecoderOutput;
using IAudioDecoderOutputPtr = TSharedPtr<IAudioDecoderOutput, ESPMode::ThreadSafe>;
class IMetaDataDecoderOutput;
using IMetaDataDecoderOutputPtr = TSharedPtr<IMetaDataDecoderOutput, ESPMode::ThreadSafe>;
class ISubtitleDecoderOutput;
using ISubtitleDecoderOutputPtr = TSharedPtr<ISubtitleDecoderOutput, ESPMode::ThreadSafe>;

// ---------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_TwoParams(FElectraPlayerSendAnalyticMetricsDelegate, const TSharedPtr<IAnalyticsProviderET>& /*AnalyticsProvider*/, const FGuid& /*PlayerGuid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FElectraPlayerSendAnalyticMetricsPerMinuteDelegate, const TSharedPtr<IAnalyticsProviderET>& /*AnalyticsProvider*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FElectraPlayerReportVideoStreamingErrorDelegate, const FGuid& /*PlayerGuid*/, const FString& /*LastError*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FElectraPlayerReportSubtitlesMetricsDelegate, const FGuid& /*PlayerGuid*/, const FString& /*URL*/, double /*ResponseTime*/, const FString& /*LastError*/);

// ---------------------------------------------------------------------------------------------

class IElectraPlayerAdapterDelegate : public TSharedFromThis<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>
{
public:
	virtual ~IElectraPlayerAdapterDelegate() {}

	enum class EOptionType
	{
		MaxVerticalStreamResolution = 0,
		MaxBandwidthForStreaming,
		PlayListData,
		LicenseKeyData,
		PlaystartPosFromSeekPositions,
		Android_VideoSurface,
	};
	virtual Electra::FVariantValue QueryOptions(EOptionType Type, const Electra::FVariantValue& Param = Electra::FVariantValue()) = 0;

	enum class EPlayerEvent
	{
		MediaBuffering = 0,
		MediaClosed,
		MediaConnecting,
		MediaOpened,
		MediaOpenFailed,
		PlaybackEndReached,
		PlaybackResumed,
		PlaybackSuspended,
		SeekCompleted,
		TracksChanged,

		Internal_Start,
		Internal_PurgeVideoSamplesHint = Internal_Start,
		Internal_ResetForDiscontinuity,
		Internal_RenderClockStart,
		Internal_RenderClockStop,
		Internal_VideoSamplesAvailable,
		Internal_VideoSamplesUnavailable,
		Internal_AudioSamplesAvailable,
		Internal_AudioSamplesUnavailable
	};
	virtual void SendMediaEvent(EPlayerEvent Event) = 0;
	virtual void OnVideoFlush() = 0;
	virtual void OnAudioFlush() = 0;
	virtual void OnSubtitleFlush() = 0;
	virtual void PresentVideoFrame(const FVideoDecoderOutputPtr& InVideoFrame) = 0;
	virtual void PresentAudioFrame(const IAudioDecoderOutputPtr& InAudioFrame) = 0;
	virtual void PresentSubtitleSample(const ISubtitleDecoderOutputPtr& InSubtitleSample) = 0;
	virtual void PresentMetadataSample(const IMetaDataDecoderOutputPtr& InMetadataSample) = 0;
	virtual bool CanReceiveVideoSamples(int32 NumFrames) = 0;
	virtual bool CanReceiveAudioSamples(int32 NumFrames) = 0;
	virtual void PrepareForDecoderShutdown() = 0;

	virtual FString GetVideoAdapterName() const = 0;

	virtual TSharedPtr<IElectraPlayerResourceDelegate, ESPMode::ThreadSafe> GetResourceDelegate() const = 0;
};

// Container class to be passed through options as shared pointer to allow passing any non-standard ref-counted entities to the player
class IOptionPointerValueContainer
{
public:
	virtual ~IOptionPointerValueContainer() {}
	virtual void* GetPointer() const = 0;
};

class IElectraPlayerInterface : public TSharedFromThis<IElectraPlayerInterface, ESPMode::ThreadSafe>
{
public:
	virtual ~IElectraPlayerInterface() {}

	virtual FString GetUrl() const = 0;

	virtual void SetGuid(const FGuid& Guid) = 0;

	// -------- PlayerAdapter (Plugin/Native) API

	struct FStreamSelectionAttributes
	{
		TOptional<FString> Kind;
		TOptional<FString> Language_ISO639;
		TOptional<FString> Codec;
		TOptional<int32> TrackIndexOverride;
		void Reset()
		{
			Kind.Reset();
			Language_ISO639.Reset();
			TrackIndexOverride.Reset();
		}
	};

	struct FPlaystartOptions
	{
		TOptional<FTimespan>		TimeOffset;
		FStreamSelectionAttributes	InitialVideoTrackAttributes;
		FStreamSelectionAttributes	InitialAudioTrackAttributes;
		FStreamSelectionAttributes	InitialSubtitleTrackAttributes;
		TOptional<int32>			MaxVerticalStreamResolution;
		TOptional<int32>			MaxBandwidthForStreaming;
		bool						bDoNotPreload = false;
	};

	virtual bool OpenInternal(const FString& Url, const Electra::FParamDict& PlayerOptions, const FPlaystartOptions& InPlaystartOptions) = 0;
	virtual void CloseInternal(bool bKillAfterClose) = 0;

	virtual void Tick(FTimespan DeltaTime, FTimespan Timecode) = 0;

	virtual bool IsKillAfterCloseAllowed() const = 0;

	enum class EPlayerState
	{
		Closed = 0,
		Error,
		Paused,
		Playing,
		Preparing,
		Stopped
	};

	enum class EPlayerStatus
	{
		None = 0x0,
		Buffering = 0x1,
		Connecting = 0x2
	};

	virtual EPlayerState GetState() const = 0;
	virtual EPlayerStatus GetStatus() const = 0;

	virtual bool IsLooping() const = 0;
	virtual bool SetLooping(bool bLooping) = 0;
	virtual int32 GetLoopCount() const = 0;

	virtual FTimespan GetTime() const = 0;
	virtual FTimespan GetDuration() const = 0;

	virtual bool IsLive() const = 0;
	virtual FTimespan GetSeekableDuration() const = 0;

	struct FPlaybackRange
	{
		TOptional<FTimespan> Start;
		TOptional<FTimespan> End;
	};
	virtual void SetPlaybackRange(const FPlaybackRange& InPlaybackRange) = 0;
	virtual void GetPlaybackRange(FPlaybackRange& OutPlaybackRange) const = 0;

	virtual float GetRate() const = 0;
	virtual bool SetRate(float Rate) = 0;


	struct FSeekParam
	{
		TOptional<int32> StartingBitrate;
		TOptional<bool> bOptimizeForScrubbing;
		TOptional<double> DistanceThreshold;
	};

	virtual bool Seek(const FTimespan& Time) = 0;
	virtual bool Seek(const FTimespan& Time, const FSeekParam& Param) = 0;
	virtual void SetFrameAccurateSeekMode(bool bEnableFrameAccuracy) = 0;
	
	virtual void ModifyOptions(const Electra::FParamDict& InOptionsToSetOrChange, const Electra::FParamDict& InOptionsToClear) = 0;

	struct FAudioTrackFormat
	{
		uint32 BitsPerSample;
		uint32 NumChannels;
		uint32 SampleRate;
		FString TypeName;
	};
	struct FVideoTrackFormat
	{
		FIntPoint Dim;
		float FrameRate;
		TRange<float> FrameRates;
		FString TypeName;
	};
	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FAudioTrackFormat& OutFormat) const = 0;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FVideoTrackFormat& OutFormat) const = 0;

	enum class EPlayerTrackType
	{
		Audio = 0,
		Caption,
		Metadata,
		Script,
		Subtitle,
		Text,
		Video
	};
	virtual int32 GetNumTracks(EPlayerTrackType TrackType) const = 0;
	virtual int32 GetNumTrackFormats(EPlayerTrackType TrackType, int32 TrackIndex) const = 0;
	virtual int32 GetSelectedTrack(EPlayerTrackType TrackType) const = 0;
	virtual FText GetTrackDisplayName(EPlayerTrackType TrackType, int32 TrackIndex) const = 0;
	virtual int32 GetTrackFormat(EPlayerTrackType TrackType, int32 TrackIndex) const = 0;
	virtual FString GetTrackLanguage(EPlayerTrackType TrackType, int32 TrackIndex) const = 0;
	virtual FString GetTrackName(EPlayerTrackType TrackType, int32 TrackIndex) const = 0;
	virtual bool SelectTrack(EPlayerTrackType TrackType, int32 TrackIndex) = 0;

	struct FVideoStreamFormat
	{
		FIntPoint Resolution;
		double FrameRate;
		int32 Bitrate;
	};
	virtual int32 GetNumVideoStreams(int32 TrackIndex) const = 0;
	virtual bool GetVideoStreamFormat(FVideoStreamFormat& OutFormat, int32 InTrackIndex, int32 InStreamIndex) const = 0;
	virtual bool GetActiveVideoStreamFormat(FVideoStreamFormat& OutFormat) const = 0;

	virtual void NotifyOfOptionChange() = 0;

	// Suspends or resumes decoder instances. Not supported on all platforms.
	virtual void SuspendOrResumeDecoders(bool bSuspend) = 0;

	enum
	{
		ResourceFlags_Decoder = 1 << 0,
		ResourceFlags_OutputBuffers = 1 << 1,

		ResourceFlags_All = (1 << 2) - 1,
		ResourceFlags_Any = ResourceFlags_All,
	};

	class IAsyncResourceReleaseNotifyContainer
	{
	public:
		virtual ~IAsyncResourceReleaseNotifyContainer() {}
		virtual void Signal(uint32 ResourceFlags) = 0;
	};
	virtual void SetAsyncResourceReleaseNotification(IAsyncResourceReleaseNotifyContainer* AsyncResourceReleaseNotification) = 0;
};

class ELECTRAPLAYERRUNTIME_API FElectraPlayerRuntimeFactory
{
public:
	static IElectraPlayerInterface* CreatePlayer(const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate,
												FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
												FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
												FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
												FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate);
};

class ELECTRAPLAYERRUNTIME_API FElectraPlayerPlatform
{
public:
	static bool StartupPlatformResources(const Electra::FParamDict & Params = Electra::FParamDict());
};
