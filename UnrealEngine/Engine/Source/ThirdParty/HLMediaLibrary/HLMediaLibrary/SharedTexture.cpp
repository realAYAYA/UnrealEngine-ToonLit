// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "SharedTexture.h"
#include "Transform.h"
#include "MediaHelpers.h"

#include <mferror.h>
#include <winrt/windows.foundation.metadata.h>
#include <winrt/windows.media.playback.h>
#include <winrt/windows.perception.spatial.preview.h>

using namespace winrt;
using namespace HLMediaLibrary;
using namespace Windows::Foundation;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Perception;
using namespace Windows::Perception::Spatial;
using namespace Windows::Perception::Spatial::Preview;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Metadata;
using namespace Windows::Foundation::Numerics;

HRESULT SharedTexture::Create(
    com_ptr<ID3D11Device> const d3dDevice,
    com_ptr<IMFDXGIDeviceManager> const dxgiDeviceManager,
    uint32_t width, uint32_t height, DXGI_FORMAT format,
    com_ptr<ISharedTexture>& sharedTexture)
{
    NULL_CHK_HR(d3dDevice, E_INVALIDARG);
    NULL_CHK_HR(dxgiDeviceManager, E_INVALIDARG);
    if (width < 1 && height < 1)
    {
        IFR(E_INVALIDARG);
    }

    // only support these 2 formats
    // NV12 requires special handling given the layout of the buffer
    if (format != DXGI_FORMAT_B8G8R8A8_UNORM && format != DXGI_FORMAT_NV12)
    {
        IFR(E_INVALIDARG);
    }

    HANDLE deviceHandle;
    IFR(dxgiDeviceManager->OpenDeviceHandle(&deviceHandle));

    com_ptr<ID3D11Device1> mediaDevice = nullptr;
    IFR(dxgiDeviceManager->LockDevice(deviceHandle, guid_of<ID3D11Device1>(), mediaDevice.put_void(), TRUE));

    // since the device is locked, unlock before we exit function
    HRESULT hr = S_OK;

    //DXGI_FORMAT_B8G8R8A8_UNORM / DXGI_FORMAT_NV12
    D3D11_TEXTURE2D_DESC textureDesc = CD3D11_TEXTURE2D_DESC(format, width, height);
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.MipLevels = 1;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.CPUAccessFlags = 0;

    // create a texture
    com_ptr<ID3D11Texture2D> spTexture = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    D3D11_SHADER_RESOURCE_VIEW_DESC srvUVDesc{};
    com_ptr<ID3D11ShaderResourceView> spSRV = nullptr;
    com_ptr<ID3D11ShaderResourceView> spSRVUV = nullptr;
    com_ptr<IDXGIResource1> spDXGIResource = nullptr;
    com_ptr<ID3D11Texture2D> spMediaTexture = nullptr;
    HANDLE sharedHandle = INVALID_HANDLE_VALUE;

    IDirect3DSurface mediaSurface;
    com_ptr<IMFMediaBuffer> dxgiMediaBuffer = nullptr;
    com_ptr<IMFSample> mediaSample = nullptr;

    auto videoTexture = make<SharedTexture>().as<ISharedTexturePriv>();

    IFG(d3dDevice->CreateTexture2D(&textureDesc, nullptr, spTexture.put()), done);

    // srv for the texture
    if (textureDesc.Format == DXGI_FORMAT_NV12)
    {
        //DXGI_FORMAT_R8_UNORM
        //DXGI_FORMAT_R8G8_UNORM
        srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.get(), D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
        srvDesc.Format = DXGI_FORMAT_R8_UNORM;

        srvUVDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.get(), D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
        srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        IFG(d3dDevice->CreateShaderResourceView(spTexture.get(), &srvUVDesc, spSRVUV.put()), done);
    }
    else if (textureDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.get(), D3D11_SRV_DIMENSION_TEXTURE2D);
    }

    IFG(d3dDevice->CreateShaderResourceView(spTexture.get(), &srvDesc, spSRV.put()), done);

    IFG(spTexture->QueryInterface(guid_of<IDXGIResource1>(), spDXGIResource.put_void()), done);

    // create shared texture 
    IFG(spDXGIResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &sharedHandle), done);

    IFG(mediaDevice->OpenSharedResource1(sharedHandle, guid_of<ID3D11Texture2D>(), spMediaTexture.put_void()), done);

    IFG(GetSurfaceFromTexture(spMediaTexture.get(), mediaSurface), done);

    // create a media buffer for the texture
    IFG(MFCreateDXGISurfaceBuffer(guid_of<ID3D11Texture2D>(), spMediaTexture.get(), 0, /*fBottomUpWhenLinear*/false, dxgiMediaBuffer.put()), done);

    // create a sample with the buffer
    IFG(MFCreateSample(mediaSample.put()), done);

    IFG(mediaSample->AddBuffer(dxgiMediaBuffer.get()), done);

    videoTexture->Texture2DDesc(textureDesc);
    videoTexture->Texture2D(spTexture.get());
    videoTexture->ShaderResourceView(spSRV.get());
    if (textureDesc.Format == DXGI_FORMAT_NV12)
    {
        videoTexture->ShaderResourceViewUV(spSRVUV.get());
    }
    videoTexture->SharedTextureHandle(sharedHandle);
    videoTexture->MediaTexture(spMediaTexture.get());
    videoTexture->MediaBuffer(dxgiMediaBuffer.get());
    videoTexture->MediaSample(mediaSample.get());
    videoTexture->MediaSurface(mediaSurface);

    sharedTexture.attach(videoTexture.as<ISharedTexture>().detach());

done:
    if (FAILED(hr))
    {
        if (sharedHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(sharedHandle);
        }
    }

    dxgiDeviceManager->UnlockDevice(deviceHandle, FALSE);

    return hr;
}

