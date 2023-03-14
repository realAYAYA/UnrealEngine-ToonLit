// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraPlayerInterface.h"

#include "Containers/UnrealString.h"
#include "Containers/Queue.h"
#include "Misc/Guid.h"
#include "IMediaCache.h"
#include "IMediaPlayer.h"
#include "IMediaView.h"
#include "IMediaControls.h"
#include "IMediaTracks.h"
#include "IMediaEventSink.h"
#include "IMediaPlayerLifecycleManager.h"
#include "MediaSampleQueue.h"
#include "Templates/SharedPointer.h"
#include "Logging/LogMacros.h"
#include "ElectraTextureSample.h"
#include "ElectraPlayerAudioSample.h"

class IElectraPlayerRuntimeModule;
class IElectraSafeMediaOptionInterface;
class FElectraPlayerResourceDelegate;


DECLARE_LOG_CATEGORY_EXTERN(LogElectraPlayerPlugin, Log, All);

//-----------------------------------------------------------------------------

class IMediaOptions;
class FMediaSamples;
class FArchive;

/**
	* Implements a media player
	* Supports multiple platforms.
	*/
class FElectraPlayerPlugin
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaView
	, protected IMediaControls
	, public IMediaTracks
	, public TSharedFromThis<FElectraPlayerPlugin, ESPMode::ThreadSafe>
{
public:

	FElectraPlayerPlugin();

	virtual ~FElectraPlayerPlugin();

	bool Initialize(IMediaEventSink& InEventSink,
		FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
		FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
		FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
		FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate);

public:
	// IMediaPlayer impl

	void Close() override;

	IMediaCache& GetCache() override
	{
		return *this;
	}

	IMediaControls& GetControls() override
	{
		return *this;
	}

	FString GetUrl() const override
	{
		return Player->GetUrl();
	}

	IMediaView& GetView() override
	{
		return *this;
	}

	void SetGuid(const FGuid& Guid) override
	{
		Player->SetGuid(Guid);
	}

	FString GetInfo() const override;
	FGuid GetPlayerPluginGUID() const override;
	IMediaSamples& GetSamples() override;
	FString GetStats() const override;
	IMediaTracks& GetTracks() override;
	bool Open(const FString& Url, const IMediaOptions* Options) override;
	bool Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions) override;
	bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
	void SetLastAudioRenderedSampleTime(FTimespan SampleTime) override;
	bool FlushOnSeekStarted() const override
	{
		return true;
	}
	bool FlushOnSeekCompleted() const override
	{
		return false;
	}

	bool GetPlayerFeatureFlag(EFeatureFlag flag) const override;

	bool SetAsyncResourceReleaseNotification(IAsyncResourceReleaseNotificationRef AsyncResourceReleaseNotification) override;
	uint32 GetNewResourcesOnOpen() const override;

private:
	friend class FElectraPlayerPluginModule;
	friend class FElectraPlayerResourceDelegate;

	// IMediaControls impl
	bool CanControl(EMediaControl Control) const override;

	FTimespan GetDuration() const override;

	bool IsLooping() const override;
	bool SetLooping(bool bLooping) override;

	EMediaState GetState() const override;
	EMediaStatus GetStatus() const override;

	TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	FTimespan GetTime() const override;
	float GetRate() const override;
	bool SetRate(float Rate) override;
	bool Seek(const FTimespan& Time) override;


	// From IMediaTracks
	bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	int32 GetNumTracks(EMediaTrackType TrackType) const override;
	int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;
	bool SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate) override;

	IElectraPlayerResourceDelegate* PlatformCreatePlayerResourceDelegate();
	static void PlatformSetupResourceParams(Electra::FParamDict& Params);

	// IElectraPlayerAdapterDelegate impl
	class FPlayerAdapterDelegate : public IElectraPlayerAdapterDelegate
	{
	public:
		FPlayerAdapterDelegate(const TSharedPtr<FElectraPlayerPlugin, ESPMode::ThreadSafe> & InHost) : Host(InHost) {}

		Electra::FVariantValue QueryOptions(EOptionType Type, const Electra::FVariantValue & Param) override;
		void SendMediaEvent(EPlayerEvent Event) override;
		void OnVideoFlush() override;
		void OnAudioFlush() override;
		void OnSubtitleFlush() override;
		void PresentVideoFrame(const FVideoDecoderOutputPtr& InVideoFrame) override;
		void PresentAudioFrame(const IAudioDecoderOutputPtr& InAudioFrame) override;
		void PresentSubtitleSample(const ISubtitleDecoderOutputPtr& InSubtitleSample) override;
		void PresentMetadataSample(const IMetaDataDecoderOutputPtr& InMetadataSample) override;
		bool CanReceiveVideoSamples(int32 NumFrames) override;
		bool CanReceiveAudioSamples(int32 NumFrames) override;
		void PrepareForDecoderShutdown() override;
		FString GetVideoAdapterName() const override;
		TSharedPtr<IElectraPlayerResourceDelegate, ESPMode::ThreadSafe> GetResourceDelegate() const override;

	private:
		TWeakPtr<FElectraPlayerPlugin, ESPMode::ThreadSafe> Host;
	};
	friend class FPlayerAdapterDelegate;
	TSharedPtr<FPlayerAdapterDelegate, ESPMode::ThreadSafe> PlayerDelegate;


private:
	/** Output queues as needed by MediaFramework */
	TUniquePtr<FMediaSamples> MediaSamples;

	/** Lock to guard the POD callback pointers from being changed while being used. */
	FCriticalSection CallbackPointerLock;

	/** Option interface */
	TWeakPtr<IElectraSafeMediaOptionInterface, ESPMode::ThreadSafe> OptionInterface;

	/** The media event handler */
	IMediaEventSink* EventSink = nullptr;

	/** The actual player */
	TSharedPtr<IElectraPlayerInterface, ESPMode::ThreadSafe> Player;

	/** Output sample pools */
	TSharedPtr<FElectraTextureSamplePool, ESPMode::ThreadSafe> OutputTexturePool;
	FElectraPlayerAudioSamplePool OutputAudioPool;

	TSharedPtr<IElectraPlayerResourceDelegate,ESPMode::ThreadSafe> PlayerResourceDelegate;
};
