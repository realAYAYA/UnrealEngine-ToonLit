// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaVideoDecoderOutputPC.h"
#include "RHI.h"

#if PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_START
#include "d3d12.h"
THIRD_PARTY_INCLUDES_END

#endif // PLATFORM_WINDOWS

class TextureMediaPlayerVideoDecoderOutput : public FVideoDecoderOutputPC
{
public:
	void Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, const TArray<uint8>& InBuffer, const FIntPoint& InSampleDim);

#if PLATFORM_WINDOWS
	void Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, FTexture2DRHIRef InTexture, const FIntPoint& InSampleDim, TRefCountPtr<ID3D12Fence> InFence, uint64 InFenceValue);
#else
	void Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, FTexture2DRHIRef InTexture, const FIntPoint& InSampleDim, FGPUFenceRHIRef InFence, uint64 InFenceValue);
#endif // PLATFORM_WINDOWS

	// FVideoDecoderOutputPC interface.
	virtual void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& Renderer) override;

	virtual EOutputType GetOutputType() const override;

	virtual const TArray<uint8>& GetBuffer() const override;
	virtual uint32 GetStride() const override;
	virtual FIntPoint GetDim() const;

#if PLATFORM_WINDOWS
	virtual TRefCountPtr<IUnknown> GetTexture() const override;
	virtual TRefCountPtr<IUnknown> GetSync(uint64& SyncValue) const override;
	virtual TRefCountPtr<ID3D11Device> GetDevice() const override;
#endif // PLATFORM_WINDOWS

private:
	/** Texture from WebRTC with accompaning sync data. */
	FTexture2DRHIRef Texture;
	
#if PLATFORM_WINDOWS
	TRefCountPtr<ID3D12Fence> Fence;
#else
	FGPUFenceRHIRef Fence;
#endif // PLATFORM_WINDOWS

	uint64 FenceValue;

	/** Dimension of any internally allocated buffer - stored explicitly to cover various special cases for DX. */
	FIntPoint SampleDim;

	/** Not used but I need to return a buffer in GetBuffer. */
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ByteBuffer;
};
