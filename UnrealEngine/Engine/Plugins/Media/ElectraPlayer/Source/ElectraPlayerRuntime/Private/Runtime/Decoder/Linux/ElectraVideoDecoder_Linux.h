// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "PlayerCore.h"
#include "Linux/MediaVideoDecoderOutputLinux.h"
#include "Renderer/RendererVideo.h"

#include "libav_Decoder_Common_Video.h"

namespace Electra
{

class FElectraPlayerVideoDecoderOutputLinux : public FVideoDecoderOutputLinux
{
public:
	FElectraPlayerVideoDecoderOutputLinux();

	~FElectraPlayerVideoDecoderOutputLinux();

	void SetDecodedImage(TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> InDecodedImage);


	bool InitializeForBuffer(FIntPoint Dim, EPixelFormat PixFmt, FParamDict* InParamDict);
	TArray<uint8>& GetMutableBuffer();
	FIntPoint GetBufferDimensions() const override;

	const TArray<uint8>& GetBuffer() const override;
	uint32 GetStride() const override;
	FIntPoint GetDim() const override
	{ return SampleDim; }

	void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer) override;

	void ShutdownPoolable() override;

private:
	FIntPoint SampleDim;
	TArray<uint8> Buffer;
	uint32 Stride;

	TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> DecodedImage;

	// We hold a weak reference to the video renderer. During destruction the video renderer could be destroyed while samples are still out there..
	TWeakPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> OwningRenderer;
};


} // namespace Electra
