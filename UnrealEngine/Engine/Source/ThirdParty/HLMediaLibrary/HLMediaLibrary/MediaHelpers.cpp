// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "pch.h"
#include "MediaHelpers.h"

#include <winrt/windows.media.playback.h>
#include <windows.graphics.directx.direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>


using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Media::Playback;

// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
    HRESULT hr = E_NOTIMPL;

#if defined(_DEBUG)
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
        0,
        D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
        nullptr,                    // Any feature level will do.
        0,
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        nullptr,                    // No need to keep the D3D device reference.
        nullptr,                    // No need to know the feature level.
        nullptr                     // No need to keep the D3D device context reference.
    );
#endif

    return SUCCEEDED(hr);
}

HRESULT HLMediaLibrary::CreateMediaDevice(
    _In_opt_ IDXGIAdapter* pDXGIAdapter,
    _COM_Outptr_ ID3D11Device** ppDevice)
{
    if (pDXGIAdapter == nullptr || ppDevice == nullptr)
    {
        return E_INVALIDARG;
    }

    (*ppDevice) = nullptr;
    
    // Create the Direct3D 11 API device object and a corresponding context.
    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    if (SdkLayersAvailable())
    {
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    com_ptr<ID3D11Device> spDevice;
    com_ptr<ID3D11DeviceContext> spContext;

    D3D_DRIVER_TYPE driverType = (nullptr != pDXGIAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    // Create a device using the hardware graphics driver if adapter is not supplied
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        pDXGIAdapter,               // if nullptr will use default adapter.
        driverType, 
        0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
        creationFlags,              // Set debug and Direct2D compatibility flags.
        featureLevels,              // List of feature levels this app can support.
        ARRAYSIZE(featureLevels),   // Size of the list above.
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        spDevice.put(),             // Returns the Direct3D device created.
        &featureLevel,              // Returns feature level of device created.
        spContext.put()             // Returns the device immediate context.
    );

    if (FAILED(hr))
    {
        // fallback to WARP if we are not specifying an adapter
        if (nullptr == pDXGIAdapter)
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see: 
            // http://go.microsoft.com/fwlink/?LinkId=286690
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                0,
                creationFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                spDevice.put(),
                &featureLevel,
                spContext.put());
        }

        IFR(hr);
    }
    else
    {
        // workaround for nvidia GPU's, cast to ID3D11VideoDevice
        auto videoDevice = spDevice.as<ID3D11VideoDevice>();
    }

    // Turn multithreading on 
    auto spMultithread = spContext.as<ID3D10Multithread>();
    NULL_CHK_HR(spMultithread, E_POINTER);

    spMultithread->SetMultithreadProtected(TRUE);

    *ppDevice = spDevice.detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT HLMediaLibrary::GetSurfaceFromTexture(
    ID3D11Texture2D* pTexture,
    IDirect3DSurface& direct3dSurface)
{
    com_ptr<IDXGISurface> dxgiSurface = nullptr;
    IFR(pTexture->QueryInterface(
        guid_of<IDXGISurface>(), 
        dxgiSurface.put_void()));
    
    com_ptr<::IInspectable> spInspectable = nullptr;
    IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), spInspectable.put()));

    IFR(spInspectable->QueryInterface(
        guid_of<IDirect3DSurface>(),
        reinterpret_cast<void**>(put_abi(direct3dSurface))));

    return S_OK;
}
