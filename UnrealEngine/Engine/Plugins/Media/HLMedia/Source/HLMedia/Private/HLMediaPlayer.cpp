// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLMediaPlayer.h"

#include "Async/Async.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "MediaUtils/Public/MediaPlayerOptions.h"
#include "MediaUtils/Public/MediaSamples.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "RenderingThread.h"

#include "HLMediaLibrary.h"
#include "HLMediaPlayerTracks.h"

#include "ID3D12DynamicRHI.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <d3d11on12.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "HLMediaPlayer"

DEFINE_LOG_CATEGORY(LogHLMediaPlayer);

using namespace HLMediaLibrary;

// Sets default values for this component's properties
FHLMediaPlayer::FHLMediaPlayer(IMediaEventSink& InEventSink)
    : EventSink(InEventSink)
    , PlaybackEngine(nullptr)
    , Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
    , Tracks(MakeShared<FHLMediaPlayerTracks, ESPMode::ThreadSafe>())
{
}

FHLMediaPlayer::~FHLMediaPlayer()
{
    Close();
}

// IMediaPlayer
void FHLMediaPlayer::Close()
{
    if (GetState() == EMediaState::Closed)
    {
        return;
    }

	UE_LOG(LogHLMediaPlayer, Log, TEXT("HLMediaPlayer %p: Close."), this);

    if (PlaybackEngine != nullptr)
    {
		PlaybackEngine->StateChanged(StateChangedEventToken);

        PlaybackEngine->Stop();

        PlaybackEngine.Reset();

        PlaybackEngine = nullptr;
    }

    MediaUrl.Empty();

    // notify listeners
    EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
    EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

IMediaCache& FHLMediaPlayer::GetCache()
{
    return *this;
}

IMediaControls& FHLMediaPlayer::GetControls()
{
    return *this;
}

FString FHLMediaPlayer::GetInfo() const
{
    return GetUrl();
}

FGuid FHLMediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x6505c26f, 0xec614c0e, 0xb5be5be1, 0x57fac58e);
	return PlayerPluginGUID;
}

IMediaSamples& FHLMediaPlayer::GetSamples()
{
    check(Samples.IsValid());

    return *Samples.Get();
}

FString FHLMediaPlayer::GetStats() const
{
    FString Result;

    return Result;
}

IMediaTracks& FHLMediaPlayer::GetTracks()
{
    check(Tracks.IsValid());

    return *Tracks.Get();
}

FString FHLMediaPlayer::GetUrl() const
{
    return MediaUrl;
}

IMediaView& FHLMediaPlayer::GetView()
{
    return *this;
}

bool FHLMediaPlayer::Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions)
{
    Close();

    if (Url.IsEmpty())
    {
        return false;
    }

    const bool IsAdaptiveStreaming = (Options != nullptr) ? Options->GetMediaOption("IsAdaptiveSource", false) : false;

    return InitializePlayer(nullptr, Url, IsAdaptiveStreaming, PlayerOptions);
}

bool FHLMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
    return false;
//    return Open(Url, Options, nullptr);
}

bool FHLMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* /*Options*/)
{
    Close();

    if (Archive->TotalSize() == 0)
    {
        UE_LOG(LogHLMediaPlayer, Verbose, TEXT("HLMediaPlayer %p: Cannot open media from archive (archive is empty)."), this);
        return false;
    }

    if (OriginalUrl.IsEmpty())
    {
        UE_LOG(LogHLMediaPlayer, Verbose, TEXT("HLMediaPlayer %p: Cannot open media from archive (no original URL provided)."), this);
        return false;
    }

    return InitializePlayer(Archive, OriginalUrl, false, nullptr);
}

void FHLMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
    if (PlaybackEngine == nullptr)
    {
        return;
    }

    auto CurrentState = GetState();
    if (CurrentState == EMediaState::Paused || CurrentState == EMediaState::Playing)
    {
        struct FVideoSamplerTickParams
        {
            TWeakPtr<FHLMediaPlayerTracks, ESPMode::ThreadSafe> TracksPtr;
            FHLMediaPlayer* ThisPtr;
        }
        Params = { Tracks, this };

        ENQUEUE_RENDER_COMMAND(HLMediaPlayerTick)(
            [Params](FRHICommandListImmediate& RHICmdList)
        {
            auto This = Params.ThisPtr;
            auto PinnedTracks = Params.TracksPtr.Pin();
            if (PinnedTracks.IsValid())
            {
                PinnedTracks->AddVideoFrameSample(This->GetTime());
            }
        });
    }
}


// IMediaControls interface
bool FHLMediaPlayer::CanControl(EMediaControl Control) const
{
    if (PlaybackEngine == nullptr)
    {
        return false;
    }

    auto CurrentState = GetState();

    if (Control == EMediaControl::BlockOnFetch)
    {
        return (CurrentState == EMediaState::Paused || CurrentState == EMediaState::Playing);
    }

    if (Control == EMediaControl::Pause)
    {
        return (CurrentState == EMediaState::Playing && PlaybackEngine->CanPause());
    }

    if (Control == EMediaControl::Resume)
    {
        return (CurrentState != EMediaState::Playing);
    }

    if (Control == EMediaControl::Scrub)
    {
        return true;
    }

    if (Control == EMediaControl::Seek)
    {
        return PlaybackEngine->CanSeek();
    }

    return false;
}

