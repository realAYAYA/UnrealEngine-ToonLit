// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "HLMediaLibrary.h"
#include "SharedTexture.h"
#include <vector>

namespace HLMediaLibrary
{
    struct __declspec(uuid("9f8b0d7c-9f6e-4ca2-9067-2fef484f28f5")) IPlaybackEnginePriv : ::IUnknown
    {
        STDMETHOD(Initialize)(_In_ ID3D11Device*) PURE;
    };

    struct PlaybackManager : winrt::implements<PlaybackManager, IPlaybackEngineItem, IPlaybackEngine, IPlaybackEnginePriv>
    {
        PlaybackManager();

        virtual ~PlaybackManager();

        // IPlaybackEngineItem
        IFACEMETHOD(Load)(_In_ bool const& autoPlay, _In_ bool const& loopingEnabled, _In_ bool const& isAdaptiveStreaming, _In_ PCWSTR url);
        IFACEMETHOD_(int64_t, StartTime)();
        IFACEMETHOD_(uint32_t, AudioTrackCount)();
        IFACEMETHOD_(AudioProperties, AudioTrack)(_In_ uint32_t const& index);
        IFACEMETHOD_(int32_t, SelectedAudioTrack)();
        IFACEMETHOD_(bool, SelectAudioTrack)(_In_ int32_t const& index);

        IFACEMETHOD_(uint32_t, VideoTrackCount)();
        IFACEMETHOD_(VideoProperties, VideoTrack)(_In_ uint32_t const& index);
        IFACEMETHOD_(int32_t, SelectedVideoTrack)();
        IFACEMETHOD_(bool, SelectVideoTrack)(_In_ int32_t const& index);

        IFACEMETHOD_(ISharedTexture*, VideoTexture)();

        // IPlaybackEngine
        IFACEMETHOD_(int32_t, State)();
        IFACEMETHOD_(bool, CanPause)();
        IFACEMETHOD_(int64_t, Duration)();
        IFACEMETHOD_(bool, IsLooping)();
        IFACEMETHOD(SetLooping)(_In_ bool const&);
        IFACEMETHOD_(bool, CanSeek)();
        IFACEMETHOD(Seek)(_In_ int64_t const&);
        IFACEMETHOD_(int64_t, Position)();
        IFACEMETHOD_(double, PlaybackRate)();
        IFACEMETHOD(PlaybackRate)(double const&);

        IFACEMETHOD(Play)();
        IFACEMETHOD(Pause)();
        IFACEMETHOD(Stop)();

        IFACEMETHOD_(EventToken, StateChanged)(StateChangedCallback const& callback);
        IFACEMETHOD_(void, StateChanged)(EventToken const& token);

        // IPlaybackEnginePriv
        IFACEMETHOD(Initialize)(_In_ ID3D11Device*);

    private:
        HRESULT CreateMediaPlayer();
        void ReleaseMediaPlayer();
        HRESULT CreateResources(_In_ winrt::com_ptr<IDXGIDevice> const&);
        void ReleaseResources();
        void Callback(StateChangedArgs const& args);

    private:
        winrt::com_ptr<ID3D11Device> m_engineDevice;
        winrt::com_ptr<ID3D11Device> m_mediaDevice;

        uint32_t m_resetToken;
        winrt::com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;

        winrt::com_ptr<ISharedTexture> m_sharedTexture;

        winrt::Windows::Media::Playback::MediaPlaybackItem m_mediaPlaybackItem;

        winrt::Windows::Media::Playback::MediaPlayer m_mediaPlayer;
        winrt::Windows::Media::Playback::MediaPlayer::MediaOpened_revoker m_openedEventRevoker;
        winrt::Windows::Media::Playback::MediaPlayer::MediaEnded_revoker m_endedEventRevoker;
        winrt::Windows::Media::Playback::MediaPlayer::MediaFailed_revoker m_failedEventRevoker;
        winrt::Windows::Media::Playback::MediaPlayer::SourceChanged_revoker m_sourceChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlayer::VideoFrameAvailable_revoker m_videoFrameAvailableEventRevoker;

        winrt::Windows::Media::Playback::MediaPlaybackSession m_mediaPlaybackSession;
        winrt::Windows::Media::Playback::MediaPlaybackSession::BufferingEnded_revoker m_bufferingEndedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::BufferingProgressChanged_revoker m_bufferingProgressChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::DownloadProgressChanged_revoker m_downloadProgressChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::NaturalDurationChanged_revoker m_durationChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::NaturalVideoSizeChanged_revoker m_videoSizeChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::PlaybackRateChanged_revoker m_rateChangedRevoker;
        winrt::Windows::Media::Playback::MediaPlaybackSession::PlaybackStateChanged_revoker m_stateChangedRevoker;

        int64_t m_callbackIndex;
        std::map<int64_t, StateChangedCallback> m_statecallbacks;
    };
}
