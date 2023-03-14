// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLMediaPlayerTracks.h"

#include "MediaUtils/Public/MediaSamples.h"

using namespace HLMediaLibrary;

#define LOCTEXT_NAMESPACE "HLMediaPlayerTracks"

FHLMediaPlayerTracks::FHLMediaPlayerTracks()
{
}

FHLMediaPlayerTracks::~FHLMediaPlayerTracks()
{
    Shutdown();
}

void FHLMediaPlayerTracks::Shutdown()
{
    VideoSample.Reset();

    Info.Empty();

    Samples.Reset();

    PlaybackItem.Reset();
}

void FHLMediaPlayerTracks::Initialize(
    IPlaybackEngineItem* InPlaybackEngineItem,
    const TSharedRef<FMediaSamples, ESPMode::ThreadSafe>& InSamples,
    const FMediaPlayerOptions* PlayerOptions)
{
    Shutdown();

    UE_LOG(LogHLMediaPlayer, Verbose, TEXT("Tracks: %p: Initializing tracks"), this);

    if (InPlaybackEngineItem == nullptr)
    {
        return;
    }

    PlaybackItem = InPlaybackEngineItem;

    Samples = InSamples;

    if (PlayerOptions)
    {
        // Select tracks based on the options provided
        SelectTrack(EMediaTrackType::Audio, PlayerOptions->Tracks.Audio);
        SelectTrack(EMediaTrackType::Video, PlayerOptions->Tracks.Video);
    }
    else
    {
        // Select default tracks
        SelectTrack(EMediaTrackType::Audio, 0);
        SelectTrack(EMediaTrackType::Video, 0);
    }

}

void FHLMediaPlayerTracks::AddVideoFrameSample(FTimespan Time)
{
    check(IsInRenderingThread());

    if (PlaybackItem == nullptr || PlaybackItem->VideoTexture() == nullptr)
    {
        return;
    }

    auto SharedTexture = PlaybackItem->VideoTexture();

    if (!VideoSample.IsValid() ||
        VideoSample->GetTexture() == nullptr)
    {
        VideoSample = MakeShared<FHLMediaTextureSample, ESPMode::ThreadSafe>(SharedTexture->Texture2D(), SharedTexture->ShaderResourceView(), SharedTexture->SharedTextureHandle());

        auto Track = PlaybackItem->VideoTrack(PlaybackItem->SelectedVideoTrack());

        double FPS = 30.0;

        if (Track.Denominator != 0)
        {
            FPS = static_cast<double>(Track.Numerator) / static_cast<double>(Track.Denominator);
        }

        auto Duration = FTimespan::FromSeconds(1.0 / FPS);

        VideoSample->Update(Time, Duration);
        Samples->AddVideo(VideoSample.ToSharedRef());
    }


    auto Dim = VideoSample->GetDim();

    auto Desc = SharedTexture->Texture2DDesc();

    if (Dim.X != Desc.Width
        ||
        Dim.Y != Desc.Height)
    {
        VideoSample->Reset();
    }
}


bool FHLMediaPlayerTracks::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
    if (FormatIndex != 0 || !PlaybackItem.IsValid())
    {
        return false;
    }

    const auto& Track = PlaybackItem->AudioTrack(TrackIndex);

    OutFormat.BitsPerSample = Track.BitsPerSample;
    OutFormat.NumChannels = Track.ChannelCount;
    OutFormat.SampleRate = Track.SampleRate;
    OutFormat.TypeName = Track.Subtype.c_str();

    return true;
}

int32 FHLMediaPlayerTracks::GetNumTracks(EMediaTrackType TrackType) const
{
    switch (TrackType)
    {
    case EMediaTrackType::Audio:
        return PlaybackItem != nullptr ? PlaybackItem->AudioTrackCount() : 0;
    case EMediaTrackType::Video:
        return PlaybackItem != nullptr ? PlaybackItem->VideoTrackCount() : 0;
    default:
        return 0;
    }
}

int32 FHLMediaPlayerTracks::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
    return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}

