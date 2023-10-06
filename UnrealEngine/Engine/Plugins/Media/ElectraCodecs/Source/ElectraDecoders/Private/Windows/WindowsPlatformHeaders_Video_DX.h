// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifdef ELECTRA_DECODERS_ENABLE_DX

#include "CoreMinimal.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"

THIRD_PARTY_INCLUDES_START
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"


struct FElectraVideoDecoderDXDeviceContext
{
	void Release()
	{
		DxDevice.SafeRelease();
		DxDeviceContext.SafeRelease();
	}
	void SetDeviceAndContext(void* InDxDevice, void* InDxDeviceContext)
	{
		DxDevice = reinterpret_cast<ID3D11Device*>(InDxDevice);
		DxDeviceContext = reinterpret_cast<ID3D11DeviceContext*>(InDxDeviceContext);
	}
	TRefCountPtr<ID3D11Device> DxDevice;
	TRefCountPtr<ID3D11DeviceContext> DxDeviceContext;
};

#endif
