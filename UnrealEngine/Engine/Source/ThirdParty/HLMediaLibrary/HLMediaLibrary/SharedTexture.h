// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "HLMediaLibrary.h"

#include <mfidl.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.perception.spatial.preview.h>
#include "Transform.h"

namespace HLMediaLibrary
{
    struct __declspec(uuid("cce65a52-8db2-4a9a-90d1-d5d883d01fef")) ISharedTexturePriv : ::IUnknown
    {
        STDMETHOD_(void, Texture2DDesc)(
            _In_ D3D11_TEXTURE2D_DESC) PURE;
        STDMETHOD(Texture2D)(
            _In_ ID3D11Texture2D*) PURE;
        STDMETHOD(ShaderResourceView)(
            _In_ ID3D11ShaderResourceView*) PURE;
        STDMETHOD(ShaderResourceViewUV)(
            _In_ ID3D11ShaderResourceView* srv) PURE;
        STDMETHOD_(HANDLE, SharedTextureHandle)() PURE;
        STDMETHOD_(void, SharedTextureHandle)(
            HANDLE) PURE;
        STDMETHOD_(ID3D11Texture2D*, MediaTexture)() PURE;
        STDMETHOD(MediaTexture)(
            _In_ ID3D11Texture2D*) PURE;
        STDMETHOD_(IMFMediaBuffer*, MediaBuffer)() PURE;
        STDMETHOD(MediaBuffer)(
            _In_ IMFMediaBuffer*) PURE;
        STDMETHOD_(IMFSample*, MediaSample)() PURE;
        STDMETHOD(MediaSample)(
            _In_ IMFSample*) PURE;
        STDMETHOD_(void, MediaSurface)(
            _In_ winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const&) PURE;
    };

    struct SharedTexture : winrt::implements<SharedTexture, ISharedTexture, ISharedTexturePriv>
    {
        static HRESULT Create(
            _In_ winrt::com_ptr<ID3D11Device> const d3dDevice,
            _In_ winrt::com_ptr<IMFDXGIDeviceManager> const dxgiDeviceManager,
            _In_ uint32_t width,
            _In_ uint32_t height,
            _In_ DXGI_FORMAT format,
            _Out_ winrt::com_ptr<ISharedTexture>& sharedTexture);

        SharedTexture();
        virtual ~SharedTexture();

        // IMediaSharedTexture
        IFACEMETHOD_(D3D11_TEXTURE2D_DESC, Texture2DDesc)();
        IFACEMETHOD_(ID3D11Texture2D*, Texture2D)();
        IFACEMETHOD_(ID3D11ShaderResourceView*, ShaderResourceView)();
        IFACEMETHOD_(ID3D11ShaderResourceView*, ShaderResourceViewUV)();
        IFACEMETHOD_(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface, MediaSurface)();

        // IMediaSharedTexturePriv
        IFACEMETHOD_(void, Texture2DDesc)(D3D11_TEXTURE2D_DESC textureDesc);
        IFACEMETHOD(Texture2D)(ID3D11Texture2D* pTexture);
        IFACEMETHOD(ShaderResourceView)(ID3D11ShaderResourceView* pSrv);
        IFACEMETHOD(ShaderResourceViewUV)(ID3D11ShaderResourceView* pSrv);
        IFACEMETHOD_(HANDLE, SharedTextureHandle)();
        IFACEMETHOD_(void, SharedTextureHandle)(HANDLE handle);
        IFACEMETHOD_(ID3D11Texture2D*, MediaTexture)();
        IFACEMETHOD(MediaTexture)(ID3D11Texture2D* pTexture);
        IFACEMETHOD_(IMFMediaBuffer*, MediaBuffer)();
        IFACEMETHOD(MediaBuffer)(IMFMediaBuffer* pMediaBuffer);
        IFACEMETHOD_(IMFSample*, MediaSample)();
        IFACEMETHOD(MediaSample)(IMFSample* pSample);
        IFACEMETHOD_(void, MediaSurface)(winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface);

        winrt::hresult UpdateTransforms(
            _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

    private:
        void Close();

    public:
        D3D11_TEXTURE2D_DESC                        m_frameTextureDesc;
        winrt::com_ptr<ID3D11Texture2D>             m_frameTexture;
        winrt::com_ptr<ID3D11ShaderResourceView>    m_frameTextureSRV;
        winrt::com_ptr<ID3D11ShaderResourceView>    m_frameTextureSRVUV;
        HANDLE                                      m_sharedTextureHandle;
        winrt::com_ptr<ID3D11Texture2D>             m_mediaTexture;
        winrt::com_ptr<IMFMediaBuffer>              m_mediaBuffer;
        winrt::com_ptr<IMFSample>                   m_mediaSample;

        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface m_mediaSurface;
        winrt::com_ptr<ITransformPriv>              m_transforms;
    };
}