SharedTexture::SharedTexture()
    : m_frameTextureDesc{}
    , m_frameTexture(nullptr)
    , m_frameTextureSRV(nullptr)
    , m_frameTextureSRVUV(nullptr)
    , m_sharedTextureHandle(INVALID_HANDLE_VALUE)
    , m_mediaTexture(nullptr)
    , m_mediaBuffer(nullptr)
    , m_mediaSample(nullptr)
    , m_mediaSurface(nullptr)
    , m_transforms(nullptr)
{}

SharedTexture::~SharedTexture()
{
    Close();
}

void SharedTexture::Close()
{
    // primary texture
    if (m_sharedTextureHandle != INVALID_HANDLE_VALUE)
    {
        if (CloseHandle(m_sharedTextureHandle))
        {
            m_sharedTextureHandle = INVALID_HANDLE_VALUE;
        }
    }

    if (m_transforms != nullptr)
    {
        m_transforms->Reset();

        m_transforms = nullptr;
    }

    m_mediaSample = nullptr;
    m_mediaSurface = nullptr;
    m_mediaBuffer = nullptr;
    m_mediaTexture = nullptr;
    m_frameTextureSRVUV = nullptr;
    m_frameTextureSRV = nullptr;
    m_frameTexture = nullptr;

    ZeroMemory(&m_frameTextureDesc, sizeof(CD3D11_TEXTURE2D_DESC));
}

hresult SharedTexture::UpdateTransforms(
    SpatialCoordinateSystem const& appCoordinateSystem)
{
    NULL_CHK_HR(appCoordinateSystem, E_INVALIDARG);

    if (m_transforms == nullptr)
    {
        IFR(Transform::Create(m_mediaSample, m_transforms.put()));
    }

    return m_transforms->Update(appCoordinateSystem);
}

D3D11_TEXTURE2D_DESC SharedTexture::Texture2DDesc()
{ 
    return m_frameTextureDesc; 
}

void SharedTexture::Texture2DDesc(D3D11_TEXTURE2D_DESC textureDesc)
{ 
    m_frameTextureDesc = textureDesc; 
}

ID3D11Texture2D* SharedTexture::Texture2D()
{ 
    return m_frameTexture.get(); 
}

HRESULT SharedTexture::Texture2D(ID3D11Texture2D* pTexture)
{
    HRESULT hr = S_OK;

    if (pTexture == nullptr)
    {
        m_frameTexture = nullptr;
    }
    else
    {
        hr = pTexture->QueryInterface(guid_of<ID3D11Texture2D>(), m_frameTexture.put_void());
    }

    return hr;
}

ID3D11ShaderResourceView* SharedTexture::ShaderResourceView()
{ 
    return m_frameTextureSRV.get(); 
}

HRESULT SharedTexture::ShaderResourceView(ID3D11ShaderResourceView* pSrv)
{ 
    HRESULT hr = S_OK;

    if (pSrv == nullptr)
    {
        m_frameTextureSRV = nullptr;
    }
    else
    {
        hr = pSrv->QueryInterface(guid_of<ID3D11ShaderResourceView>(), m_frameTextureSRV.put_void());
    }

    return hr;
}

ID3D11ShaderResourceView* SharedTexture::ShaderResourceViewUV()
{
    return m_frameTextureSRVUV.get();
}

HRESULT SharedTexture::ShaderResourceViewUV(ID3D11ShaderResourceView* pSrv)
{
    HRESULT hr = S_OK;

    if (pSrv == nullptr)
    {
        m_frameTextureSRVUV = nullptr;
    }
    else
    {
        hr = pSrv->QueryInterface(guid_of<ID3D11ShaderResourceView>(), m_frameTextureSRVUV.put_void());
    }

    return hr;
}

HANDLE SharedTexture::SharedTextureHandle() 
{ 
    return m_sharedTextureHandle;
}

void SharedTexture::SharedTextureHandle(HANDLE handle)
{ 
    m_sharedTextureHandle = handle;
}

ID3D11Texture2D* SharedTexture::MediaTexture()
{ 
    return m_mediaTexture.get();
}

HRESULT SharedTexture::MediaTexture(ID3D11Texture2D* pTexture)
{
    HRESULT hr = S_OK;

    if (pTexture == nullptr)
    {
        m_mediaTexture = nullptr;
    }
    else
    {
        hr = pTexture->QueryInterface(guid_of<ID3D11Texture2D>(), m_mediaTexture.put_void());
    }

    return hr;
}

IMFMediaBuffer* SharedTexture::MediaBuffer()
{ 
    return m_mediaBuffer.get(); 
}

HRESULT SharedTexture::MediaBuffer(IMFMediaBuffer* pMediaBuffer)
{ 
    HRESULT hr = S_OK;

    if (pMediaBuffer == nullptr)
    {
        m_mediaBuffer = nullptr;
    }
    else
    {
        hr = pMediaBuffer->QueryInterface(guid_of<IMFMediaBuffer>(), m_mediaBuffer.put_void());
    }

    return hr;
}

IMFSample* SharedTexture::MediaSample()
{ 
    return m_mediaSample.get(); 
}

HRESULT SharedTexture::MediaSample(IMFSample* pSample)
{
    HRESULT hr = S_OK;

    if (pSample == nullptr)
    {
        m_mediaSample = nullptr;
    }
    else
    {
        hr = pSample->QueryInterface(guid_of<IMFMediaBuffer>(), m_mediaSample.put_void());
    }

    return hr;
}

Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface SharedTexture::MediaSurface()
{ 
    return m_mediaSurface; 
}

void SharedTexture::MediaSurface(Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface)
{ 
    m_mediaSurface = surface; 
}
