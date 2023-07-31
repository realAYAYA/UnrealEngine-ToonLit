// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaSamples.h"
#include "IMediaTracks.h"
#include "IMediaView.h"

class FImgMediaLoader;
class FImgMediaScheduler;
class IImgMediaReader;
class IMediaEventSink;
class IMediaTextureSample;
class FImgMediaGlobalCache;


/**
 * Implements a media player for image sequences.
 */
class FImgMediaPlayer
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaSamples
	, protected IMediaTracks
	, public IMediaView
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 * @param InScheduler The image loading scheduler to use.
	 */
	FImgMediaPlayer(IMediaEventSink& InEventSink, const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
		const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache);

	/** Virtual destructor. */
	virtual ~FImgMediaPlayer();

	/** Get the loader. */
	TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> GetLoader() { return Loader; }

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FVariant GetMediaInfo(FName InfoName) const override;
	virtual FGuid GetPlayerPluginGUID() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual bool FlushOnSeekStarted() const;
	virtual bool FlushOnSeekCompleted() const;
	virtual void ProcessVideoSamples() override;
	virtual bool GetPlayerFeatureFlag(EFeatureFlag flag) const override;

protected:

	/**
	 * Check whether this player is initialized.
	 *
	 * @return true if initialized, false otherwise.
	 */
	bool IsInitialized() const;

protected:

	//~ IMediaCache interface

	virtual bool QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const override;

protected:

	//~ IMediaControls interface

	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual void SetBlockingPlaybackHint(bool bFacadeWillUseBlockingPlayback) override;

protected:

	//~ IMediaSamples interface

	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual void FlushSamples() override;

	virtual EFetchBestSampleResult FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp> & TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bReverse) override;
	virtual bool PeekVideoSampleTime(FMediaTimeStamp & TimeStamp) override;

protected:

	//~ IMediaTracks interface

	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:

	/** The duration of the currently loaded media. */
	FTimespan CurrentDuration;

	/** The current playback rate. */
	float CurrentRate;

	/** The player's current state. */
	EMediaState CurrentState;

	/** The current time of the playback. */
	FTimespan CurrentTime;

	/** The URL of the currently opened media. */
	FString CurrentUrl;

	/** Whether an offset to delta time has been applied yet. */
	bool DeltaTimeHackApplied;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Sample time of the last fetched video sample. */
	FTimespan LastFetchTime;

	/** The image sequence loader. */
	TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> Loader;

	/** If playback just restarted from the Stopped state. */
	bool PlaybackRestarted;

	/** The scheduler for image loading. */
	TSharedPtr<FImgMediaScheduler, ESPMode::ThreadSafe> Scheduler;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

	/** Should the video loop to the beginning at completion */
    bool ShouldLoop;

	/** The global cache to use. */
	TSharedPtr<FImgMediaGlobalCache, ESPMode::ThreadSafe> GlobalCache;

	/** True if we have run RequestFrame already for this frame. */
	bool RequestFrameHasRun;

	/** True if facade has signaled it uses blocking playback, false if not */
	bool PlaybackIsBlocking;
};
