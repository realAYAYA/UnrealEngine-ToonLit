// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCIncludes.h"

namespace UE::PixelStreaming
{
	/**
	 * A frame buffer for our NVENC encoders (h264). Contains a single RHI texture.
	 */
	class FFrameBufferRHI : public webrtc::VideoFrameBuffer
	{
	public:
		FFrameBufferRHI(FTexture2DRHIRef InFrameTexture)
		:FrameTexture(InFrameTexture)
		{
		}

		virtual ~FFrameBufferRHI() = default;

		virtual webrtc::VideoFrameBuffer::Type type() const override { return webrtc::VideoFrameBuffer::Type::kNative; }
		virtual int width() const override { return FrameTexture->GetDesc().Extent.X; }
		virtual int height() const override { return FrameTexture->GetDesc().Extent.Y; }

		virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
		{
			unimplemented();
			return nullptr;
		}

		virtual const webrtc::I420BufferInterface* GetI420() const override
		{
			unimplemented();
			return nullptr;
		}

		FTexture2DRHIRef GetFrameTexture() const { return FrameTexture; }

	private:
		FTexture2DRHIRef FrameTexture;
	};
} // namespace UE::PixelStreaming
