// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBM_LIBS

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "IMediaEventSink.h"
#include "Misc/Timespan.h"
#include "MkvFileReader.h"
#include "WebMSamplesSink.h"

class FMediaSamples;
class FWebMVideoDecoder;
class FWebMAudioDecoder;

class FWebMMediaPlayer
	: public IMediaPlayer
	, public IWebMSamplesSink
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaTracks
	, protected IMediaView
{
public:
	FWebMMediaPlayer(IMediaEventSink& InEventSink);
	virtual ~FWebMMediaPlayer();

public:
	//~ IWebMSamplesSink interface
	virtual void AddVideoSampleFromDecodingThread(TSharedRef<FWebMMediaTextureSample, ESPMode::ThreadSafe> Sample) override;
	virtual void AddAudioSampleFromDecodingThread(TSharedRef<FWebMMediaAudioSample, ESPMode::ThreadSafe> Sample) override;

public:
	//~ IMediaPlayer interface
	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FGuid GetPlayerPluginGUID() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

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
	TArray<EMediaEvent> OutEvents;
	IMediaEventSink& EventSink;
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TUniquePtr<FWebMVideoDecoder> VideoDecoder;
	TUniquePtr<FWebMAudioDecoder> AudioDecoder;
	TArray<const mkvparser::VideoTrack*> VideoTracks;
	TArray<const mkvparser::AudioTrack*> AudioTracks;
	TUniquePtr<FMkvFileReader> MkvReader;
	TUniquePtr<mkvparser::Segment> MkvSegment;
	const mkvparser::Cluster* MkvCurrentCluster;
	const mkvparser::BlockEntry* MkvCurrentBlockEntry;
	FString MediaUrl;
	EMediaState CurrentState;
	int32 SelectedAudioTrack;
	int32 SelectedVideoTrack;
	FTimespan CurrentTime;
	bool bNoMoreToRead;
	bool bLooping;

private:
	void Resume();
	void Pause();
	void Stop();
	bool MkvRead();
	void MkvSeekToNextValidBlock();
	void MkvSeekToTime(const FTimespan& Time);
};

#endif // WITH_WEBM_LIBS