int32 FHLMediaPlayerTracks::GetSelectedTrack(EMediaTrackType TrackType) const
{
    if (PlaybackItem.IsValid())
    {
        switch (TrackType)
        {
        case EMediaTrackType::Audio:
            return PlaybackItem->SelectedAudioTrack();

        case EMediaTrackType::Video:
            return PlaybackItem->SelectedVideoTrack();
        }
    }

    return INDEX_NONE;
}

FText FHLMediaPlayerTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
    FText DisplayName = FText::GetEmpty();

    if (PlaybackItem != nullptr)
    {
        switch (TrackType)
        {
        case EMediaTrackType::Audio:
            if (TrackIndex < static_cast<int32>(PlaybackItem->AudioTrackCount()))
            {
                DisplayName = FText::Format(LOCTEXT("AudioTrackName", "Audio Track {0}"), FText::AsNumber(TrackIndex));
            }
            break;
        case EMediaTrackType::Video:
            if (TrackIndex < static_cast<int32>(PlaybackItem->VideoTrackCount()))
            {
                DisplayName = FText::Format(LOCTEXT("VideoTrackName", "Video Track {0}"), FText::AsNumber(TrackIndex));
            }
            break;
        }
    }

    return DisplayName;
}

int32 FHLMediaPlayerTracks::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
    return 0; // only one format per trackindex
}

FString FHLMediaPlayerTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
    FString Language = FString();

    return Language;
}

FString FHLMediaPlayerTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
    FString TrackName = TEXT("Invalid track request");

    if (PlaybackItem != nullptr)
    {
        switch (TrackType)
        {
        case EMediaTrackType::Audio:
            if (TrackIndex < static_cast<int32>(PlaybackItem->AudioTrackCount()))
            {
                TrackName = FString::Printf(TEXT("Audio Track {0}")), FText::AsNumber(TrackIndex);
            }
            break;
        case EMediaTrackType::Video:
            if (TrackIndex < static_cast<int32>(PlaybackItem->VideoTrackCount()))
            {
                TrackName = FString::Printf(TEXT("Video Track {0}")), FText::AsNumber(TrackIndex);
            }
            break;
        }
    }

    return TrackName;
}

bool FHLMediaPlayerTracks::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
    if (PlaybackItem.IsValid() && TrackIndex < static_cast<int32>(PlaybackItem->VideoTrackCount()) && FormatIndex == 0)
    {
        auto video = PlaybackItem->VideoTrack(TrackIndex);

        OutFormat.Dim = FIntPoint{ static_cast<int32>(video.Width), static_cast<int32>(video.Height) };
		if (video.Denominator != 0)
		{
			OutFormat.FrameRate = (static_cast<float>(video.Numerator) / static_cast<float>(video.Denominator));
		}
		else
		{
			OutFormat.FrameRate = 0.0f;
		}

        OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
        OutFormat.TypeName = video.Subtype.c_str();

        return true;
    }
    else
    {
        return false;
    }
}

bool FHLMediaPlayerTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
    if (!PlaybackItem.IsValid())
    {
        return false;
    }

    // select stream for new track
    if (TrackIndex != INDEX_NONE)
    {
        if (TrackType == EMediaTrackType::Audio)
        {
            return PlaybackItem->SelectAudioTrack(TrackIndex);
        }
        else if(TrackType == EMediaTrackType::Video)
        {
            return PlaybackItem->SelectVideoTrack(TrackIndex);
        }
    }

    return false;
}

bool FHLMediaPlayerTracks::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
    if (FormatIndex != 0 || !PlaybackItem.IsValid())
    {
        return false;
    }

    switch (TrackType)
    {
    case EMediaTrackType::Audio:
        return TrackIndex < static_cast<int32>(PlaybackItem->AudioTrackCount());

    case EMediaTrackType::Video:
        return TrackIndex < static_cast<int32>(PlaybackItem->VideoTrackCount());
    }

    return false;
}

#undef LOCTEXT_NAMESPACE
