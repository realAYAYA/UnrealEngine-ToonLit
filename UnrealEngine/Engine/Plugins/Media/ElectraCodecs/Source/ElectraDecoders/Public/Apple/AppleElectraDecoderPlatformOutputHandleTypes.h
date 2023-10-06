// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EElectraDecoderPlatformOutputHandleType
{
	ImageBuffers			// IElectraDecoderVideoOutputImageBuffers interface
};

enum class EElectraDecoderPlatformPixelFormat
{
	INVALID = 0,
	R8G8B8A8,
	A8R8G8B8,
	B8G8R8A8,
	R16G16B16A16,
	A16B16G16R16,
	A32B32G32R32F,
	A2B10G10R10,
	DXT1,
	DXT5,
	BC4,
	NV12,
	P010,
};

enum class EElectraDecoderPlatformPixelEncoding
{
	Native = 0,		//!< Pixel formats native representation
	RGB,			//!< Interpret as RGB
	RGBA,			//!< Interpret as RGBA
	YCbCr,			//!< Interpret as YCbCR
	YCbCr_Alpha,	//!< Interpret as YCbCR with alpha
	YCoCg,			//!< Interpret as scaled YCoCg
	YCoCg_Alpha,	//!< Interpret as scaled YCoCg with trailing BC4 alpha data
	CbY0CrY1,		//!< Interpret as CbY0CrY1
	Y0CbY1Cr,		//!< Interpret as Y0CbY1Cr
	ARGB_BigEndian,	//!< Interpret as ARGB, big endian
};

class IElectraDecoderVideoOutputImageBuffers
{
public:
	// Return the 4cc of the codec. This determines how the rest of the information must be interpreted.
	virtual uint32 GetCodec4CC() const = 0;

	// Returns the number of separate image buffers making up the frame.
	virtual int32 GetNumberOfBuffers() const = 0;

	// Returns the n'th image buffer data address.
	virtual TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetBufferDataByIndex(int32 InBufferIndex) const = 0;
	
	// Returns the n'th CFImageBuffer ("texture") reference.
	virtual void* GetBufferTextureByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer format
	virtual EElectraDecoderPlatformPixelFormat GetBufferFormatByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer encoding
	virtual EElectraDecoderPlatformPixelEncoding GetBufferEncodingByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer pitch
	virtual int32 GetBufferPitchByIndex(int32 InBufferIndex) const = 0;
};
