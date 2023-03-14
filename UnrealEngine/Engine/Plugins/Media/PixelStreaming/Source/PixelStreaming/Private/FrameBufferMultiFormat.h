// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturerMultiFormat.h"
#include "WebRTCIncludes.h"

namespace UE::PixelStreaming
{
	class FFrameBufferMultiFormat;

	/**
	 * Base class for our multi format buffers.
	 */
	class FFrameBufferMultiFormatBase : public webrtc::VideoFrameBuffer
	{
	public:
		FFrameBufferMultiFormatBase(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer);
		virtual ~FFrameBufferMultiFormatBase() = default;

		virtual webrtc::VideoFrameBuffer::Type type() const override { return webrtc::VideoFrameBuffer::Type::kNative; }

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

	protected:
		TSharedPtr<FPixelCaptureCapturerMultiFormat> FrameCapturer;
	};

	/**
	 * A multi layered, multi format frame buffer for our encoder.
	 */
	class FFrameBufferMultiFormatLayered : public FFrameBufferMultiFormatBase
	{
	public:
		FFrameBufferMultiFormatLayered(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer);
		virtual ~FFrameBufferMultiFormatLayered() = default;

		virtual int width() const override;
		virtual int height() const override;

		int GetNumLayers() const;
		rtc::scoped_refptr<FFrameBufferMultiFormat> GetLayer(int LayerIndex) const;
	};

	/**
	 * A single layer, multi format frame buffer.
	 */
	class FFrameBufferMultiFormat : public FFrameBufferMultiFormatBase
	{
	public:
		FFrameBufferMultiFormat(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer, int InLayerIndex);
		virtual ~FFrameBufferMultiFormat() = default;

		virtual int width() const override;
		virtual int height() const override;

		IPixelCaptureOutputFrame* RequestFormat(int32 Format) const;

	private:
		int32 LayerIndex;
		// we want the frame buffer to always refer to the same frame. so the first request for a format
		// will fill this cache.
		mutable TMap<int32, TSharedPtr<IPixelCaptureOutputFrame>> CachedFormat;
	};
} // namespace UE::PixelStreaming
