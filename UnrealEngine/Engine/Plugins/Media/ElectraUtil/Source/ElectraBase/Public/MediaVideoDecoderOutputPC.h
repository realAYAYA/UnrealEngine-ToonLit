// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MediaVideoDecoderOutput.h"

#include "Templates/RefCounting.h"
#include "Containers/Array.h"
#include "Pixelformat.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition
# include <d3d11.h>
#pragma warning(pop)

THIRD_PARTY_INCLUDES_START
#include "mfobjects.h"
#include "mfapi.h"
THIRD_PARTY_INCLUDES_END

class FVideoDecoderOutputPC : public FVideoDecoderOutput
{
public:
	enum class EOutputType
	{
		Unknown = 0,
		SoftwareWin7,			// SW decode into buffer
		SoftwareWin8Plus,		// SW decode into DX11 texture
		HardwareWin8Plus,		// HW decode into shared DX11 texture
		HardwareDX9_DX12,		// HW decode into buffer
	};

	virtual EOutputType GetOutputType() const = 0;

	virtual TRefCountPtr<IMFSample> GetMFSample() const = 0;

	virtual const TArray<uint8> & GetBuffer() const = 0;
	virtual uint32 GetStride() const = 0;

	virtual TRefCountPtr<ID3D11Texture2D> GetTexture() const = 0;
	virtual TRefCountPtr<ID3D11Device> GetDevice() const = 0;
};

#include "Windows/HideWindowsPlatformTypes.h"