FTimespan FHLMediaPlayer::GetDuration() const
{
    return PlaybackEngine != nullptr ? PlaybackEngine->Duration() : FTimespan::Zero();
}

float FHLMediaPlayer::GetRate() const
{
    return PlaybackEngine != nullptr ? PlaybackEngine->PlaybackRate() : 0.0f;
}

EMediaState FHLMediaPlayer::GetState() const
{
    if (PlaybackEngine == nullptr)
    {
        return EMediaState::Closed;
    }

    auto CurrentState = EMediaState::Closed;;

    switch (PlaybackEngine->State())
    {
    case 0:
        CurrentState = EMediaState::Closed;
        break;
    case 1:
        CurrentState = EMediaState::Preparing;
        break;
    case 2:
        CurrentState = EMediaState::Preparing;
        break;
    case 3:
        CurrentState = EMediaState::Playing;
        break;
    case 4:
        CurrentState = EMediaState::Paused;
        break;
    case 5:
        CurrentState = EMediaState::Stopped;
        break;
    }

    return CurrentState;
}

EMediaStatus FHLMediaPlayer::GetStatus() const
{
    if (PlaybackEngine == nullptr)
    {
        return EMediaStatus::None;
    }

    auto CurrentStatus = EMediaStatus::None;

    if (PlaybackEngine->State() == 1)
    {
        CurrentStatus = EMediaStatus::Connecting;
    }
    else if (PlaybackEngine->State() == 2)
    {
        CurrentStatus = EMediaStatus::Buffering;
    }

    return CurrentStatus;
}

TRangeSet<float> FHLMediaPlayer::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
    TRangeSet<float> Result;
    if (PlaybackEngine != nullptr)
    {
        Result.Add(TRange<float>::Inclusive(0.0f, 4.0f));
    }
    return Result;
}

FTimespan FHLMediaPlayer::GetTime() const
{
    return (PlaybackEngine != nullptr) ? PlaybackEngine->Position() : FTimespan::Zero();
}

bool FHLMediaPlayer::IsLooping() const
{
    return (PlaybackEngine != nullptr) ? PlaybackEngine->IsLooping() : false;
}

bool FHLMediaPlayer::Seek(const FTimespan& Time)
{
    auto result = (PlaybackEngine != nullptr) ? SUCCEEDED(PlaybackEngine->Seek(Time.GetTicks())) : false;

    EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

    return result;
}

bool FHLMediaPlayer::SetLooping(bool Looping)
{
    return (PlaybackEngine != nullptr) ? SUCCEEDED(PlaybackEngine->SetLooping(Looping)) : false;
}

bool FHLMediaPlayer::SetRate(float Rate)
{
    if (PlaybackEngine == nullptr)
    {
        UE_LOG(LogHLMediaPlayer, Warning, TEXT("HLMediaPlayer %p: Cannot set play rate while player is not ready"), this);

        return false;
    }

    if (FAILED(PlaybackEngine->PlaybackRate(Rate)))
    {
        return false;
    }

    auto CurrentState = GetState();

    // handle resume
    if (CurrentState != EMediaState::Playing && Rate != 0.0f)
    {
        return SUCCEEDED(PlaybackEngine->Play());
    }

    // handle pause
    if (CurrentState != EMediaState::Paused && Rate == 0.0f)
    {
        return SUCCEEDED(PlaybackEngine->Pause());
    }

    return true;
}


