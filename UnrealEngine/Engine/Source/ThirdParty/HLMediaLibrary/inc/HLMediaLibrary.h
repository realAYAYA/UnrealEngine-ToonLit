// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <string>
#include <vector>
#include <future>

#include <combaseapi.h>
#include <d3d11.h>
#include <mfidl.h>

#ifdef HL_MEDIA_EXPORTS
#define HLM_API __declspec(dllexport)
#else
#define HLM_API __declspec(dllimport)
#endif

#if 0
enum class SphericalVideoFrameFormat : int32_t
{
    None = 0,
    Unsupported = 1,
    Equirectangular = 2,
};
#endif

namespace winrt
{
    namespace Windows
    {
        namespace Graphics
        {
            namespace DirectX
            {
                namespace Direct3D11
                {
                    struct IDirect3DSurface;
                }
            }
        }
    }
}

namespace HLMediaLibrary
{
    struct EventToken
    {
        int64_t value{};

        explicit operator bool() const noexcept
        {
            return value != 0;
        }
    };

    struct AudioProperties
    {
        uint32_t Bitrate;
        uint32_t BitsPerSample;
        uint32_t ChannelCount;
        bool IsSpatial;
        uint32_t SampleRate;
        std::wstring Subtype;
        std::wstring Type;
    };

    struct VideoProperties
    {
        uint32_t Bitrate;
        uint32_t Numerator;
        uint32_t Denominator;
        uint32_t AspectRatioNumerator;
        uint32_t AspectRatioDenominator;
        uint32_t Width;
        uint32_t Height;
        int32_t SphericalFormat;
        int32_t SteroPackingMode;
        std::wstring Subtype;
        std::wstring Type;
    };

    enum class FailedError : int32_t
    {
        Unknown    = 0,
        Aborted, 
        NetworkError,
        DecodingError,
        SourceNotSupported,
    };

    enum class PlaybackState : int32_t
    {
        None = 0,
        SourceChanged,
        Opening,
        Opened,
        Failed,
        Buffering,
        BufferingEnded,
        Downloading,
        Playing,
        Paused,
        ResolutionChanged,
        DurationChanged,
        RateChanged,
        Ended,
    };

    struct StateChangedArgs
    {
        PlaybackState State;
        union
        {
            union
            {
                FailedError Error;
                HRESULT Result;
            } Failed;
            union
            {
                double_t Value;
            } Progress;
            union
            {
                int64_t Value;
            } Duration;
            union
            {
                uint32_t Width;
                uint32_t Height;
            } Video;
        };
    };

    typedef std::function<void(StateChangedArgs const&)> StateChangedCallback;

    struct __declspec(uuid("55ef6bd8-0cb8-4233-8859-8e0401dff363")) ISharedTexture : ::IUnknown
    {
        STDMETHOD_(D3D11_TEXTURE2D_DESC, Texture2DDesc)() PURE;
        STDMETHOD_(ID3D11Texture2D*, Texture2D)() PURE;
        STDMETHOD_(ID3D11ShaderResourceView*, ShaderResourceView)() PURE;
        STDMETHOD_(ID3D11ShaderResourceView*, ShaderResourceViewUV)() PURE;
        STDMETHOD_(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface, MediaSurface)() PURE;
		STDMETHOD_(HANDLE, SharedTextureHandle)() PURE;
    };

    struct __declspec(uuid("ff43f65c-7d34-4dc1-8501-117431b65f34")) IPlaybackEngineItem : ::IUnknown
    {
        STDMETHOD(Load)(_In_ bool const& autoPlay, _In_ bool const& loopingEnabled, _In_ bool const& isAdaptiveStreaming, _In_ PCWSTR sourceUrl) PURE;
        STDMETHOD_(int64_t, StartTime)() PURE;

        STDMETHOD_(uint32_t, AudioTrackCount)() PURE;
        STDMETHOD_(AudioProperties, AudioTrack)(_In_ uint32_t const& index) PURE;
        STDMETHOD_(int32_t, SelectedAudioTrack)() PURE;
        STDMETHOD_(bool, SelectAudioTrack)(_In_ int32_t const& index) PURE;

        STDMETHOD_(uint32_t, VideoTrackCount)() PURE;
        STDMETHOD_(VideoProperties, VideoTrack)(_In_ uint32_t const& index) PURE;
        STDMETHOD_(int32_t, SelectedVideoTrack)() PURE;
        STDMETHOD_(bool, SelectVideoTrack)(_In_ int32_t const& index) PURE;

        STDMETHOD_(ISharedTexture*, VideoTexture)() PURE;
    };

    struct __declspec(uuid("c389bc33-df73-426f-ba8d-de56047e63de")) IPlaybackEngine : ::IUnknown
    {
        STDMETHOD_(int32_t, State)() PURE;
        STDMETHOD_(int64_t, Position)() PURE;
        STDMETHOD_(int64_t, Duration)() PURE;
        STDMETHOD_(bool, CanPause)() PURE;
        STDMETHOD_(bool, IsLooping)() PURE;
        STDMETHOD(SetLooping)(_In_ bool const& loopingEnabled) PURE;
        STDMETHOD_(bool, CanSeek)() PURE;
        STDMETHOD(Seek)(_In_ int64_t const& position) PURE;
        STDMETHOD_(double, PlaybackRate)() PURE;
        STDMETHOD(PlaybackRate)(double const& rate) PURE;
        STDMETHOD(Play)() PURE;
        STDMETHOD(Stop)() PURE;
        STDMETHOD(Pause)() PURE;
        STDMETHOD_(EventToken, StateChanged)(StateChangedCallback const& callback) PURE;
        STDMETHOD_(void, StateChanged)(EventToken const& token) PURE;
    };

    HRESULT HLM_API CreatePlaybackEngine(
        _In_ ID3D11Device* device, 
        _COM_Outptr_ IPlaybackEngine** playbackEngine);

    HRESULT HLM_API ValidateSourceUrl(
        _In_ bool isManifest,
        _In_ PCWSTR sourceUrl);
}
