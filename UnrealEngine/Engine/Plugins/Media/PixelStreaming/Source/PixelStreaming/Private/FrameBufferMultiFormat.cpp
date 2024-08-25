// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameBufferMultiFormat.h"

namespace UE::PixelStreaming
{
	FFrameBufferMultiFormatBase::FFrameBufferMultiFormatBase(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer, uint32 InStreamId)
		: FrameCapturer(InFrameCapturer)
		, StreamId(InStreamId)
	{
	}

	FFrameBufferMultiFormatLayered::FFrameBufferMultiFormatLayered(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer, uint32 InStreamId)
		: FFrameBufferMultiFormatBase(InFrameCapturer, InStreamId)
	{
	}

	int FFrameBufferMultiFormatLayered::width() const
	{
		return FrameCapturer ? FrameCapturer->GetWidth(GetNumLayers() - 1) : -1;
	}

	int FFrameBufferMultiFormatLayered::height() const
	{
		return FrameCapturer ? FrameCapturer->GetHeight(GetNumLayers() - 1) : -1;
	}

	int FFrameBufferMultiFormatLayered::GetNumLayers() const
	{
		return FrameCapturer ? FrameCapturer->GetNumLayers() : -1;
	}

	rtc::scoped_refptr<FFrameBufferMultiFormat> FFrameBufferMultiFormatLayered::GetLayer(int LayerIndex) const
	{
#if WEBRTC_5414
		return rtc::make_ref_counted<FFrameBufferMultiFormat>(FrameCapturer, StreamId, LayerIndex);
#else
		return new rtc::RefCountedObject<FFrameBufferMultiFormat>(FrameCapturer, StreamId, LayerIndex);
#endif
	}

	FFrameBufferMultiFormat::FFrameBufferMultiFormat(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer, uint32 InStreamId, int InLayerIndex)
		: FFrameBufferMultiFormatBase(InFrameCapturer, InStreamId)
		, LayerIndex(InLayerIndex)
	{
	}

	int FFrameBufferMultiFormat::width() const
	{
		return FrameCapturer ? FrameCapturer->GetWidth(LayerIndex) : -1;
	}

	int FFrameBufferMultiFormat::height() const
	{
		return FrameCapturer ? FrameCapturer->GetHeight(LayerIndex) : -1;
	}

	IPixelCaptureOutputFrame* FFrameBufferMultiFormat::RequestFormat(int32 Format) const
	{
		// ensure this frame buffer will always refer to the same frame
		if (TSharedPtr<IPixelCaptureOutputFrame>* CachedFrame = CachedFormat.Find(Format))
		{
			return CachedFrame->Get();
		}

		if(!FrameCapturer)
		{
			return nullptr;
		}

		TSharedPtr<IPixelCaptureOutputFrame> Frame = FrameCapturer->WaitForFormat(Format, LayerIndex);
		CachedFormat.Add(Format, Frame);
		return Frame.Get();
	}
} // namespace UE::PixelStreaming
