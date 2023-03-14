// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "PlaybackManager.h"

#include "SharedTexture.h"
#include "MediaHelpers.h"

#include <utility>
#include <winrt/windows.media.streaming.adaptive.h>
#include <pplawait.h>

using namespace winrt;
using namespace HLMediaLibrary;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Core;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Playback;
using namespace Windows::Media::Streaming::Adaptive;

static const int64_t CALLBACK_START_INDEX = 0xf0f0f0f0;

MediaSource CreateMediaSource(bool isManifest, PCWSTR sourceUrl)
{
    auto uri = Windows::Foundation::Uri(hstring(sourceUrl));

    MediaSource mediaSource = nullptr;

    if (isManifest)
    {
        auto result = AdaptiveMediaSource::CreateFromUriAsync(uri).get();

        if (result.Status() == AdaptiveMediaSourceCreationStatus::Success)
        {
            auto ams = result.MediaSource();

            auto availableBitrates = ams.AvailableBitrates();

            ams.InitialBitrate(availableBitrates.GetAt(availableBitrates.Size() - 1));

            mediaSource = MediaSource::CreateFromAdaptiveMediaSource(ams);
        }
    }
    else
    {
        mediaSource = MediaSource::CreateFromUri(uri);
    }

    return mediaSource;
}

_Use_decl_annotations_
HRESULT HLM_API HLMediaLibrary::CreatePlaybackEngine(
    ID3D11Device* pDevice,
    IPlaybackEngine** ppPlaybackEngine)
{
    NULL_CHK_HR(pDevice, E_INVALIDARG);
    NULL_CHK_HR(ppPlaybackEngine, E_POINTER);

    auto manager = winrt::make<PlaybackManager>();
    if (manager != nullptr)
    {
        IFR(manager.as<IPlaybackEnginePriv>()->Initialize(pDevice));

        IFR(manager->QueryInterface(ppPlaybackEngine));
    }
    else
    {
        (*ppPlaybackEngine) = nullptr;

        IFR(E_OUTOFMEMORY);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT HLM_API HLMediaLibrary::ValidateSourceUrl(bool isManifest, PCWSTR sourceUrl)
{
    HRESULT hr = S_OK;

    try
    {
        auto source = CreateMediaSource(isManifest, sourceUrl);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

PlaybackManager::PlaybackManager()
    : m_engineDevice(nullptr)
    , m_mediaDevice(nullptr)
    , m_dxgiDeviceManager(nullptr)
    , m_resetToken(0)
    , m_mediaPlaybackItem(nullptr)
    , m_sharedTexture(nullptr)
    , m_mediaPlayer(nullptr)
    , m_openedEventRevoker()
    , m_endedEventRevoker()
    , m_failedEventRevoker()
    , m_sourceChangedRevoker()
    , m_videoFrameAvailableEventRevoker()
    , m_mediaPlaybackSession(nullptr)
    , m_bufferingEndedRevoker()
    , m_bufferingProgressChangedRevoker()
    , m_downloadProgressChangedRevoker()
    , m_durationChangedRevoker()
    , m_videoSizeChangedRevoker()
    , m_rateChangedRevoker()
    , m_stateChangedRevoker()
    , m_callbackIndex(CALLBACK_START_INDEX)
    , m_statecallbacks()
{
}

PlaybackManager::~PlaybackManager()
{
    ReleaseMediaPlayer();

    ReleaseResources();
}

// IPlaybackEngineSource
_Use_decl_annotations_
IFACEMETHODIMP PlaybackManager::Load(
    bool const& autoPlay,
    bool const& loopingEnabled,
    bool const& isManifest,
    PCWSTR url)
{
    if (url == nullptr)
    {
        IFR(E_INVALIDARG);
    }

    if (m_mediaPlayer == nullptr)
    {
        IFR(CreateMediaPlayer());
    }

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.AutoPlay(autoPlay);

        m_mediaPlayer.IsLoopingEnabled(loopingEnabled);

        auto mediaSource = CreateMediaSource(isManifest, url);

        m_mediaPlaybackItem = MediaPlaybackItem(mediaSource);

        m_mediaPlayer.Source(m_mediaPlaybackItem);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

IFACEMETHODIMP_(int64_t) PlaybackManager::StartTime()
{
    return m_mediaPlaybackItem != nullptr ? m_mediaPlaybackItem.StartTime().count() : 0;
}

IFACEMETHODIMP_(uint32_t) PlaybackManager::AudioTrackCount()
{
    return m_mediaPlaybackItem != nullptr ? m_mediaPlaybackItem.AudioTracks().Size() : 0;
}

_Use_decl_annotations_
IFACEMETHODIMP_(AudioProperties) PlaybackManager::AudioTrack(uint32_t const& index)
{
    AudioProperties audio{};

    if (m_mediaPlaybackItem != nullptr)
    {
        auto tracks = m_mediaPlaybackItem.AudioTracks();

        if (tracks != nullptr && index < tracks.Size())
        {
            auto track = tracks.GetAt(index);
            if (track != nullptr)
            {
                auto props = track.GetEncodingProperties();
                if (props != nullptr)
                {
                    audio.Bitrate = props.Bitrate();
                    audio.BitsPerSample = props.BitsPerSample();
                    audio.ChannelCount = props.ChannelCount();
                    audio.IsSpatial = props.IsSpatial();
                    audio.SampleRate = props.SampleRate();
                    audio.Subtype = props.Subtype();
                    audio.Type = props.Type();
                }
            }
        }
    }

    return audio;
}

IFACEMETHODIMP_(int32_t) PlaybackManager::SelectedAudioTrack()
{
    int32_t selectedIndex = 0;

    if (m_mediaPlaybackItem != nullptr)
    {
        if (m_mediaPlaybackItem.AudioTracks() != nullptr)
        {
            selectedIndex = m_mediaPlaybackItem.AudioTracks().SelectedIndex();
        }
    }

    return selectedIndex;
}

_Use_decl_annotations_
IFACEMETHODIMP_(bool) PlaybackManager::SelectAudioTrack(int32_t const& index)
{
    NULL_CHK_HR(m_mediaPlaybackItem, MF_E_NOT_INITIALIZED);

    hresult hr = S_OK;

    try
    {
        m_mediaPlaybackItem.AudioTracks().SelectedIndex(index);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return SUCCEEDED(hr);
}

IFACEMETHODIMP_(uint32_t) PlaybackManager::VideoTrackCount()
{
    return m_mediaPlaybackItem != nullptr ? m_mediaPlaybackItem.VideoTracks().Size() : 0;
}

_Use_decl_annotations_
IFACEMETHODIMP_(VideoProperties) PlaybackManager::VideoTrack(uint32_t const& index)
{
    VideoProperties video{};

    if (m_mediaPlaybackItem != nullptr)
    {
        auto tracks = m_mediaPlaybackItem.VideoTracks();

        if (tracks != nullptr && index < tracks.Size())
        {
            auto track = tracks.GetAt(index);

            if (track != nullptr)
            {
                auto props = track.GetEncodingProperties();

                if (props != nullptr)
                {
                    video.Bitrate = props.Bitrate();
                    video.Numerator = props.FrameRate().Numerator();
                    video.Denominator = props.FrameRate().Denominator();
                    video.Height = props.Height();
                    video.AspectRatioNumerator = props.PixelAspectRatio().Numerator();
                    video.AspectRatioDenominator = props.PixelAspectRatio().Denominator();
                    video.SphericalFormat = static_cast<int32_t>(props.SphericalVideoFrameFormat());
                    video.SteroPackingMode = static_cast<int32_t>(props.StereoscopicVideoPackingMode());
                    video.Subtype = props.Subtype();
                    video.Type = props.Type();
                    video.Width = props.Width();
                }
            }
        }
    }

    return video;
}

IFACEMETHODIMP_(int32_t) PlaybackManager::SelectedVideoTrack()
{
    int32_t selectedIndex = 0;

    if (m_mediaPlaybackItem != nullptr)
    {
        if (m_mediaPlaybackItem.AudioTracks() != nullptr)
        {
            selectedIndex = m_mediaPlaybackItem.AudioTracks().SelectedIndex();
        }
    }

    return selectedIndex;
}

_Use_decl_annotations_
IFACEMETHODIMP_(bool) PlaybackManager::SelectVideoTrack(int32_t const& index)
{
    NULL_CHK_HR(m_mediaPlaybackItem, MF_E_NOT_INITIALIZED);

    hresult hr = S_OK;

    try
    {
        m_mediaPlaybackItem.VideoTracks().SelectedIndex(index);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return SUCCEEDED(hr);
}

IFACEMETHODIMP_(ISharedTexture*) PlaybackManager::VideoTexture()
{
    return m_sharedTexture.as<ISharedTexture>().get();
}


// IPlaybackEngine
IFACEMETHODIMP_(int32_t) PlaybackManager::State()
{
    return m_mediaPlaybackSession != nullptr ? static_cast<int32_t>(m_mediaPlaybackSession.PlaybackState()) : 0;
}

IFACEMETHODIMP_(bool) PlaybackManager::CanPause()
{
    return m_mediaPlaybackSession != nullptr ? m_mediaPlaybackSession.CanPause() : false;
}

IFACEMETHODIMP_(int64_t) PlaybackManager::Duration()
{
    return m_mediaPlaybackSession != nullptr ? m_mediaPlaybackSession.NaturalDuration().count() : 0;
}

IFACEMETHODIMP_(bool) PlaybackManager::IsLooping()
{
    return m_mediaPlayer != nullptr ? m_mediaPlayer.IsLoopingEnabled() : false;
}

_Use_decl_annotations_
IFACEMETHODIMP PlaybackManager::SetLooping(bool const& value)
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.IsLoopingEnabled(value);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

IFACEMETHODIMP_(bool) PlaybackManager::CanSeek()
{
    return m_mediaPlaybackSession != nullptr ? m_mediaPlaybackSession.CanSeek() : false;
}

_Use_decl_annotations_
IFACEMETHODIMP PlaybackManager::Seek(int64_t const& timestamp)
{
    NULL_CHK_HR(m_mediaPlaybackSession, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        auto position = TimeSpan(timestamp);

        m_mediaPlaybackSession.Position(position);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

IFACEMETHODIMP_(int64_t) PlaybackManager::Position()
{
    return  m_mediaPlaybackSession != nullptr ? m_mediaPlaybackSession.Position().count() : 0;
}

IFACEMETHODIMP_(double) PlaybackManager::PlaybackRate()
{
    return m_mediaPlaybackSession != nullptr ? static_cast<float>(m_mediaPlaybackSession.PlaybackRate()) : 0.0f;
}

_Use_decl_annotations_
IFACEMETHODIMP PlaybackManager::PlaybackRate(double const& rate)
{
    NULL_CHK_HR(m_mediaPlaybackItem, MF_E_NOT_INITIALIZED);
    NULL_CHK_HR(m_mediaPlaybackSession, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlaybackSession.PlaybackRate(rate);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}


IFACEMETHODIMP PlaybackManager::Play()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Play();
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

IFACEMETHODIMP PlaybackManager::Pause()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Pause();
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}

IFACEMETHODIMP PlaybackManager::Stop()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Source(nullptr);
    }
    catch (hresult_error const& e)
    {
        hr = e.code();
    }

    return hr;
}


IFACEMETHODIMP_(EventToken) PlaybackManager::StateChanged(StateChangedCallback const& callback)
{
    auto index = m_callbackIndex;

    auto pair = std::pair<int64_t, StateChangedCallback>(index, callback);

    auto result = m_statecallbacks.emplace(pair);

    if (result.second)
    {
        m_callbackIndex++;
    }

    return EventToken{ index };
}

IFACEMETHODIMP_(void) PlaybackManager::StateChanged(EventToken const& token)
{
    auto iter = m_statecallbacks.find(token.value);
    if (iter == m_statecallbacks.end())
    {
        return;
    }

    iter->second = nullptr;

    m_statecallbacks.erase(iter);
}


_Use_decl_annotations_
HRESULT PlaybackManager::Initialize(
    ID3D11Device* pDevice)
{
    NULL_CHK_HR(pDevice, E_INVALIDARG);

    com_ptr<IDXGIDevice> dxgiDevice = nullptr;
    IFR(pDevice->QueryInterface(
        guid_of<IDXGIDevice>(), 
        dxgiDevice.put_void()));

    // make sure we have created our own d3d device
    IFR(CreateResources(dxgiDevice));

    IFR(CreateMediaPlayer());

    return S_OK;
}

HRESULT PlaybackManager::CreateMediaPlayer()
{
    if (m_mediaPlayer != nullptr)
    {
        ReleaseMediaPlayer();
    }

    auto strong = get_strong();

    m_mediaPlayer = Windows::Media::Playback::MediaPlayer();

    m_openedEventRevoker = m_mediaPlayer.MediaOpened(winrt::auto_revoke, [this, strong](MediaPlayer const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::Opened;

        Callback(stateCallbackArgs);
    });

    m_failedEventRevoker = m_mediaPlayer.MediaFailed(winrt::auto_revoke, [this, strong](MediaPlayer const& sender, MediaPlayerFailedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::Failed;
        stateCallbackArgs.Failed.Error = static_cast<FailedError>(args.Error());
        stateCallbackArgs.Failed.Result = args.ExtendedErrorCode();

        Callback(stateCallbackArgs);
    });

    m_endedEventRevoker = m_mediaPlayer.MediaEnded(winrt::auto_revoke, [this, strong](MediaPlayer const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::Ended;

        Callback(stateCallbackArgs);
    });

    m_sourceChangedRevoker = m_mediaPlayer.SourceChanged(winrt::auto_revoke, [this, strong](MediaPlayer const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::SourceChanged;

        Callback(stateCallbackArgs);
    });

    // set frameserver mode for video
    m_mediaPlayer.IsVideoFrameServerEnabled(true);
    m_videoFrameAvailableEventRevoker = m_mediaPlayer.VideoFrameAvailable(winrt::auto_revoke, [this, strong](MediaPlayer const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_sharedTexture == nullptr || m_sharedTexture->MediaSurface() == nullptr)
        {
            return;
        }

        sender.CopyFrameToVideoSurface(m_sharedTexture->MediaSurface());
    });

    m_mediaPlaybackSession = m_mediaPlayer.PlaybackSession();

    m_bufferingEndedRevoker = m_mediaPlaybackSession.BufferingEnded(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::BufferingEnded;
        stateCallbackArgs.Progress.Value = sender.BufferingProgress();
        Callback(stateCallbackArgs);
    });

    m_bufferingProgressChangedRevoker = m_mediaPlaybackSession.BufferingProgressChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::Buffering;
        stateCallbackArgs.Progress.Value = sender.BufferingProgress();
        Callback(stateCallbackArgs);
    });

    m_downloadProgressChangedRevoker = m_mediaPlaybackSession.DownloadProgressChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::Downloading;
        stateCallbackArgs.Progress.Value = sender.DownloadProgress();
        Callback(stateCallbackArgs);
    });

    m_videoSizeChangedRevoker = m_mediaPlaybackSession.NaturalVideoSizeChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::ResolutionChanged;
        stateCallbackArgs.Video.Width = sender.NaturalVideoWidth();
        stateCallbackArgs.Video.Height = sender.NaturalVideoHeight();

        Callback(stateCallbackArgs);

        // todo: remove when moving texture creation to caller
        auto width = m_mediaPlaybackSession.NaturalVideoWidth();
        auto height = m_mediaPlaybackSession.NaturalVideoHeight();
        if (width > 0 && height > 0)
        {
            com_ptr<ISharedTexture> sharedTexture = nullptr;
            if (SUCCEEDED(SharedTexture::Create(m_engineDevice, m_dxgiDeviceManager, width, height, DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM, sharedTexture)))
            {
                m_sharedTexture = sharedTexture;
            }
        }
    });

    m_durationChangedRevoker = m_mediaPlaybackSession.NaturalDurationChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::DurationChanged;
        stateCallbackArgs.Duration.Value = sender.NaturalDuration().count();
        Callback(stateCallbackArgs);
    });

    m_rateChangedRevoker = m_mediaPlaybackSession.PlaybackRateChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        stateCallbackArgs.State = PlaybackState::RateChanged;
        Callback(stateCallbackArgs);
    });

    m_stateChangedRevoker = m_mediaPlaybackSession.PlaybackStateChanged(winrt::auto_revoke, [this, strong](MediaPlaybackSession const& sender, IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto stateCallbackArgs = StateChangedArgs();
        switch (sender.PlaybackState())
        {
        case MediaPlaybackState::Buffering:
            stateCallbackArgs.State = PlaybackState::Buffering;
            stateCallbackArgs.Progress.Value = sender.BufferingProgress();
            break;
        case MediaPlaybackState::Opening:
            stateCallbackArgs.State = PlaybackState::Opening;
            break;
        case MediaPlaybackState::Paused:
            stateCallbackArgs.State = PlaybackState::Paused;
            break;
        case MediaPlaybackState::Playing:
            stateCallbackArgs.State = PlaybackState::Playing;
            break;
        default:
            stateCallbackArgs.State = PlaybackState::None;
        }

        Callback(stateCallbackArgs);
    });

    return S_OK;
}

