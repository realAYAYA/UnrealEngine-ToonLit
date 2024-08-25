// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Missing defines around platform specific includes

#include "MediaVideoDecoderOutput.h"

#include "Templates/RefCounting.h"
#include "Containers/Array.h"
#include "PixelFormat.h"

#if PLATFORM_WINDOWS

	#include "Windows/AllowWindowsPlatformTypes.h"

	#pragma warning(push)
	#pragma warning(disable : 4005) // macro redefinition
	#include <d3d11.h>
	#pragma warning(pop)
	#include <d3d12.h>

THIRD_PARTY_INCLUDES_START
	#include "mfobjects.h"
	#include "mfapi.h"
THIRD_PARTY_INCLUDES_END

#endif // PLATFORM_WINDOWS

class FVideoDecoderOutputPC : public FVideoDecoderOutput
{
public:
	enum class EOutputType
	{
		Unknown = 0,
		SoftwareWin7,	  // SW decode into CPU buffer
		SoftwareWin8Plus, // SW decode into DX11 texture
		HardwareWin8Plus, // HW decode into shared DX11 texture
		HardwareDX9_DX12, // HW decode into CPU buffer
		Hardware_DX,	  // HW decode into DX texture on render device (DX11 or DX12)
	};

	virtual EOutputType GetOutputType() const = 0;
	virtual const TArray<uint8>& GetBuffer() const = 0;
	virtual uint32 GetStride() const = 0;

#if PLATFORM_WINDOWS
	virtual TRefCountPtr<IUnknown> GetTexture() const = 0;
	virtual TRefCountPtr<IUnknown> GetSync(uint64& SyncValue) const = 0;
	virtual TRefCountPtr<ID3D11Device> GetDevice() const = 0;
#endif // PLATFORM_WINDOWS
};

#if PLATFORM_WINDOWS
	#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS
