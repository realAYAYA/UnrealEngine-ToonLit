// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Containers/Queue.h"
#include "ElectraTextureSample.h"
#include "IMediaCache.h"
#include "IMediaPlayer.h"
#include "IMediaView.h"
#include "IMediaControls.h"
#include "IMediaTracks.h"
#include "IMediaEventSink.h"
#include "ITextureMediaPlayer.h"
#include "MediaSampleQueue.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "TextureMediaPlayerPrivate.h"

class IMediaOptions;
class FMediaSamples;
class FArchive;

/**
 * Implements a media player that can handle Texture.
 */
class FTextureMediaPlayer
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaView
	, protected IMediaControls
	, public IMediaTracks
	, public ITextureMediaPlayer
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	**/
	FTextureMediaPlayer(IMediaEventSink& InEventSink);

	virtual ~FTextureMediaPlayer();

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
		return MediaUrl;
	}

	IMediaView& GetView() override
	{
		return *this;
	}

	void SetGuid(const FGuid& Guid) override
	{
		PlayerGuid = Guid;
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

	FTimespan GetTime() const override;

	// ITextureMediaPlayer interface
	virtual void OnFrame(const TArray<uint8>& TextureBuffer, FIntPoint Size) override; // TODO (aidan.possemiers) 
#if PLATFORM_WINDOWS
	virtual void OnFrame(FTextureRHIRef TextureRHIRef, TRefCountPtr<ID3D12Fence> D3DFence, uint64 FenceValue) override;
#else
	virtual void OnFrame(FTextureRHIRef TextureRHIRef, FGPUFenceRHIRef Fence, uint64 FenceValue) override { unimplemented(); }; // TODO (aidan.possemiers) 
#endif // PLATFORM_WINDOWS

private:

	void CloseInternal(bool bKillAfterClose);

	// -------------------------------------------------------------------------------------------------------------------------

	// IMediaControls impl
	bool CanControl(EMediaControl Control) const override;

	FTimespan GetDuration() const override;

	bool IsLooping() const override
	{
		return false;
	}

	bool SetLooping(bool bLooping) override
	{
		return bLooping == false;
	}

	EMediaState GetState() const override;
	EMediaStatus GetStatus() const override;

	TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	float GetRate() const override;
	bool SetRate(float Rate) override;
	bool Seek(const FTimespan& Time) override;

	// From IMediaTracks
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
	virtual bool SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate);

private:
	/** The media event handler. */
	IMediaEventSink&								EventSink;
	/** Media player Guid */
	FGuid											PlayerGuid;

	/** Option interface **/
	const IMediaOptions*							OptionInterface;

	/**  */
	TAtomic<EMediaState>							State;
	TAtomic<EMediaStatus>							Status;
	bool											bWasClosedOnError;
	bool											bAllowKillAfterCloseEvent;

	/** Queued events */
	TQueue<EMediaEvent>								DeferredEvents;

	/** The URL of the currently opened media. */
	FString											MediaUrl;

	int32											NumTracksVideo;

	FCriticalSection												MediaSamplesAccessLock;
	TUniquePtr<FMediaSamples>										MediaSamples;

	FElectraTextureSamplePool										OutputTexturePool;

	FTimespan														CurrentTime;
	FTimespan														TimeSinceLastFrameReceived;
	uint32															CurrentFrameCount;
	uint32															MostRecentFrameCount;
};

