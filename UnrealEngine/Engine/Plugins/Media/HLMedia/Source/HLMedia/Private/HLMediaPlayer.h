// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HLMediaPrivate.h"

#include "IMediaCache.h"
#include "IMediaPlayer.h"
#include "IMediaView.h"
#include "IMediaControls.h"
#include "IMediaEventSink.h"

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

#include "HLMediaPlayerTracks.h"

class FMediaSamples;
class FHLMediaPlayerTracks;

class FHLMediaPlayer
    : public IMediaPlayer
    , protected IMediaCache
    , protected IMediaControls
    , protected IMediaView
{
public:

	FHLMediaPlayer(IMediaEventSink& InEventSink);
    virtual ~FHLMediaPlayer();

    // IMediaPlayer
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
    virtual bool Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions) override;
    virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
    virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

protected:

    // IMediaControls
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

    bool InitializePlayer(const TSharedPtr<FArchive, ESPMode::ThreadSafe>& Archive, const FString& Url, bool Precache, const FMediaPlayerOptions* PlayerOptions);

private:

    mutable FCriticalSection CriticalSection;

    IMediaEventSink& EventSink;

    /** The URL of the currently opened media. */
    FString MediaUrl;

    /** Interop library object */
    TComPtr<HLMediaLibrary::IPlaybackEngine> PlaybackEngine;

	HLMediaLibrary::EventToken StateChangedEventToken;

    /** Media sample collection that receives the output. */
    TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

    /** tracks that represent the playback item */
    TSharedPtr<FHLMediaPlayerTracks, ESPMode::ThreadSafe> Tracks;
};
