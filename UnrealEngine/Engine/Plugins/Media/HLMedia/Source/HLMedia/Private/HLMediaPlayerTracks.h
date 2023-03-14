// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HLMediaPrivate.h"

#include "IMediaTracks.h"

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Internationalization/Text.h"
#include "Math/IntPoint.h"
#include "Math/Range.h"
#include "Templates/Function.h"
#include "MediaUtils/Public/MediaPlayerOptions.h"

#include "HLMediaTextureSample.h"

class FMediaSamples;

struct FMediaPlayerOptions;

enum class EMediaTextureSampleFormat;

class FHLMediaPlayerTracks : public IMediaTracks
{
public:
	FHLMediaPlayerTracks();
    virtual ~FHLMediaPlayerTracks();

    void Shutdown();
    void Initialize(HLMediaLibrary::IPlaybackEngineItem* InPlaybackEngineItem, const TSharedRef<FMediaSamples, ESPMode::ThreadSafe>& InSamples, const FMediaPlayerOptions* PlayerOptions);
    void AddVideoFrameSample(FTimespan Time);

    // IMediaTracks interface
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
    /** media information */
    FString Info;

    /** The currently opened media. */
    TComPtr<HLMediaLibrary::IPlaybackEngineItem> PlaybackItem;

    /** Media sample collection that receives the output. */
    TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

    /** Video sample object pool. */
    TSharedPtr<FHLMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
};
