// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IElectraDecoderResourceDelegateBase.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include "d3d12.h"
#include "mfobjects.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

enum class EElectraDecoderPlatformOutputHandleType
{
	MFSample,
	DXDevice,
	DXDeviceContext,
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

class FElectraDecoderOutputSync final
{
public:
	FElectraDecoderOutputSync() : SyncValue(0), CopyDoneSyncValue(0) {}
	FElectraDecoderOutputSync(TRefCountPtr<IUnknown> InSync, uint64 InSyncValue, TRefCountPtr<IMFSample> InMFSample = nullptr, TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync> InTaskSync = nullptr)
		: Sync(InSync), SyncValue(InSyncValue), TaskSync(InTaskSync), MFSample(InMFSample)
	{}
	FElectraDecoderOutputSync(TRefCountPtr<IUnknown> InSync, uint64 InSyncValue, TRefCountPtr<ID3D12Fence> InCopyDoneSync, uint64 InCopyDoneSyncValue)
		: Sync(InSync), SyncValue(InSyncValue), CopyDoneSync(InCopyDoneSync), CopyDoneSyncValue(InCopyDoneSyncValue)
	{}

	TRefCountPtr<IUnknown>		Sync;														// Optional IUnknown based sync primitive (fence, or DX12 specific WMF object)
	uint64						SyncValue;													// Optionally needed sync value
	TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync> TaskSync;	// Optional Sync/Perquisite object to use to run code async after the decoder data arrives

	TRefCountPtr<ID3D12Fence>	CopyDoneSync;												// Optional fence to signal end of copy from this buffer back to decoder
	uint64						CopyDoneSyncValue;											// Value to be used with CopyDoneSync fence

private:
	TRefCountPtr<IMFSample>		MFSample;													// The sample reference can ride along to ensure that the DX12 WMF interface properly keeps reference to the sample until we enqueue their sync primitives with the a D3D queue
};

class IElectraDecoderVideoOutputImageBuffers
{
public:
	// Return the 4cc of the codec.
	virtual uint32 GetCodec4CC() const = 0;

	// Returns the number of separate image buffers making up the frame.
	virtual int32 GetNumberOfBuffers() const = 0;

	// Returns the n'th image buffer as CPU data buffer.
	virtual TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetBufferDataByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer as GPU texture resource.
	virtual void* GetBufferTextureByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer's GPU sync object to signal texture data as being ready to be read
	virtual bool GetBufferTextureSyncByIndex(int32 InBufferIndex, FElectraDecoderOutputSync& SyncObject) const = 0;

	// Returns the n'th image buffer format
	virtual EElectraDecoderPlatformPixelFormat GetBufferFormatByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer encoding
	virtual EElectraDecoderPlatformPixelEncoding GetBufferEncodingByIndex(int32 InBufferIndex) const = 0;

	// Returns the n'th image buffer pitch
	virtual int32 GetBufferPitchByIndex(int32 InBufferIndex) const = 0;
};

#define ELECTRA_HAVE_IMAGEBUFFERS 1
