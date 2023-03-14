// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VideoEncoderInput.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"

class FRHITexture;

namespace UE::PixelStreaming
{
	/*
	* Factory to create `AVEncoder::FVideoEncoderInputFrame` for use in Pixel Streaming encoders.
	* This class is responsible for creating/managing the underlying `AVEncoder::FVideoEncoderInput` required to create
	* `AVEncoder::FVideoEncoderInputFrame`.
	* This class is not threadsafe and should only be accessed from a single thread.
	*/
	class FEncoderFrameFactory
	{
	public:
		FEncoderFrameFactory();
		~FEncoderFrameFactory();
		TSharedPtr<AVEncoder::FVideoEncoderInputFrame> GetFrameAndSetTexture(TRefCountPtr<FRHITexture> InTexture);
		TSharedPtr<AVEncoder::FVideoEncoderInput> GetOrCreateVideoEncoderInput();

	private:
		TSharedPtr<AVEncoder::FVideoEncoderInputFrame> GetOrCreateFrame(const TRefCountPtr<FRHITexture> InTexture);
		void RemoveStaleTextures();
		void FlushFrames();
		TSharedPtr<AVEncoder::FVideoEncoderInput> CreateVideoEncoderInput() const;
		void SetTexture(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);
		void SetTextureCUDAVulkan(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);
#if PLATFORM_WINDOWS
		void SetTextureCUDAD3D11(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);
		void SetTextureCUDAD3D12(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);
#endif // PLATFORM_WINDOWS

	private:
		uint64 FrameId = 0;
		TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput;
		// Store a mapping between raw textures and the FVideoEncoderInputFrames that wrap them
		TMap<TRefCountPtr<FRHITexture>, TSharedPtr<AVEncoder::FVideoEncoderInputFrame>> TextureToFrameMapping;
	};
} // namespace UE::PixelStreaming