// FHLMediaPlayer
bool FHLMediaPlayer::InitializePlayer(const TSharedPtr<FArchive, ESPMode::ThreadSafe>& Archive, const FString& Url, bool IsAdaptiveStreaming, const FMediaPlayerOptions* PlayerOptions)
{
    UE_LOG(LogHLMediaPlayer, Verbose, TEXT("HLMediaPlayer %p: Initializing %s (archive = %s, adaptive streaming = %s)"), this, *Url, Archive.IsValid() ? TEXT("yes") : TEXT("no"), IsAdaptiveStreaming ? TEXT("yes") : TEXT("no"));

	if (!GIsRunning)
	{
		UE_LOG(LogHLMediaPlayer, Warning, L"Attempting to start FHLMediaPlayer before engine is fully initialized.");
		return false;
	}
	
    MediaUrl = Url;

    // initialize presentation on a separate thread
    const EAsyncExecution Execution = IsAdaptiveStreaming ? EAsyncExecution::Thread : EAsyncExecution::ThreadPool;
    Async(Execution, [this, Archive, Url, IsAdaptiveStreaming, PlayerOptions,
        SamplesPtr = TWeakPtr<FMediaSamples, ESPMode::ThreadSafe>(Samples),
        TracksPtr = TWeakPtr<FHLMediaPlayerTracks, ESPMode::ThreadSafe>(Tracks)]()
    {
        UE_LOG(LogHLMediaPlayer, Verbose, TEXT("HLMediaPlayer %p: FHLMediaPlayer::InitializePlayer::Async()"), this);

		ID3D11Device* Device = GetID3D11DynamicRHI()->RHIGetDevice();
		checkf(Device, TEXT("No available D3D11Device"));

		if (RHIGetInterfaceType() == ERHIInterfaceType::D3D12)
		{
			ID3D11Device* PrevDevice = Device;
			ID3D12Device* D3d12Device = GetID3D12DynamicRHI()->RHIGetDevice(0);
			ID3D12CommandQueue* CommandQueue = GetID3D12DynamicRHI()->RHIGetCommandQueue();

			ID3D11DeviceContext* D3d11DeviceContext;
			if (FAILED(D3D11On12CreateDevice(
				D3d12Device,
				D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				nullptr,
				0,
				reinterpret_cast<IUnknown**>(&CommandQueue),
				1,
				0,
				&Device,
				&D3d11DeviceContext,
				nullptr
			)))
			{
				UE_LOG(LogHLMediaPlayer, Error, TEXT("D3D11On12CreateDevice failed"));
				Device = PrevDevice;
			}
		}

		TComPtr<IPlaybackEngine> PlaybackEngineIn = nullptr;
        if (SUCCEEDED(CreatePlaybackEngine(Device, &PlaybackEngineIn)))
        {
            {
                FScopeLock Lock(&CriticalSection);

                PlaybackEngine = PlaybackEngineIn;
				StateChangedEventToken = PlaybackEngine->StateChanged([=](StateChangedArgs const& args)
				{
					switch (args.State)
					{
						case PlaybackState::SourceChanged:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed SourceChanged."), this);
							break;
						case PlaybackState::Opening:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Opening"), this);
							break;
						case PlaybackState::Opened:
							EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Opened."), this);
							break;
						case PlaybackState::Failed:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Failed: %d - 0x%lx."), this, args.Failed.Error, args.Failed.Result);
							break;
						case PlaybackState::Buffering:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Buffering %.2f%"), this, args.Progress.Value * 100);
							break;
						case PlaybackState::BufferingEnded:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Buffering Ended %.2f%."), this, args.Progress.Value * 100);
							break;
						case PlaybackState::Downloading:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Downloading %.2f%"), this, args.Progress.Value * 100);
							break;
						case PlaybackState::Playing:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Playing..."), this);
							break;
						case PlaybackState::Paused:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Paused."), this);
							break;
						case PlaybackState::ResolutionChanged:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Resolution changed %d x %d."), this, args.Video.Width, args.Video.Height);
							break;
						case PlaybackState::DurationChanged:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Duration change %d."), this, args.Duration.Value);
							break;
						case PlaybackState::RateChanged:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Rate changed."), this);
							break;
						case PlaybackState::Ended:
							UE_LOG(LogHLMediaPlayer, Display, TEXT("HLMediaPlayer %p: FHLMediaPlayer state changed Ended."), this);
							break;
					}
				});
            }

            TComPtr<IPlaybackEngineItem> PlaybackItem = nullptr;
            if (SUCCEEDED(PlaybackEngineIn->QueryInterface(&PlaybackItem)))
            {
                bool bPlayOnOpen = false;
                bool bLoop = false;
                if (PlayerOptions != nullptr)
                {
                    bPlayOnOpen = (PlayerOptions->PlayOnOpen != EMediaPlayerOptionBooleanOverride::UseMediaPlayerSetting) ? PlayerOptions->PlayOnOpen == EMediaPlayerOptionBooleanOverride::Enabled : false;

                    bLoop = (PlayerOptions->Loop != EMediaPlayerOptionBooleanOverride::UseMediaPlayerSetting) ? PlayerOptions->Loop == EMediaPlayerOptionBooleanOverride::Enabled : this->IsLooping();
                }

                if (SUCCEEDED(PlaybackItem->Load(bPlayOnOpen, bLoop, IsAdaptiveStreaming, *Url)))
                {
                    TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> PinnedSamples = SamplesPtr.Pin();
                    TSharedPtr<FHLMediaPlayerTracks, ESPMode::ThreadSafe> PinnedTracks = TracksPtr.Pin();
                    if (PinnedSamples.IsValid() && PinnedTracks.IsValid())
                    {
                        Tracks->Initialize(PlaybackItem, PinnedSamples.ToSharedRef(), PlayerOptions);
                    }

                    EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
                }
                else
                {
                    FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("HLMediaPlayerError_UrlError", "Could not load Url."));

                    EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
                }
            }
            else
            {
                EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
            }
        }
        else
        {
            FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("HLMediaPlayerError_EngineError", "Could not create Media Playback Engine component."));

            EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
        }
    });

    return true;
}

#undef LOCTEXT_NAMESPACE