void PlaybackManager::ReleaseMediaPlayer()
{
    Stop();

    if (CALLBACK_START_INDEX != m_callbackIndex)
    {
        m_statecallbacks.clear();

        m_callbackIndex = CALLBACK_START_INDEX;
    }

    if (m_sharedTexture != nullptr)
    {
        m_sharedTexture = nullptr;
    }

    if (m_mediaPlaybackSession != nullptr)
    {
        m_bufferingEndedRevoker.revoke();
        m_bufferingProgressChangedRevoker.revoke();
        m_downloadProgressChangedRevoker.revoke();
        m_videoSizeChangedRevoker.revoke();
        m_stateChangedRevoker.revoke();
        m_mediaPlaybackSession = nullptr;
    }

    if (m_mediaPlayer != nullptr)
    {
        m_openedEventRevoker.revoke();
        m_endedEventRevoker.revoke();
        m_failedEventRevoker.revoke();
        m_sourceChangedRevoker.revoke();
        m_videoFrameAvailableEventRevoker.revoke();

        m_mediaPlayer = nullptr;
    }

    if (m_mediaPlaybackItem != nullptr)
    {
        m_mediaPlaybackItem = nullptr;
    }
}

_Use_decl_annotations_
HRESULT PlaybackManager::CreateResources(
    com_ptr<IDXGIDevice> const& dxgiDevice)
{
    if (m_mediaDevice != nullptr)
    {
        return S_OK;
    }

    NULL_CHK_HR(dxgiDevice, E_INVALIDARG);

    com_ptr<IDXGIAdapter> dxgiAdapter = nullptr;
    IFR(dxgiDevice->GetAdapter(dxgiAdapter.put()));

    // create dx device for media pipeline
    com_ptr<ID3D11Device> mediaDevice = nullptr;
    IFR(CreateMediaDevice(dxgiAdapter.get(), mediaDevice.put()));

    // create DXGIManager
    uint32_t resetToken;
    com_ptr<IMFDXGIDeviceManager> dxgiDeviceManager = nullptr;
    IFR(MFCreateDXGIDeviceManager(&resetToken, dxgiDeviceManager.put()));

    // associtate the device with the manager
    IFR(dxgiDeviceManager->ResetDevice(mediaDevice.get(), resetToken));

    // setup is complete, store objects
    m_engineDevice = dxgiDevice.as<ID3D11Device>();
    m_mediaDevice.attach(mediaDevice.detach());
    m_dxgiDeviceManager.attach(dxgiDeviceManager.detach());
    m_resetToken = resetToken;

    return S_OK;
}

void PlaybackManager::ReleaseResources()
{
    ReleaseMediaPlayer();

    if (m_dxgiDeviceManager != nullptr)
    {
        if (m_mediaDevice != nullptr)
        {
            m_dxgiDeviceManager->ResetDevice(nullptr, m_resetToken);

            m_mediaDevice = nullptr;
        }

        m_dxgiDeviceManager = nullptr;
    }
}

void PlaybackManager::Callback(StateChangedArgs const& args)
{
    for (auto const& pair : m_statecallbacks)
    {
        auto callback = pair.second;
        if (callback != nullptr)
        {
            pair.second(args);
        }
    }
}
