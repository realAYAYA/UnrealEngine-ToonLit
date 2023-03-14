// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <d3d11_1.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#pragma comment(lib, "mfuuid")

#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.media.devices.h>
#include <winrt/windows.perception.spatial.h>

namespace HLMediaLibrary
{
    HRESULT CreateMediaDevice(
        _In_opt_ IDXGIAdapter* pDXGIAdapter,
        _COM_Outptr_ ID3D11Device** ppDevice);

    HRESULT GetSurfaceFromTexture(
        _In_ ID3D11Texture2D* pTexture,
        _Out_ winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& ppSurface);
}
